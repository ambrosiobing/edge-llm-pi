/* daemon.c: the inference half of project e08, catalogue variant O. One process owns
 * the model and nothing else: no GPIO line, no framebuffer, no sysfs file. It listens
 * on a Unix domain socket and answers one request at a time.
 *
 *   edgellm-e08-daemon --socket <path> --grammar <gbnf> --prompt <txt> [--model <gguf>]
 *                      [--threads N] [--n-predict N] [--n-ctx N] [--temperature T]
 *                      [--state <path>] [--template chatml|zephyr|none] [--stub]
 *
 * Line protocol, one request per line, one reply per line:
 *
 *   DECIDE <user text>      ->  <grammar output>            or  ERROR <reason>
 *   STATS                   ->  STATS <prompt_ms> <generate_ms> <n_generated>
 *   VERSION                 ->  VERSION <engine>
 *
 * The user text is wrapped in the chat template here, in the same form as the shared
 * main in common/c/src/edgellm_main.c wraps it (ChatML for SmolLM2 and Qwen2.5), and
 * prompt.txt stays plain text. The system prompt is decoded once at start; with no
 * --state the engine keeps it resident and trims the context back to it before every
 * request with llama_memory_seq_rm, which is the resident route of the volume's
 * latency design and the one a long-running service wants.
 *
 * SIGTERM and SIGINT end the accept loop; the socket file is unlinked and the engine
 * closed before exit 0. Signal 9 ends the process at once, which is what the restart
 * test of the chapter does on purpose; systemd starts this binary again.
 */
#define _GNU_SOURCE
#include "edgellm.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define LINE_MAX_BYTES 4096

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
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

/* The chat template, the same three forms as the shared main. */
static const char *template_name = "chatml";

static char *wrap_system(const char *prompt)
{
    const char *head, *tail;
    char *out;
    if (strcmp(template_name, "zephyr") == 0) {
        head = "<|system|>\n";
        tail = "</s>\n";
    } else if (strcmp(template_name, "none") == 0) {
        head = "";
        tail = "";
    } else {
        head = "<|im_start|>system\n";
        tail = "<|im_end|>\n";
    }
    out = malloc(strlen(head) + strlen(prompt) + strlen(tail) + 1);
    if (out != NULL)
        sprintf(out, "%s%s%s", head, prompt, tail);
    return out;
}

static const char *wrap_user(const char *user, char *buf, size_t len)
{
    const char *head, *tail;
    if (strcmp(template_name, "zephyr") == 0) {
        head = "<|user|>\n";
        tail = "</s>\n<|assistant|>\n";
    } else if (strcmp(template_name, "none") == 0) {
        return user;
    } else {
        head = "<|im_start|>user\n";
        tail = "<|im_end|>\n<|im_start|>assistant\n";
    }
    if (strlen(head) + strlen(user) + strlen(tail) + 1 > len)
        return NULL;
    sprintf(buf, "%s%s%s", head, user, tail);
    return buf;
}

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --socket <path> --grammar <gbnf> --prompt <txt> [--model <gguf>]\n"
            "          [--threads N] [--n-predict N] [--n-ctx N] [--temperature T]\n"
            "          [--state <path>] [--template chatml|zephyr|none] [--stub]\n",
            argv0);
    return 2;
}

static int write_all(int fd, const char *text, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, text, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        text += n;
        len -= (size_t)n;
    }
    return 0;
}

static int reply_line(int fd, const char *text)
{
    char nl = '\n';
    if (write_all(fd, text, strlen(text)) != 0)
        return -1;
    return write_all(fd, &nl, 1);
}

/* Reads one line into buf without the newline. Returns 1, 0 at end of stream, -1 on an
 * error or a line longer than the buffer. */
static int read_line(int fd, char *buf, size_t len)
{
    size_t k = 0;
    for (;;) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return k > 0 ? 1 : 0;
        if (c == '\n') {
            buf[k] = '\0';
            return 1;
        }
        if (k + 1 >= len)
            return -1;
        buf[k++] = c;
    }
}

/* One request. The answer text goes back as one line; the grammar admits no newline,
 * and any that slipped through is replaced so the protocol holds. */
static int handle_request(edgellm_t *engine, int fd, const char *line, edgellm_result *last,
                          char *wrapped, size_t wrapped_len)
{
    char out[640];
    if (strncmp(line, "DECIDE ", 7) == 0) {
        const char *user = wrap_user(line + 7, wrapped, wrapped_len);
        char *p;
        if (user == NULL)
            return reply_line(fd, "ERROR user text too long");
        if (edgellm_decide(engine, user, last) != 0) {
            memset(last, 0, sizeof(*last));
            return reply_line(fd, "ERROR decide failed");
        }
        for (p = last->text; *p != '\0'; p++)
            if (*p == '\n' || *p == '\r')
                *p = ' ';
        return reply_line(fd, last->text);
    }
    if (strcmp(line, "STATS") == 0) {
        snprintf(out, sizeof(out), "STATS %.3f %.3f %d", last->prompt_ms, last->generate_ms,
                 last->n_generated);
        return reply_line(fd, out);
    }
    if (strcmp(line, "VERSION") == 0) {
        snprintf(out, sizeof(out), "VERSION %s", edgellm_engine_version());
        return reply_line(fd, out);
    }
    return reply_line(fd, "ERROR unknown command");
}

int main(int argc, char **argv)
{
    const char *socket_path = NULL, *grammar = NULL, *prompt_path = NULL, *model = NULL;
    const char *state = NULL;
    int want_stub = 0, i, listen_fd = -1, rc = 0;
    long n_requests = 0;
    edgellm_config cfg;
    edgellm_t *engine = NULL;
    edgellm_result last;
    char err[256], *system_prompt = NULL, *line = NULL, *wrapped = NULL;
    struct sockaddr_un addr;
    struct sigaction sa;

    edgellm_config_init(&cfg);
    cfg.n_predict = 64;
    cfg.temperature = 0.3f;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(a, "--stub") == 0) { want_stub = 1; continue; }
        if (v == NULL)
            return usage(argv[0]);
        if (strcmp(a, "--socket") == 0) socket_path = v;
        else if (strcmp(a, "--grammar") == 0) grammar = v;
        else if (strcmp(a, "--prompt") == 0) prompt_path = v;
        else if (strcmp(a, "--model") == 0) model = v;
        else if (strcmp(a, "--state") == 0) state = v;
        else if (strcmp(a, "--n-predict") == 0) cfg.n_predict = atoi(v);
        else if (strcmp(a, "--threads") == 0) cfg.n_threads = atoi(v);
        else if (strcmp(a, "--n-ctx") == 0) cfg.n_ctx = atoi(v);
        else if (strcmp(a, "--temperature") == 0) cfg.temperature = (float)atof(v);
        else if (strcmp(a, "--template") == 0) template_name = v;
        else return usage(argv[0]);
        i++;
    }
    if (socket_path == NULL || grammar == NULL || prompt_path == NULL)
        return usage(argv[0]);
    if (strcmp(template_name, "chatml") != 0 && strcmp(template_name, "zephyr") != 0 &&
        strcmp(template_name, "none") != 0)
        return usage(argv[0]);
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
    line = malloc(LINE_MAX_BYTES);
    wrapped = malloc(LINE_MAX_BYTES + 128);
    if (line == NULL || wrapped == NULL) {
        rc = 1;
        goto out;
    }

    listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd < 0) {
        fprintf(stderr, "%s: socket: %s\n", argv[0], strerror(errno));
        rc = 1;
        goto out;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(socket_path) >= sizeof(addr.sun_path)) {
        fprintf(stderr, "%s: socket path too long\n", argv[0]);
        rc = 1;
        goto out;
    }
    strcpy(addr.sun_path, socket_path);
    unlink(socket_path);                       /* a stale file from a process ended by 9 */
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(listen_fd, 4) != 0) {
        fprintf(stderr, "%s: cannot listen on %s: %s\n", argv[0], socket_path, strerror(errno));
        rc = 1;
        goto out;
    }
    chmod(socket_path, 0660);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);                  /* a client that left mid-reply is not fatal */
    memset(&last, 0, sizeof(last));
    fprintf(stderr, "e08-daemon: listening on %s, engine %s\n", socket_path,
            edgellm_engine_version());

    while (!stop_requested) {
        struct pollfd pfd = { listen_fd, POLLIN, 0 };
        int client, r = poll(&pfd, 1, 500);
        if (r <= 0)
            continue;
        client = accept(listen_fd, NULL, NULL);
        if (client < 0)
            continue;
        while (!stop_requested && read_line(client, line, LINE_MAX_BYTES) == 1) {
            n_requests++;
            if (handle_request(engine, client, line, &last, wrapped, LINE_MAX_BYTES + 128) != 0)
                break;
        }
        close(client);
    }
    fprintf(stderr, "e08-daemon: %ld requests, rss %ld kB, clean exit\n", n_requests,
            edgellm_rss_kb());
out:
    if (listen_fd >= 0) {
        close(listen_fd);
        unlink(socket_path);
    }
    free(wrapped);
    free(line);
    edgellm_close(engine);
    free(system_prompt);
    return rc;
}
