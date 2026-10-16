/* e11_main.c: project e11, the Nucleo CDC sensor hub narrator, on the shared main
 * common/c/src/edgellm_main.c. The shared main owns the loop; this file owns three
 * things: the user text, the sequence and rate check on every frame, and the frame
 * layer's attachment for a live run.
 *
 * The live run. `--live cdc <device>:<baud>` is handled here, before the shared main
 * sees the arguments: the port is opened by cbor_frames_open_port, a thread runs
 * cbor_frames_pump, which decodes the hub's CBOR frames into replay-shaped CSV lines on
 * a pipe, and the shared main is started with `--replay /dev/fd/<n>` in place of the
 * three live arguments. The loop is therefore the same code on the bench and on a file;
 * the log's tag column reads "iks5a1" from the frame layer and "replay" from a file, and
 * the engine column names the build, so the three cannot be confused.
 *
 * The counter check. project_actuate is called for every frame with the rule's index,
 * before the prompt is built; e11 drives no output, so the hook is used as the per-frame
 * observer: it compares v0 with the previous frame's v0 plus one, prints every gap as it
 * happens, and keeps the first and last timestamp for the frame rate. At the end main()
 * prints the counts and the rate against the hub's stated 104 Hz. A gap in a live run
 * makes the exit status 3; a gap in a replay is reported and does not, because the
 * replay file carries two on purpose so that this very check is exercised in CI.
 *
 * When the model is asked. Not on every frame: at 104 Hz that would be a hundred
 * decisions a second. The model is asked on a rising edge of the rule's index (QUIET to
 * an event, or LOW_G_EVENT to HIGH_G_EVENT) and on a tick every E11_QUIET_ASK_S seconds
 * on the tick's own clock, so that QUIET rows exist in the agreement table however
 * busy the run. Every other frame logs the rule alone (project_format_prompt returns
 * NULL). tools/make_replay.py applies the same policy when it writes the oracle file.
 */
#define _GNU_SOURCE
#include "edgellm_main.h"
#include "cbor_frames.h"

#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define E11_HUB_RATE_HZ  104.0       /* the hub's stated rate: one SAMPLE per low-g data-ready */
#define E11_QUIET_ASK_S  10.0        /* ask the model this often while nothing happens */
#define E11_HIST_FRAMES  16          /* frames of history the prompt summarises */

static struct e11_state {
    int           have_prev;
    double        prev_seq;
    int           prev_idx;
    int           ask_now;
    int           have_tick;
    double        last_tick_t;
    unsigned long frames, gaps, missing;
    double        t_first, t_last;
    int           live;
} st;

int project_actuate(io_source *src, const rule_desc *rule, int rule_index, const io_event *ev)
{
    (void)src;
    (void)rule;                                  /* e11 drives no output: this is the observer */
    if (ev->n >= 1) {
        if (st.have_prev && ev->v[0] != st.prev_seq + 1.0) {
            double lost = ev->v[0] - st.prev_seq - 1.0;
            st.gaps++;
            if (lost > 0.0)
                st.missing += (unsigned long)lost;
            fprintf(stderr, "e11: sequence gap at t %.6f s: expected %.0f, got %.0f\n", ev->t_s,
                    st.prev_seq + 1.0, ev->v[0]);
        }
        st.prev_seq = ev->v[0];
        st.have_prev = 1;
    }
    if (st.frames == 0)
        st.t_first = ev->t_s;
    st.t_last = ev->t_s;
    st.frames++;
    if (!st.have_tick || ev->t_s - st.last_tick_t >= E11_QUIET_ASK_S) {
        st.have_tick = 1;                        /* the quiet tick, on its own clock */
        st.last_tick_t = ev->t_s;
        st.ask_now = 1;
    } else {
        st.ask_now = rule_index > st.prev_idx;   /* a rising edge: something began */
    }
    st.prev_idx = rule_index;
    return 0;
}

const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    double mag = 0.0, peak = 0.0;
    int i;
    if (!st.ask_now)
        return NULL;                             /* the rule ran alone; no model row */
    st.ask_now = 0;
    for (i = (ctx->n > E11_HIST_FRAMES ? ctx->n - E11_HIST_FRAMES : 0); i < ctx->n; i++)
        if (ctx->hist[i].n > 4 && ctx->hist[i].v[4] > peak)
            peak = ctx->hist[i].v[4];
    if (ev->n > 3)
        mag = sqrt(ev->v[1] * ev->v[1] + ev->v[2] * ev->v[2] + ev->v[3] * ev->v[3]);
    snprintf(buf, len, "frame %.0f: low-g magnitude %.1f g, high-g peak %.1f g, largest peak "
                       "over the previous %d frames %.1f g. Decide and write the line.",
             ev->n > 0 ? ev->v[0] : 0.0, mag, ev->n > 4 ? ev->v[4] : 0.0, E11_HIST_FRAMES, peak);
    return buf;
}

typedef struct pump_args {
    int          fd;
    FILE        *out;
    cbor_frames  frames;
    volatile int stop;
    long         lines;
} pump_args;

static void *pump_thread(void *arg)
{
    pump_args *a = arg;
    a->lines = cbor_frames_pump(a->fd, a->out, &a->frames, stderr, &a->stop);
    fclose(a->out);                              /* end of file for the loop's reader */
    a->out = NULL;
    return NULL;
}

int main(int argc, char **argv)
{
    char **args = argv;
    int n = argc, i, rc, live_at = -1, fds[2] = { -1, -1 };
    char path[32], err[256];
    pthread_t th;
    pump_args pa;

    for (i = 1; i + 2 < argc; i++)
        if (strcmp(argv[i], "--live") == 0 && strcmp(argv[i + 1], "cdc") == 0)
            live_at = i;
    if (live_at >= 0) {
        memset(&pa, 0, sizeof(pa));
        cbor_frames_init(&pa.frames);
        signal(SIGPIPE, SIG_IGN);                /* a reader that left is an EPIPE, not a death */
        pa.fd = cbor_frames_open_port(argv[live_at + 2], err, sizeof(err));
        if (pa.fd < 0) {
            fprintf(stderr, "%s: %s\n", argv[0], err);
            return 1;
        }
        if (pipe(fds) != 0 || (pa.out = fdopen(fds[1], "w")) == NULL) {
            fprintf(stderr, "%s: cannot create the frame pipe\n", argv[0]);
            return 1;
        }
        if (pthread_create(&th, NULL, pump_thread, &pa) != 0) {
            fprintf(stderr, "%s: cannot start the frame layer\n", argv[0]);
            return 1;
        }
        snprintf(path, sizeof(path), "/dev/fd/%d", fds[0]);
        args = calloc((size_t)argc + 1, sizeof(*args));
        if (args == NULL)
            return 1;
        for (i = 0, n = 0; i < argc; i++) {
            if (i == live_at) {
                args[n++] = "--replay";
                args[n++] = path;
                i += 2;                          /* skip "cdc" and the spec */
            } else {
                args[n++] = argv[i];
            }
        }
        st.live = 1;
        fprintf(stderr, "e11: live: frames from %s through the frame layer\n", argv[live_at + 2]);
    }

    rc = edgellm_main(n, args);

    if (st.live) {
        pa.stop = 1;
        pthread_join(th, NULL);
        close(pa.fd);
        cbor_frames_report(&pa.frames.st, stderr);
    }
    fprintf(stderr, "e11: %lu frames, %lu sequence gaps (%lu frames missing), %.2f Hz from the "
                    "timestamps against the hub's stated %.0f Hz, source %s\n",
            st.frames, st.gaps, st.missing,
            st.frames > 1 && st.t_last > st.t_first
                ? (double)(st.frames - 1) / (st.t_last - st.t_first) : 0.0,
            E11_HUB_RATE_HZ, st.live ? "cdc" : "replay");
    if (rc == 0 && st.live && st.gaps > 0) {
        fprintf(stderr, "e11: the CDC stream had gaps: the run fails, the rule does not\n");
        rc = 3;
    }
    if (args != argv)
        free(args);
    return rc;
}
