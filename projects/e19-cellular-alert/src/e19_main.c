/* e19_main.c: project e19, the event driven cellular alert composer. One C file owns
 * the loop, the prompt and the send path, and it is meant to be read top to bottom in
 * one sitting, because the review item of this chapter is a claim about this file:
 * there is exactly one call to sms_send(), exactly one call to alert_queue_drain(), and
 * exactly one call to alert_queue_push(), and all three sit inside the branch the
 * rule's index opened.
 *
 *   edgellm-e19 --project e19 (--replay <csv> | --live iio <dir>[:<interval_s>])
 *               --grammar <gbnf> --prompt <txt> --log <csv> [--alerts <csv>]
 *               [--model <gguf>] [--state <path>] [--modem <dev>:<baud>]
 *               [--sms-to <number>] [--n-predict N] [--threads N] [--n-ctx N]
 *               [--temperature T] [--interval S] [--max-events N]
 *               [--template chatml|zephyr|none] [--stub]
 *
 * Why this project has its own loop. The shared main in common/c/src/edgellm_main.c
 * hands a project two hooks: one before the model is asked, project_actuate, and one to
 * build the user text, project_format_prompt. Neither of them is ever given the model's
 * answer, which is exactly right for every project where the rule acts and the model
 * only explains. Here the thing that may be sent IS the model's answer, so it has to
 * pass a checker between the engine and the modem, and no hook of the shared loop sits
 * at that point. So this file carries its own main, takes the same options, and reuses
 * io.h, rules.h, elog.h and edgellm.h unchanged. project_format_prompt keeps its name
 * and its signature so that the chapter and every other project read the same.
 *
 * The event is the ADXL345 through the IIO backend: v0, v1 and v2 are the x, y and z
 * accelerations in metres per second squared. The modem is NOT an io_source: the serial
 * backend parses replies and has no way to write a command (see
 * common/c/src/io_serial.c), so the AT port is opened here and parsed with at_parser.c.
 * One io_source, one modem, as the house rule asks.
 *
 * The time in a message is the elapsed time since the first event of the run, printed
 * hh:mm:ss, and it says so in the message with the literal "t+". It is not a UTC wall
 * clock. The Pi 3 Model B Plus has no battery backed clock, so after a boot with no
 * network its idea of the date is whatever the last synchronisation left behind, and a
 * message that may be read as evidence is the wrong place to put that. The modem's own
 * AT+CCLK? is the route to a real stamp and is named in the chapter's future exercises.
 */
#define _GNU_SOURCE
#include "at_parser.h"
#include "compose_check.h"
#include "edgellm.h"
#include "edgellm_main.h"
#include "elog.h"

#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define PROMPT_BUF  2048
#define E19_QUEUE   4
#define E19_G       9.80665
#define E19_READY_CACHE_S 5.0

/* Declared and not included: rules_e19.c is compiled a second time into the shared
 * harness tests/rules_test, whose include path carries the core headers alone, so it
 * owns no header of its own. src/compose_check_test.c carries the same declaration. */
int rule_e19_sends(int index, double t_s, double t_last_arm_s);

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

/* ---------------------------------------------------------------- small helpers */

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void sleep_s(double seconds)
{
    struct timespec ts;
    if (seconds <= 0.0)
        return;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

static char *read_text(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long n;
    char *text;
    if (fp == NULL)
        return NULL;
    if (fseek(fp, 0, SEEK_END) != 0 || (n = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    text = malloc((size_t)n + 1);
    if (text == NULL || fread(text, 1, (size_t)n, fp) != (size_t)n) {
        free(text);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    text[n] = '\0';
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r'))
        text[--n] = '\0';
    return text;
}

/* The peak magnitude of one event in g. This is the only place a number that reaches a
 * message is computed, and the printed form that e19_record_init() makes from it is
 * what both the log and the field checker compare against. */
static double peak_g(const io_event *ev)
{
    if (ev->n < 3)
        return 0.0;
    return sqrt(ev->v[0] * ev->v[0] + ev->v[1] * ev->v[1] + ev->v[2] * ev->v[2]) / E19_G;
}

/* Elapsed seconds as hh:mm:ss, wrapping after a day. */
static void stamp_of(double seconds, char *out, size_t len)
{
    long s = (long)(seconds < 0.0 ? 0.0 : seconds) % 86400L;
    snprintf(out, len, "%02ld:%02ld:%02ld", s / 3600L, (s / 60L) % 60L, s % 60L);
}

static void write_quoted(FILE *fp, const char *text)
{
    fputc('"', fp);
    for (; text != NULL && *text != '\0'; text++) {
        if (*text == '"')
            fputs("\"\"", fp);
        else if (*text == '\n' || *text == '\r')
            fputc(' ', fp);
        else
            fputc(*text, fp);
    }
    fputc('"', fp);
}

/* ---------------------------------------------------------------- the modem port */

typedef struct modem {
    int      fd;                  /* -1 when no modem is fitted */
    at_lines lines;
    char     to[32];              /* the destination number from --sms-to */
    int      registered;
    double   checked_s;           /* when registration was last read */
    int      last_mr;             /* the message reference of the last send */
    long     n_cme;               /* +CME and +CMS errors seen */
} modem;

static speed_t baud_code(long baud)
{
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    default:     return B0;
    }
}

/* Opens the AT character device. Spec "<device>:<baud>", split at the last ':' so a
 * /dev/serial/by-id name may carry its own. Non blocking, raw, no controlling
 * terminal: the loop must never block on a modem. */
static int modem_open(modem *m, const char *spec, char *err, size_t err_len)
{
    char dev[256];
    const char *colon = strrchr(spec, ':');
    long baud;
    speed_t code;
    struct termios tio;

    at_lines_init(&m->lines);
    m->fd = -1;
    m->checked_s = -1e9;
    if (colon == NULL || (baud = strtol(colon + 1, NULL, 10)) <= 0 ||
        (code = baud_code(baud)) == B0) {
        snprintf(err, err_len, "modem: spec needs <device>:<baud> at a standard rate");
        return -1;
    }
    snprintf(dev, sizeof(dev), "%.*s", (int)(colon - spec), spec);
    m->fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m->fd < 0) {
        snprintf(err, err_len, "modem: cannot open %s", dev);
        return -1;
    }
    if (tcgetattr(m->fd, &tio) != 0) {
        snprintf(err, err_len, "modem: %s is not a terminal", dev);
        close(m->fd);
        m->fd = -1;
        return -1;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, code);
    cfsetospeed(&tio, code);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(m->fd, TCSANOW, &tio) != 0) {
        snprintf(err, err_len, "modem: cannot configure %s", dev);
        close(m->fd);
        m->fd = -1;
        return -1;
    }
    tcflush(m->fd, TCIFLUSH);
    return 0;
}

static int modem_write(modem *m, const char *text)
{
    size_t left = strlen(text);
    const char *p = text;
    while (left > 0) {
        ssize_t w = write(m->fd, p, left);
        if (w <= 0)
            return -1;
        left -= (size_t)w;
        p += w;
    }
    return 0;
}

/* Reads whatever is waiting into the line assembler, for at most timeout_ms. */
static int modem_pump(modem *m, int timeout_ms)
{
    struct pollfd pfd;
    char buf[256];
    ssize_t r;
    pfd.fd = m->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeout_ms) <= 0)
        return 0;
    r = read(m->fd, buf, sizeof(buf));
    if (r <= 0)
        return 0;
    at_lines_feed(&m->lines, buf, (size_t)r);
    return 1;
}

/* Waits for one of the final result codes, or for the body prompt when want_prompt is
 * set, until the deadline. The newest information line is copied into info. Returns the
 * kind that ended the wait, or AT_OTHER on a timeout. */
static at_kind modem_wait(modem *m, int timeout_ms, int want_prompt, char *info,
                          size_t info_len)
{
    double deadline = now_s() + (double)timeout_ms / 1e3;
    char line[AT_LINE_MAX];
    if (info != NULL && info_len > 0)
        info[0] = '\0';
    for (;;) {
        while (at_lines_pop(&m->lines, line, sizeof(line))) {
            at_kind k = at_classify(line);
            if (k == AT_INFO && info != NULL)
                snprintf(info, info_len, "%s", line);
            if (k == AT_CME_ERROR || k == AT_CMS_ERROR) {
                m->n_cme++;
                fprintf(stderr, "e19: modem answered %s\n", line);
                return k;
            }
            if (k == AT_OK || k == AT_ERROR)
                return k;
            if (k == AT_PROMPT && want_prompt)
                return k;
        }
        if (now_s() >= deadline || stop_requested)
            return AT_OTHER;
        modem_pump(m, 50);
    }
}

/* One command and its final result code. Returns 0 on OK. */
static int modem_cmd(modem *m, const char *cmd, int timeout_ms, char *info, size_t len)
{
    char line[AT_LINE_MAX];
    while (at_lines_pop(&m->lines, line, sizeof(line)))
        ;                                      /* drop anything unsolicited first */
    if (modem_write(m, cmd) != 0 || modem_write(m, "\r") != 0)
        return -1;
    return modem_wait(m, timeout_ms, 0, info, len) == AT_OK ? 0 : -1;
}

/* Text mode, verbose errors, the GSM alphabet, echo off. AT+CSCS="GSM" is the line that
 * keeps the body in the 7 bit default alphabet the field checker admits; with
 * AT+CSCS="UCS2" the same 160 characters would not fit in one message. */
static int modem_init(modem *m, char *err, size_t err_len)
{
    static const char *const seq[] = { "AT", "ATE0", "AT+CMEE=2", "AT+CMGF=1",
                                       "AT+CSCS=\"GSM\"", NULL };
    int i;
    for (i = 0; seq[i] != NULL; i++) {
        if (modem_cmd(m, seq[i], 3000, NULL, 0) != 0) {
            snprintf(err, err_len, "modem: %s was refused or timed out", seq[i]);
            return -1;
        }
    }
    return 0;
}

/* Registration, cached for E19_READY_CACHE_S so that a quiet stretch costs no AT
 * traffic. Status 2, searching, is the mid attach state the queue exists for. */
static int modem_ready(modem *m)
{
    char info[AT_LINE_MAX];
    int r;
    if (m->fd < 0)
        return 0;
    if (now_s() - m->checked_s < E19_READY_CACHE_S)
        return m->registered;
    m->checked_s = now_s();
    if (modem_cmd(m, "AT+CREG?", 2000, info, sizeof(info)) != 0) {
        m->registered = 0;
        return 0;
    }
    r = at_parse_registered(info);
    m->registered = (r == 1);
    return m->registered;
}

/* ---------------------------------------------------------------- the send path */

/* The one function in this project that hands a message body to the modem. It is
 * called from alert_queue_drain() and from nowhere else, and alert_queue_drain() is
 * called from one place, inside the branch the rule's index opened. Writes the message
 * reference the module answers with into m->last_mr.
 *
 * The dialogue is the one 3GPP TS 27.005 fixes for text mode: the command names the
 * destination, the module answers with the prompt "> ", the body follows, and CTRL-Z
 * ends it. A CTRL-X instead of CTRL-Z abandons the message, which is what the error
 * path below does when the prompt never arrives. */
static int sms_send(modem *m, const char *body)
{
    char cmd[64], info[AT_LINE_MAX];
    at_kind k;
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", m->to);
    if (modem_write(m, cmd) != 0 || modem_write(m, "\r") != 0)
        return -1;
    k = modem_wait(m, 5000, 1, info, sizeof(info));
    if (k != AT_PROMPT) {
        modem_write(m, "\x18");                /* CTRL-X: abandon, do not leave a body */
        return -1;
    }
    if (modem_write(m, body) != 0 || modem_write(m, "\x1a") != 0)
        return -1;
    k = modem_wait(m, 60000, 0, info, sizeof(info));
    if (k != AT_OK)
        return -1;
    m->last_mr = -1;
    at_parse_cmgs(info, &m->last_mr);
    return 0;
}

/* ---------------------------------------------------------------- the queue */

/* The modem may be mid attach when an event arrives, and a send that blocked would
 * block the loop and with it the next reading of the accelerometer. So a message that
 * passed the field checker is queued, and the queue is drained on the spot and again on
 * the next armed event. The queue holds four messages: more than that in flight means
 * the modem has been unreachable across four armings, which at the rule's minimum
 * interval is at least forty seconds, and the oldest is then dropped and counted. */
typedef struct alert {
    char   msg[E19_MSG_MAX];
    double t_s;
    int    tries;
} alert;

typedef struct alert_queue {
    alert item[E19_QUEUE];
    int   n;
    long  dropped;
} alert_queue;

static void alert_queue_push(alert_queue *q, const char *msg, double t_s)
{
    int i;
    if (q->n >= E19_QUEUE) {
        for (i = 1; i < E19_QUEUE; i++)
            q->item[i - 1] = q->item[i];
        q->n = E19_QUEUE - 1;
        q->dropped++;
    }
    snprintf(q->item[q->n].msg, sizeof(q->item[q->n].msg), "%s", msg);
    q->item[q->n].t_s = t_s;
    q->item[q->n].tries = 0;
    q->n++;
}

/* Sends what it can, oldest first, and leaves the rest queued. Returns how many went
 * out. A message that has been tried three times is dropped and counted: the modem is
 * not coming back inside this alert's usefulness. */
static long alert_queue_drain(alert_queue *q, modem *m)
{
    long sent = 0;
    if (q->n == 0 || m->fd < 0)
        return 0;
    if (!modem_ready(m))
        return 0;
    while (q->n > 0) {
        if (sms_send(m, q->item[0].msg) == 0) {
            sent++;
        } else if (++q->item[0].tries < 3) {
            break;
        } else {
            q->dropped++;
        }
        memmove(&q->item[0], &q->item[1], sizeof(alert) * (size_t)(q->n - 1));
        q->n--;
    }
    return sent;
}

/* ---------------------------------------------------------------- the prompt */

/* The one event the message is about, as plain text. The field names are the ones the
 * grammar's skeleton uses, so the model has to copy and not translate, and the numbers
 * are already in their printed form, so the model never rounds. The thresholds are not
 * here: they are in prompt.txt, decoded once at start, and they cost no tokens per
 * event. The record is set before this is called, which is why it is a file static. */
static e19_record record;

const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    (void)ctx;
    (void)ev;
    snprintf(buf, len, "event %s, peak %s g, time t+%s. Compose the message.",
             record.type, record.peak, record.stamp);
    return buf;
}

/* ---------------------------------------------------------------- ChatML */

/* The same wrapping the shared main does, repeated here because this project does not
 * use that main: SmolLM2 and Qwen2.5 are ChatML models, TinyLlama uses the Zephyr form,
 * and "none" passes the text through. prompt.txt stays plain text in every case. */
static const char *template_name = "chatml";

static char *wrap_system(const char *prompt)
{
    const char *head = "<|im_start|>system\n", *tail = "<|im_end|>\n";
    char *out;
    if (strcmp(template_name, "zephyr") == 0) {
        head = "<|system|>\n";
        tail = "</s>\n";
    } else if (strcmp(template_name, "none") == 0) {
        head = "";
        tail = "";
    }
    out = malloc(strlen(head) + strlen(prompt) + strlen(tail) + 1);
    if (out != NULL)
        sprintf(out, "%s%s%s", head, prompt, tail);
    return out;
}

static const char *wrap_user(const char *user, char *buf, size_t len)
{
    const char *head = "<|im_start|>user\n";
    const char *tail = "<|im_end|>\n<|im_start|>assistant\n";
    if (strcmp(template_name, "zephyr") == 0) {
        head = "<|user|>\n";
        tail = "</s>\n<|assistant|>\n";
    } else if (strcmp(template_name, "none") == 0) {
        return user;
    }
    if (strlen(head) + strlen(user) + strlen(tail) + 1 > len)
        return NULL;
    sprintf(buf, "%s%s%s", head, user, tail);
    return buf;
}

/* ---------------------------------------------------------------- the alert ledger */

/* One row per event the rule classified as TAP or FREEFALL, armed or not. This is the
 * record the acceptance test reads: every sent message beside the event it was composed
 * from, and every refusal with its reason. The main log keeps the shape elog.h fixes
 * for every project, so the two files are read side by side and neither changes shape
 * for this chapter. */
static FILE *alerts_open(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        return NULL;
    fputs("t_s,rule,armed,peak_g,stamp,verdict,sent,mr,queued,message\n", fp);
    fflush(fp);
    return fp;
}

static void alerts_row(FILE *fp, double t_s, const char *rule, int armed, double pk,
                       const char *stamp, const char *verdict, int sent, int mr,
                       int queued, const char *msg)
{
    if (fp == NULL)
        return;
    fprintf(fp, "%.6f,", t_s);
    write_quoted(fp, rule);
    fprintf(fp, ",%d,%.2f,", armed, pk);
    write_quoted(fp, stamp);
    fputc(',', fp);
    write_quoted(fp, verdict);
    fprintf(fp, ",%d,", sent);
    if (sent && mr >= 0)
        fprintf(fp, "%d", mr);
    fprintf(fp, ",%d,", queued);
    write_quoted(fp, msg);
    fputc('\n', fp);
    fflush(fp);
}

/* ---------------------------------------------------------------- main */

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --project e19 (--replay <csv> | --live iio <dir>[:<interval_s>])\n"
            "          --grammar <gbnf> --prompt <txt> --log <csv> [--alerts <csv>]\n"
            "          [--model <gguf>] [--state <path>] [--modem <dev>:<baud>]\n"
            "          [--sms-to <number>] [--n-predict N] [--threads N] [--n-ctx N]\n"
            "          [--temperature T] [--interval S] [--max-events N]\n"
            "          [--template chatml|zephyr|none] [--stub]\n",
            argv0);
    return 2;
}

int main(int argc, char **argv)
{
    const char *project = NULL, *replay = NULL, *backend = NULL, *spec = NULL;
    const char *grammar = NULL, *prompt_path = NULL, *log_path = NULL, *model = NULL;
    const char *state = NULL, *alerts_path = NULL, *modem_spec = NULL, *sms_to = NULL;
    int want_stub = 0, max_events = 0, i, r = 0, rc = 0;
    double interval = 0.0, t_zero = -1.0, t_last_arm = -1.0;
    edgellm_config cfg;
    edgellm_t *engine = NULL;
    io_source *src = NULL;
    elog *log = NULL;
    FILE *alerts = NULL;
    modem mdm;
    alert_queue queue;
    const rule_desc *rule;
    rule_ctx ctx;
    io_event ev;
    char err[256], stamp[16];
    char *system_prompt = NULL, *prompt_buf = NULL, *wrapped_buf = NULL;
    long n_events = 0, n_class[4] = { 0, 0, 0, 0 }, n_armed = 0, n_blocked = 0;
    long n_composed = 0, n_model_errors = 0, n_sent = 0;
    long n_verdict[E19_VERDICT_COUNT];
    struct sigaction sa;

    memset(&mdm, 0, sizeof(mdm));
    memset(&queue, 0, sizeof(queue));
    memset(n_verdict, 0, sizeof(n_verdict));
    mdm.fd = -1;
    mdm.last_mr = -1;
    edgellm_config_init(&cfg);
    cfg.n_predict = 64;                        /* one message, not an enum */
    cfg.temperature = 0.3f;                    /* the house setting for text */

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(a, "--stub") == 0) { want_stub = 1; continue; }
        if (v == NULL)
            return usage(argv[0]);
        if (strcmp(a, "--project") == 0) project = v;
        else if (strcmp(a, "--replay") == 0) replay = v;
        else if (strcmp(a, "--live") == 0) {
            if (i + 2 >= argc)
                return usage(argv[0]);
            backend = v;
            spec = argv[i + 2];
            i++;
        }
        else if (strcmp(a, "--grammar") == 0) grammar = v;
        else if (strcmp(a, "--prompt") == 0) prompt_path = v;
        else if (strcmp(a, "--log") == 0) log_path = v;
        else if (strcmp(a, "--alerts") == 0) alerts_path = v;
        else if (strcmp(a, "--model") == 0) model = v;
        else if (strcmp(a, "--state") == 0) state = v;
        else if (strcmp(a, "--modem") == 0) modem_spec = v;
        else if (strcmp(a, "--sms-to") == 0) sms_to = v;
        else if (strcmp(a, "--n-predict") == 0) cfg.n_predict = atoi(v);
        else if (strcmp(a, "--threads") == 0) cfg.n_threads = atoi(v);
        else if (strcmp(a, "--n-ctx") == 0) cfg.n_ctx = atoi(v);
        else if (strcmp(a, "--temperature") == 0) cfg.temperature = (float)atof(v);
        else if (strcmp(a, "--interval") == 0) interval = atof(v);
        else if (strcmp(a, "--max-events") == 0) max_events = atoi(v);
        else if (strcmp(a, "--template") == 0) template_name = v;
        else return usage(argv[0]);
        i++;
    }
    if (strcmp(template_name, "chatml") != 0 && strcmp(template_name, "zephyr") != 0 &&
        strcmp(template_name, "none") != 0)
        return usage(argv[0]);
    if (project == NULL || grammar == NULL || prompt_path == NULL || log_path == NULL ||
        (replay == NULL && backend == NULL) || (replay != NULL && backend != NULL))
        return usage(argv[0]);
    if (backend != NULL && strcmp(backend, "iio") != 0) {
        fprintf(stderr, "%s: the live source of e19 is the iio backend\n", argv[0]);
        return 2;
    }
    if (modem_spec != NULL && sms_to == NULL) {
        fprintf(stderr, "%s: --modem also needs --sms-to <number>\n", argv[0]);
        return 2;
    }
#ifdef EDGELLM_STUB_MODEL
    (void)want_stub;
#else
    if (want_stub) {
        fprintf(stderr, "%s: built with the real engine; configure with "
                        "-DEDGELLM_STUB_MODEL=ON for --stub\n", argv[0]);
        return 2;
    }
    if (model == NULL) {
        fprintf(stderr, "%s: --model is required with the real engine\n", argv[0]);
        return 2;
    }
#endif

    rule = rule_find(project);
    if (rule == NULL) {
        fprintf(stderr, "%s: no rule registered for project %s\n", argv[0], project);
        return 1;
    }
    {
        char *plain = read_text(prompt_path);
        if (plain == NULL) {
            fprintf(stderr, "%s: cannot read %s\n", argv[0], prompt_path);
            return 1;
        }
        system_prompt = wrap_system(plain);
        free(plain);
        if (system_prompt == NULL)
            return 1;
    }
    cfg.model_path = model != NULL ? model : "";
    cfg.grammar_path = grammar;
    cfg.system_prompt = system_prompt;
    cfg.state_path = state;

    engine = edgellm_open(&cfg, err, sizeof(err));
    if (engine == NULL) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        rc = 1;
        goto out;
    }
    src = replay != NULL ? io_open_replay(replay, err, sizeof(err))
                         : io_open_live(backend, spec, err, sizeof(err));
    if (src == NULL) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        rc = 1;
        goto out;
    }
    log = elog_open(log_path, edgellm_engine_version());
    if (log == NULL) {
        fprintf(stderr, "%s: cannot write %s\n", argv[0], log_path);
        rc = 1;
        goto out;
    }
    if (alerts_path != NULL) {
        alerts = alerts_open(alerts_path);
        if (alerts == NULL) {
            fprintf(stderr, "%s: cannot write %s\n", argv[0], alerts_path);
            rc = 1;
            goto out;
        }
    }
    /* No modem fitted is a run that composes and checks and sends nothing, which is
     * what CI does and what the bench does until a SIM is in the HAT. It is reported
     * on every row of the ledger rather than assumed. */
    if (modem_spec != NULL) {
        snprintf(mdm.to, sizeof(mdm.to), "%s", sms_to);
        if (modem_open(&mdm, modem_spec, err, sizeof(err)) != 0 ||
            modem_init(&mdm, err, sizeof(err)) != 0) {
            fprintf(stderr, "%s: %s\n", argv[0], err);
            rc = 1;
            goto out;
        }
    }
    prompt_buf = malloc(PROMPT_BUF);
    wrapped_buf = malloc(PROMPT_BUF + 128);
    if (prompt_buf == NULL || wrapped_buf == NULL) {
        rc = 1;
        goto out;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    memset(&ctx, 0, sizeof(ctx));

    while (!stop_requested && (r = io_next(src, &ev)) == 1) {
        int idx = rule->fn(&ctx, &ev);
        int n_names = rule_name_count(rule);
        int armed, sent_now = 0;
        double pk;
        edgellm_result res;
        const edgellm_result *logged = NULL;
        e19_verdict verdict = E19_VERDICT_COUNT;

        if (idx < 0 || idx >= n_names) {
            fprintf(stderr, "%s: rule %s returned %d outside its %d names\n", argv[0],
                    project, idx, n_names);
            rc = 1;
            break;
        }
        n_class[idx]++;
        if (t_zero < 0.0)
            t_zero = ev.t_s;
        pk = peak_g(&ev);
        stamp_of(ev.t_s - t_zero, stamp, sizeof(stamp));

        /* The send rule, and the only thing that opens the branch below. The model has
         * not been asked yet and cannot be an argument to it. */
        armed = rule_e19_sends(idx, ev.t_s, t_last_arm);
        if (armed) {
            const char *user;
            t_last_arm = ev.t_s;
            n_armed++;
            e19_record_init(&record, rule->names[idx], pk, stamp);
            user = project_format_prompt(&ev, &ctx, prompt_buf, PROMPT_BUF);
            if (user != NULL)
                user = wrap_user(user, wrapped_buf, PROMPT_BUF + 128);
            if (user != NULL && edgellm_decide(engine, user, &res) == 0) {
                n_composed++;
                verdict = e19_compose_check(res.text, &record);
                n_verdict[verdict]++;
                if (verdict == E19_PASS)
                    alert_queue_push(&queue, res.text, ev.t_s);
                /* The one call to the send path in this project. It is inside the
                 * branch the rule's index opened, and it can only send what the field
                 * checker passed, because that is all the queue ever holds. */
                n_sent += alert_queue_drain(&queue, &mdm);
                sent_now = (verdict == E19_PASS && queue.n == 0 && mdm.fd >= 0);
            } else {
                n_model_errors++;
                memset(&res, 0, sizeof(res));
            }
            logged = &res;
            alerts_row(alerts, ev.t_s, rule->names[idx], 1, pk, stamp,
                       verdict < E19_VERDICT_COUNT ? e19_verdict_name(verdict)
                                                   : "refused: no answer",
                       sent_now, mdm.last_mr, queue.n, res.text);
        } else if (idx == 1 || idx == 3) {
            /* The rule classified a TAP or a FREEFALL and the minimum interval refused
             * it. No model call, no modem dialogue, and a row in the ledger so that the
             * alerts the gate suppressed are counted and not invisible. */
            n_blocked++;
            alerts_row(alerts, ev.t_s, rule->names[idx], 0, pk, stamp,
                       "not armed: inside the minimum interval", 0, -1, queue.n, "");
        }
        if (elog_row(log, &ev, rule->names[idx], logged) != 0) {
            fprintf(stderr, "%s: log write failed\n", argv[0]);
            rc = 1;
            break;
        }
        rule_ctx_push(&ctx, &ev);
        n_events++;
        if (max_events > 0 && n_events >= max_events)
            break;
        if (interval > 0.0)
            sleep_s(interval);
    }
    if (rc == 0 && !stop_requested && r < 0 && (max_events == 0 || n_events < max_events))
        rc = 1;

    fprintf(stderr,
            "%s: %ld events (NONE %ld, TAP %ld, TILT %ld, FREEFALL %ld), %ld armed, "
            "%ld blocked by the minimum interval, %ld composed, %ld model errors, "
            "%ld passed the checker, %ld refused, %ld invented a number, %ld sent, "
            "%d left queued, %ld dropped, modem %s, engine %s, source %s\n",
            project, n_events, n_class[0], n_class[1], n_class[2], n_class[3], n_armed,
            n_blocked, n_composed, n_model_errors, n_verdict[E19_PASS],
            n_composed - n_verdict[E19_PASS], n_verdict[E19_REFUSED_INVENTED_NUMBER],
            n_sent, queue.n, queue.dropped, mdm.fd >= 0 ? "fitted" : "not fitted",
            edgellm_engine_version(), io_backend_name(src));
    for (i = 1; i < (int)E19_VERDICT_COUNT; i++)
        if (n_verdict[i] > 0)
            fprintf(stderr, "%s: %s, %ld of %ld composed\n", project,
                    e19_verdict_name((e19_verdict)i), n_verdict[i], n_composed);
out:
    if (alerts != NULL)
        fclose(alerts);
    if (mdm.fd >= 0)
        close(mdm.fd);
    free(wrapped_buf);
    free(prompt_buf);
    elog_close(log);
    io_close(src);
    edgellm_close(engine);
    free(system_prompt);
    return rc;
}
