/* e06_main.c: project e06 on the shared main, common/c/src/edgellm_main.c, with one thing
 * the shared main cannot do for it: a live source that io.h has no registration hook for.
 *
 * With --replay the file goes straight to edgellm_main(), as in e02. With
 * --live vl53l4cd <spec> this main forks a sampler child that runs io_vl53l4cd_stream()
 * (the ST ULD over i2c-dev) and writes CSV rows into a pipe, then calls edgellm_main()
 * with --replay /dev/fd/<read end> in place of the three live arguments. The loop, the
 * rule, the logger and the oracle are untouched; the sampler is the only new process. A
 * binary configured without EDGELLM_VL53L4CD_ULD_DIR refuses --live with the reason.
 *
 * The loop is synchronous: while the model answers (seconds on the Pi 3), the sampler keeps
 * writing ten rows a second into the pipe. The rule runs on every one of those rows when
 * the loop catches up, so every sample gets a rule verdict with its own timestamp; the
 * model is asked only on a sample that is still fresh when the loop reaches it, so that
 * its answer is about what is in the beam now and never about a queue. A row the model
 * was not asked about has an empty model column, which tools/agreement.py counts
 * separately and never as agreement.
 *
 * project_actuate prints the rule's verdict per sample to standard output, before the
 * model is asked; that line is what the video shows beside the log. Nothing is driven:
 * there is no actuator in e06, and the model's answer goes only to the log.
 */
#define _GNU_SOURCE
#include "edgellm_main.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef EDGELLM_HAVE_VL53L4CD
int io_vl53l4cd_stream(const char *spec, int fd, volatile sig_atomic_t *stop);
#endif

static double freshness_s;                /* live only; 0 means ask the model on every row */

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static double range_of(const io_event *e)
{
    return (e->n >= 2 && e->v[1] == 0.0) ? e->v[0] : 0.0;   /* 0 means no valid range */
}

const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    char hist[160] = "";
    int  k = 0, i;
    if (freshness_s > 0.0 && now_s() - ev->t_s > freshness_s)
        return NULL;                                   /* stale: the rule alone, no model */
    for (i = (ctx->n > 9 ? ctx->n - 9 : 0); i < ctx->n && k < (int)sizeof hist - 8; i++)
        k += snprintf(hist + k, sizeof hist - (size_t)k, "%.0f, ", range_of(&ctx->hist[i]));
    snprintf(buf, len, "ranges mm, oldest first: %s%.0f. Decide.", hist, range_of(ev));
    return buf;
}

int project_actuate(io_source *src, const rule_desc *rule, int rule_index, const io_event *ev)
{
    (void)src;
    printf("%.3f %4.0f mm status %.0f %s\n", ev->t_s, ev->n > 0 ? ev->v[0] : 0.0,
           ev->n > 1 ? ev->v[1] : 0.0, rule->names[rule_index]);
    fflush(stdout);
    return 0;
}

static volatile sig_atomic_t sampler_stop;

static void sampler_signal(int sig)
{
    (void)sig;
    sampler_stop = 1;
}

int main(int argc, char **argv)
{
    int i, live_at = -1;
    for (i = 1; i + 2 < argc; i++)
        if (strcmp(argv[i], "--live") == 0 && strcmp(argv[i + 1], "vl53l4cd") == 0)
            live_at = i;
    if (live_at < 0)
        return edgellm_main(argc, argv);
#ifndef EDGELLM_HAVE_VL53L4CD
    fprintf(stderr, "%s: built without the ST ULD; configure with "
                    "-DEDGELLM_VL53L4CD_ULD_DIR=<folder of the unpacked STSW-IMG026>\n", argv[0]);
    return 2;
#else
    {
        const char *spec = argv[live_at + 2], *p = strstr(spec, "interval=");
        double interval = p != NULL ? atof(p + 9) : 0.1;
        char path[32], **args;
        int fds[2], rc, status, k = 0;
        pid_t pid;
        struct sigaction sa;

        freshness_s = 2.5 * (interval > 0.0 ? interval : 0.05);
        if (pipe(fds) != 0) {
            perror("pipe");
            return 1;
        }
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = sampler_signal;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGTERM, &sa, NULL);
            close(fds[0]);
            rc = io_vl53l4cd_stream(spec, fds[1], &sampler_stop);
            close(fds[1]);
            _exit(rc);
        }
        close(fds[1]);
        snprintf(path, sizeof(path), "/dev/fd/%d", fds[0]);
        args = calloc((size_t)argc + 1, sizeof(char *));
        if (args == NULL)
            return 1;
        for (i = 0; i < argc; i++) {
            if (i == live_at) {
                args[k++] = "--replay";
                args[k++] = path;
                i += 2;
            } else {
                args[k++] = argv[i];
            }
        }
        rc = edgellm_main(k, args);
        kill(pid, SIGTERM);
        waitpid(pid, &status, 0);
        close(fds[0]);
        free(args);
        if (rc == 0 && WIFEXITED(status) && WEXITSTATUS(status) != 0)
            rc = WEXITSTATUS(status);
        return rc;
    }
#endif
}
