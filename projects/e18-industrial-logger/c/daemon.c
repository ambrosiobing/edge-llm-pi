/* daemon.c: the four functions of abi.h served over a Unix domain socket, so that
 * the Rust device service can reach the engine out of process as well as in
 * process. Same engine, same grammar, same answer; the only difference is which
 * process dies when the engine does, which is the whole reason the socket exists.
 *
 *   edgellm-e18-daemon --socket <path> --model <gguf> --grammar <gbnf>
 *                      --prompt <txt> [--state <path>] [--n-ctx N] [--threads N]
 *                      [--n-predict N] [--temperature T]
 *
 * The protocol is one line of text per request and one per reply, LF terminated,
 * at most LINE_MAX bytes including the LF. It is deliberately not a format with a
 * schema: a schema is a second thing to version, and the grammar already bounds
 * what comes back.
 *
 *   V            -> V <version>      the library's EDGELLM_ABI_VERSION
 *   D <user>     -> A <answer>       one decision; <answer> is the JSON object
 *                -> E <reason>       the engine refused, or the line was too long
 *
 * The user text carries no newline by construction (the service's prompt is one
 * line) and the answer carries none either (the grammar's ws admits no newline), so
 * a line is a message and no framing is needed beyond the LF. A line that arrives
 * longer than the buffer is answered E and the connection is closed, rather than
 * being split into two requests.
 *
 * One client at a time, on purpose: there is one engine, a decision is serialised
 * inside abi.c anyway, and a second connection waiting in the listen backlog is
 * easier to reason about than two clients sharing a model. SIGPIPE is ignored so
 * that a service stopped mid reply ends this connection and not this process.
 */
#define _GNU_SOURCE
#include "abi.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define LINE_MAX_BYTES 4096
#define ANSWER_MAX     1024

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --socket <path> --model <gguf> --grammar <gbnf> --prompt <txt>\n"
            "          [--state <path>] [--n-ctx N] [--threads N] [--n-predict N]\n"
            "          [--temperature T]\n",
            argv0);
    return 2;
}

static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int reply(int fd, char tag, const char *text)
{
    char line[ANSWER_MAX + 8];
    int n = snprintf(line, sizeof(line), "%c %s\n", tag, text);
    if (n < 0 || (size_t)n >= sizeof(line))
        return -1;
    return write_all(fd, line, (size_t)n);
}

/* Serves one connection until the client closes it or a line is too long. */
static void serve(int fd)
{
    char in[LINE_MAX_BYTES];
    char answer[ANSWER_MAX];
    size_t filled = 0;

    while (!stop_requested) {
        ssize_t n;
        char *nl;
        size_t len;

        n = read(fd, in + filled, sizeof(in) - filled);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (n == 0)
            return;                       /* the client closed, which is normal */
        filled += (size_t)n;
        while ((nl = memchr(in, '\n', filled)) != NULL) {
            len = (size_t)(nl - in);
            in[len] = '\0';
            if (len == 1 && in[0] == 'V') {
                char v[32];
                snprintf(v, sizeof(v), "%u", edgellm_abi_version());
                if (reply(fd, 'V', v) != 0)
                    return;
            } else if (len >= 2 && in[0] == 'D' && in[1] == ' ') {
                int r = edgellm_abi_decide(in + 2, answer, sizeof(answer));
                if (r >= 0) {
                    if (reply(fd, 'A', answer) != 0)
                        return;
                } else if (reply(fd, 'E', r == -1 ? "no engine open"
                                                  : r == -3 ? "answer too long"
                                                            : "engine error") != 0) {
                    return;
                }
            } else if (reply(fd, 'E', "expected V or D <user text>") != 0) {
                return;
            }
            memmove(in, nl + 1, filled - len - 1);
            filled -= len + 1;
        }
        if (filled == sizeof(in)) {
            reply(fd, 'E', "request line longer than the buffer");
            return;                       /* never split one line into two requests */
        }
    }
}

int main(int argc, char **argv)
{
    const char *sock_path = NULL, *model = NULL, *grammar = NULL, *prompt = NULL;
    const char *state = NULL;
    int n_ctx = 0, n_threads = 0, n_predict = 0, i, rc = 0, lfd = -1;
    float temperature = 0.0f;
    struct sockaddr_un addr;
    struct sigaction sa;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (v == NULL)
            return usage(argv[0]);
        if (strcmp(a, "--socket") == 0) sock_path = v;
        else if (strcmp(a, "--model") == 0) model = v;
        else if (strcmp(a, "--grammar") == 0) grammar = v;
        else if (strcmp(a, "--prompt") == 0) prompt = v;
        else if (strcmp(a, "--state") == 0) state = v;
        else if (strcmp(a, "--n-ctx") == 0) n_ctx = atoi(v);
        else if (strcmp(a, "--threads") == 0) n_threads = atoi(v);
        else if (strcmp(a, "--n-predict") == 0) n_predict = atoi(v);
        else if (strcmp(a, "--temperature") == 0) temperature = (float)atof(v);
        else return usage(argv[0]);
        i++;
    }
    if (sock_path == NULL || grammar == NULL || prompt == NULL)
        return usage(argv[0]);
    if (strlen(sock_path) >= sizeof(addr.sun_path)) {
        fprintf(stderr, "%s: socket path longer than %zu bytes\n", argv[0],
                sizeof(addr.sun_path) - 1);
        return 2;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    if (edgellm_abi_open(model, grammar, prompt, state, n_ctx, n_threads, n_predict,
                         temperature) != 0) {
        fprintf(stderr, "%s: the engine did not open\n", argv[0]);
        return 1;
    }

    lfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (lfd < 0) {
        perror("socket");
        edgellm_abi_close();
        return 1;
    }
    /* A socket file left by a process stopped with SIGKILL would make bind fail
     * with EADDRINUSE; the unit uses RuntimeDirectory, so the path is ours. */
    unlink(sock_path);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path);
    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(lfd, 4) != 0) {
        perror("bind or listen");
        close(lfd);
        edgellm_abi_close();
        return 1;
    }
    /* The service runs as another user in the unit; the group owns the socket. */
    chmod(sock_path, 0660);
    fprintf(stderr, "%s: abi version %u, listening on %s\n", argv[0],
            edgellm_abi_version(), sock_path);

    while (!stop_requested) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            rc = 1;
            break;
        }
        serve(cfd);
        close(cfd);
    }
    close(lfd);
    unlink(sock_path);
    edgellm_abi_close();
    return rc;
}
