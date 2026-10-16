/* e17_main.c: project e17, the cellular status summariser, in two halves that are one
 * binary.
 *
 *   edgellm-e17 --project e17 --replay <samples.csv> --grammar ... --prompt ...
 *               --log <csv> [--model <gguf>] [--n-predict 64] [--temperature 0.3]
 *                                                     the loop, on the shared main
 *   edgellm-e17 --modem <device>:<baud> --sample-log <csv> [--sample-interval 60]
 *               [--max-samples N] [--query-timeout 2]
 *                                                     the sampler, on the AT parser
 *
 * The loop half is the volume's shared main: one event is one status sample, the rule
 * decides, the model is asked under the grammar, and the row is logged. This file adds
 * the prompt formatter and nothing else to it.
 *
 * The sampler half is why the project is in C. Every sample period it asks the modem
 * the four queries of at_parser.h in order, feeds every line that comes back to the
 * parser, and appends one row in the replay CSV shape:
 *
 *     t_s,tag,v0,v1,v2,v3,v4
 *     60.0,A1,18,99,1,7,1
 *
 * tag is the operator name the modem reported ("none" when there is none), v0 the
 * signal quality from AT+CSQ, v1 its bit error rate, v2 the registration state from
 * AT+CEREG?, v3 the access technology and v4 the power saving mode; an unanswered
 * field is -1. So the file the sampler writes on the bench and the file
 * tools/make_replay.py writes for the modelled day are the same shape, and the loop
 * does not know which one it is reading. That is what "ready for board wiring" means
 * here, and it is a deliberate deviation from the volume's one-binary --live rule,
 * stated in the chapter: a status sample is four AT responses, and an event source
 * that yields one event per line cannot carry one.
 *
 * Two things about the sampler are worth reading before changing it. The replies are
 * read through the shared termios backend (common/c/src/io_serial.c, built in with
 * -DEDGELLM_IO_SERIAL=ON), which owns the port settings and splits the stream into
 * lines; the four queries go out on a second, write-only descriptor on the same
 * device, which is safe because only one descriptor ever writes and only the other
 * ever reads. And the per-query timeout is a SIGALRM: that backend's read is blocking
 * by design, so a modem that answers nothing would otherwise be waited for forever,
 * and four lines a minute do not justify a reader thread.
 */
#define _GNU_SOURCE
#include "at_parser.h"
#include "edgellm_main.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define E17_MODEL_EVERY     15     /* samples; one model answer every 15 minutes */
#define E17_SAMPLE_SECONDS  60.0   /* the sample period the chapter's acceptance uses */

/* ------------------------------------------------------------------ the loop half */

static const char *reg_words(int stat)
{
    switch (stat) {
    case 0:  return "not registered and not searching";
    case 1:  return "registered on the home network";
    case 2:  return "searching for a network";
    case 3:  return "registration denied";
    case 5:  return "registered and roaming";
    default: return "registration unknown";
    }
}

static const char *act_words(int act)
{
    if (act == 7) return "Cat-M1";
    if (act == 9) return "NB-IoT";
    return "unknown";
}

/* One user text per model answer, built from the sample and nothing else. The
 * thresholds are in the system prompt, so they cost no tokens per event, and the
 * rule's own verdict is deliberately absent: the model proposes its own state and the
 * agreement table is only meaningful while it has not been told the answer.
 *
 * Returning NULL logs the rule alone for this sample, which is how the 15 minute
 * model interval is implemented: 1440 samples a day, 96 of them answered. */
const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    long minute = (long)(ev->t_s / E17_SAMPLE_SECONDS + 0.5);
    int csq, ber, stat, act, psm;

    (void)ctx;
    if (minute % E17_MODEL_EVERY != 0)
        return NULL;
    csq = ev->n > 0 ? (int)ev->v[0] : AT_UNKNOWN;
    ber = ev->n > 1 ? (int)ev->v[1] : AT_UNKNOWN;
    stat = ev->n > 2 ? (int)ev->v[2] : AT_UNKNOWN;
    act = ev->n > 3 ? (int)ev->v[3] : AT_UNKNOWN;
    psm = ev->n > 4 ? (int)ev->v[4] : AT_UNKNOWN;
    snprintf(buf, len,
             "signal %d, bit error rate %d, %s, operator %s, access technology %s, "
             "power saving %s. Three sentences.",
             csq, ber, reg_words(stat), ev->tag[0] != '\0' ? ev->tag : "none",
             act_words(act), psm == 1 ? "on" : "off");
    return buf;
}

/* ------------------------------------------------------------------ the sampler half */

static volatile sig_atomic_t alarm_fired;

static void on_alarm(int sig)
{
    (void)sig;
    alarm_fired = 1;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Asks the four queries once and leaves the answers in the parser. Returns the number
 * of queries that were answered, so the caller can log a partial sample as partial. */
static int sample_once(io_source *src, int wfd, at_parser *p, double timeout_s)
{
    int answered = 0;
    int q;

    at_parser_init(p);
    for (q = 0; q < (int)AT_Q_COUNT; q++) {
        char cmd[32];
        int n = snprintf(cmd, sizeof cmd, "%s\r", at_query_command((at_query)q));
        at_parser_begin(p, (at_query)q, now_s(), timeout_s);
        alarm_fired = 0;
        if (write(wfd, cmd, (size_t)n) != (ssize_t)n) {
            fprintf(stderr, "e17: cannot write %s to the modem\n", cmd);
            return answered;
        }
        alarm((unsigned int)(timeout_s + 1.0));
        for (;;) {
            io_event line;
            at_result r;
            if (io_next(src, &line) != 1) {
                fprintf(stderr, "e17: %s %s\n", at_query_command((at_query)q),
                        alarm_fired ? "timed out" : "read failed");
                break;
            }
            r = at_parser_line(p, line.tag);
            if (r == AT_COMPLETE) {
                answered++;
                break;
            }
            if (r == AT_FAILED) {
                fprintf(stderr, "e17: %s returned an error\n", at_query_command((at_query)q));
                break;
            }
            if (at_parser_timed_out(p, now_s()))
                break;
        }
        alarm(0);
    }
    return answered;
}

/* The CSV the loop replays. A comma in an operator name would split the tag column, so
 * it is replaced by a space, and the file stays readable by io_open_replay. */
static void write_sample(FILE *fp, double t_s, const at_sample *s)
{
    char oper[AT_OPER_LEN];
    size_t i;

    snprintf(oper, sizeof oper, "%s", s->oper[0] != '\0' ? s->oper : "none");
    for (i = 0; oper[i] != '\0'; i++)
        if (oper[i] == ',')
            oper[i] = ' ';
    fprintf(fp, "%.1f,%s,%d,%d,%d,%d,%d\n", t_s, oper, s->csq, s->ber, s->reg_stat,
            s->act, s->psm);
    fflush(fp);
}

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --modem <device>:<baud> --sample-log <csv>\n"
            "          [--sample-interval <s>] [--query-timeout <s>] [--max-samples <n>]\n"
            "   or: %s --project e17 --replay <samples.csv> ... (the shared loop)\n",
            argv0, argv0);
    return 2;
}

static int sampler(int argc, char **argv)
{
    const char *spec = NULL, *out_path = NULL;
    double interval = E17_SAMPLE_SECONDS, timeout_s = 2.0;
    long max_samples = 0, taken = 0;
    char err[256], dev[256];
    const char *colon;
    io_source *src;
    FILE *fp;
    at_parser parser;
    struct sigaction sa;
    int wfd, i;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (v == NULL)
            return usage(argv[0]);
        if (strcmp(a, "--modem") == 0) spec = v;
        else if (strcmp(a, "--sample-log") == 0) out_path = v;
        else if (strcmp(a, "--sample-interval") == 0) interval = atof(v);
        else if (strcmp(a, "--query-timeout") == 0) timeout_s = atof(v);
        else if (strcmp(a, "--max-samples") == 0) max_samples = atol(v);
        else return usage(argv[0]);
        i++;
    }
    if (spec == NULL || out_path == NULL || interval <= 0.0 || timeout_s <= 0.0)
        return usage(argv[0]);

    colon = strrchr(spec, ':');
    if (colon == NULL)
        return usage(argv[0]);
    snprintf(dev, sizeof dev, "%.*s", (int)(colon - spec), spec);

    src = io_open_live("serial", spec, err, sizeof err);
    if (src == NULL) {
        fprintf(stderr, "e17: %s\n", err);
        return 1;
    }
    wfd = open(dev, O_WRONLY | O_NOCTTY);
    if (wfd < 0) {
        fprintf(stderr, "e17: cannot open %s for writing\n", dev);
        io_close(src);
        return 1;
    }
    fp = fopen(out_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "e17: cannot write %s\n", out_path);
        close(wfd);
        io_close(src);
        return 1;
    }
    fputs("t_s,tag,v0,v1,v2,v3,v4\n", fp);
    fflush(fp);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;            /* no SA_RESTART: the blocking read must return */
    sigaction(SIGALRM, &sa, NULL);

    for (;;) {
        int answered = sample_once(src, wfd, &parser, timeout_s);
        const at_sample *s = at_parser_sample(&parser);
        write_sample(fp, (double)taken * interval, s);
        fprintf(stderr, "e17 sample %ld: %d of %d queries answered, %d urcs dropped, "
                        "csq %d, registration %d, operator %s\n",
                taken, answered, (int)AT_Q_COUNT, s->urcs, s->csq, s->reg_stat, s->oper);
        taken++;
        if (max_samples > 0 && taken >= max_samples)
            break;
        {
            struct timespec ts;
            ts.tv_sec = (time_t)interval;
            ts.tv_nsec = (long)((interval - (double)ts.tv_sec) * 1e9);
            nanosleep(&ts, NULL);
        }
    }
    fclose(fp);
    close(wfd);
    io_close(src);
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
        if (strcmp(argv[i], "--modem") == 0)
            return sampler(argc, argv);
    return edgellm_main(argc, argv);
}
