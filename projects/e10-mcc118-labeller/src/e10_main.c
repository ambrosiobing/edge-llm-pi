/* e10_main.c: project e10 on the shared main, common/c/src/edgellm_main.c, which is
 * compiled into this executable with EDGELLM_NO_MAIN defined so that this file can own
 * main(). The shared loop, the ChatML wrapping, the state restore, the grammar sampling
 * and the log are byte for byte those of every other C project.
 *
 * Why this project owns main(): io.h has no hook for a project-local live backend.
 * io_open_live() dispatches by a fixed list of names (sysfs, gpio, i2c, iio, serial,
 * can) and the io_source shape is private to common/c/src. The MCC 118 is reached
 * through the daqhats C library in src/io_mcc118.c, which is this project's file and
 * not a shared backend, and common/ is not this project's to change. So:
 *
 *   --replay <csv>          main() hands argv to edgellm_main() unchanged.
 *   --live mcc118 <spec>    main() forks. The child opens the HAT, scans one channel at
 *                           100 kS/s on the board's own pacer and writes one line per
 *                           2 ms window, in the replay CSV shape, into a pipe. The
 *                           parent replaces the three tokens by --replay /dev/fd/N and
 *                           calls edgellm_main(). The pipe carries one summarised event
 *                           per window, sixteen numbers; the 200 samples of a window
 *                           never leave the child.
 *
 * spec is "<address>:<channel>[:<window_ms>]", default "0:0:2". A binary configured
 * without -DEDGELLM_DAQHATS=ON refuses --live mcc118 with the reason and exit 2, so a
 * host build can never claim the HAT. On the bench the log's tag column reads
 * mcc118:ch0; on a replay it reads the stimulus tag. The backend name in the end-of-run
 * line reads replay in both cases, because to the loop the pipe is a replay; the tag
 * column is what says where a log came from.
 */
#define _GNU_SOURCE
#include "edgellm_main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Defined in src/io_mcc118.c, compiled in only with EDGELLM_DAQHATS. The prototype is
 * repeated here rather than kept in a header because these two files are its only
 * users. Writes windows to fd until the reader closes; returns 0, or 1 on a HAT error. */
int mcc118_capture(int fd, const char *spec);

/* The user text for one window: the three summaries and the thirteen averaged points.
 * The thresholds live in prompt.txt, the system prompt, decoded once at start and
 * restored per decision, so they cost no tokens per window. An event without the
 * summaries is logged with the rule alone (NULL). */
const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    int k, i;
    (void)ctx;                                           /* one window decides; no history */
    if (ev->n < 4)
        return NULL;
    k = snprintf(buf, len, "window: min %.3f V, max %.3f V, steepest change %.3f V between "
                           "two samples, points", ev->v[0], ev->v[1], ev->v[2]);
    for (i = 3; i < ev->n && k > 0 && k < (int)len - 16; i++)
        k += snprintf(buf + k, len - (size_t)k, "%s%.2f", i == 3 ? " " : ", ", ev->v[i]);
    if (k > 0 && k < (int)len - 16)
        snprintf(buf + k, len - (size_t)k, " V. Label.");
    return buf;
}

static int live_index(int argc, char **argv)
{
    int i;
    for (i = 1; i + 2 < argc; i++)
        if (strcmp(argv[i], "--live") == 0 && strcmp(argv[i + 1], "mcc118") == 0)
            return i;
    return -1;
}

int main(int argc, char **argv)
{
    int at = live_index(argc, argv);
    if (at < 0)
        return edgellm_main(argc, argv);
#ifndef EDGELLM_DAQHATS
    fprintf(stderr, "%s: the live MCC 118 source is not compiled in; configure the bench "
                    "build with -DEDGELLM_DAQHATS=ON (libdaqhats from the daqhats "
                    "repository, see wiring.md)\n", argv[0]);
    return 2;
#else
    {
        const char *spec = argv[at + 2];
        int fds[2], status = 0, rc, i, n = 0;
        char path[32];
        char **args;
        pid_t pid;

        if (pipe(fds) != 0) {
            perror("e10: pipe");
            return 1;
        }
        pid = fork();
        if (pid < 0) {
            perror("e10: fork");
            return 1;
        }
        if (pid == 0) {
            close(fds[0]);
            _exit(mcc118_capture(fds[1], spec) == 0 ? 0 : 1);
        }
        close(fds[1]);
        snprintf(path, sizeof(path), "/dev/fd/%d", fds[0]);
        args = calloc((size_t)argc + 1, sizeof(*args));
        if (args == NULL)
            return 1;
        for (i = 0; i < argc; i++) {
            if (i == at) {                      /* --live mcc118 <spec> becomes --replay <pipe> */
                args[n++] = (char *)"--replay";
                args[n++] = path;
                i += 2;
                continue;
            }
            args[n++] = argv[i];
        }
        rc = edgellm_main(n, args);
        close(fds[0]);                   /* the child sees EPIPE on its next window and stops */
        waitpid(pid, &status, 0);
        free(args);
        if (rc == 0 && (!WIFEXITED(status) || WEXITSTATUS(status) != 0))
            rc = 1;                      /* a HAT error ended the capture: not a clean run */
        return rc;
    }
#endif
}
