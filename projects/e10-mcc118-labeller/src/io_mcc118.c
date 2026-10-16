/* io_mcc118.c: the live source of project e10, over the daqhats C library (MIT,
 * https://github.com/mccdaq/daqhats; the C reference at
 * https://mccdaq.github.io/daqhats/c.html and the install page, both read on
 * Wednesday 14 October 2026). This file is project-local: io.h has no hook for a
 * backend outside common/c/src, so e10_main.c runs mcc118_capture() in a child process
 * and feeds the shared loop through a pipe in the replay CSV shape. Compiled in only
 * with -DEDGELLM_DAQHATS=ON (projects/e10-mcc118-labeller/CMakeLists.txt), which the
 * CI stub build does not set; the header is /usr/local/include/daqhats/daqhats.h and
 * the library -ldaqhats after the repository's install.sh.
 *
 * What one event is: a window of window_ms milliseconds of one channel, scanned by the
 * HAT on its own pacer at 100 kS/s per channel (the board maximum), summarised as
 *   v0  the minimum, volts, scaled and calibrated by the library from the EEPROM
 *   v1  the maximum
 *   v2  the steepest change between two neighbouring samples: the slope over one
 *       10 us step, in volts
 *   v3 to v15  thirteen points, each the mean of a run of consecutive samples, in
 *       time order: the most io.h carries (IO_MAX_VALUES is 16), so a 2 ms window of
 *       200 samples is described by 13 points
 * The 200 samples never leave this file. The same arithmetic, in the same order, is in
 * tools/make_replay.py, so a replayed window and a live window describe one shape the
 * same way and the oracle can be computed on the host.
 *
 * Order of calls, from the library's finite_scan example: hat_list (it reads the
 * EEPROM copies under /etc/mcc/hats and never touches the bus, so an open that fails
 * after a successful hat_list is a bus problem, usually a chip select taken by another
 * overlay), mcc118_open, mcc118_serial and mcc118_firmware_version (printed so that
 * every capture names the board), one mcc118_a_in_read as the sanity read,
 * mcc118_a_in_scan_actual_rate, mcc118_a_in_scan_start with OPTS_CONTINUOUS,
 * mcc118_a_in_scan_read once per window, then mcc118_a_in_scan_stop,
 * mcc118_a_in_scan_cleanup and mcc118_close. The last three are not optional: a scan
 * left running meets the next open as a busy board.
 */
#define _GNU_SOURCE
#include "io.h"

#include <daqhats/daqhats.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define E10_RATE_HZ     100000.0                 /* per channel, the board maximum */
#define E10_POINTS      (IO_MAX_VALUES - 3)      /* 13 averaged points after the summaries */
#define E10_MAX_WINDOW  2000                     /* samples: 20 ms at 100 kS/s */
#define E10_SCAN_BUFFER 100000                   /* samples the library buffers: 1 s */
#define E10_READ_TIMEOUT_S 1.0

typedef struct mcc118_src {
    uint8_t  address;
    uint8_t  channel;
    uint32_t window;                             /* samples per event */
    double   actual_rate;                        /* what the board granted, not what was asked */
    double   t0;                                 /* CLOCK_MONOTONIC at scan start */
    uint64_t samples;                            /* samples delivered so far */
    char     tag[IO_TAG_LEN];
    double   buf[E10_MAX_WINDOW];
} mcc118_src;

static volatile sig_atomic_t capture_stop;

static void on_stop(int sig)
{
    (void)sig;
    capture_stop = 1;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* The window summary: minimum, maximum, steepest single-sample change, then the
 * thirteen bin means. Bin k runs from sample k * n / 13 to (k + 1) * n / 13, so 200
 * samples give bins of 15 or 16; tools/make_replay.py uses the same integer bounds. */
static void e10_summarise(const double *s, uint32_t n, io_event *ev)
{
    uint32_t i, k;
    double lo = s[0], hi = s[0], jump = 0.0;
    for (i = 1; i < n; i++) {
        double d = s[i] - s[i - 1];
        if (s[i] < lo) lo = s[i];
        if (s[i] > hi) hi = s[i];
        if (d < 0.0) d = -d;
        if (d > jump) jump = d;
    }
    ev->v[0] = lo;
    ev->v[1] = hi;
    ev->v[2] = jump;
    for (k = 0; k < E10_POINTS; k++) {
        uint32_t a = k * n / E10_POINTS, b = (k + 1) * n / E10_POINTS;
        double sum = 0.0;
        for (i = a; i < b; i++)
            sum += s[i];
        ev->v[3 + k] = (b > a) ? sum / (double)(b - a) : s[n - 1];
    }
    ev->n = 3 + E10_POINTS;
}

static void mcc118_src_close(mcc118_src *p)
{
    if (p == NULL)
        return;
    mcc118_a_in_scan_stop(p->address);
    mcc118_a_in_scan_cleanup(p->address);
    mcc118_close(p->address);
    free(p);
}

static mcc118_src *mcc118_src_open(const char *spec, char *err, size_t err_len)
{
    /* Eight entries because three address jumpers give addresses 0 to 7; that is the
     * HAT's address space, not a library constant. */
    struct HatInfo list[8];
    mcc118_src *p;
    int found, r, address = 0, channel = 0;
    double window_ms = 2.0, value = 0.0;
    uint16_t version = 0, boot = 0;
    char serial[32] = "";

    if (spec != NULL && *spec != '\0' &&
        sscanf(spec, "%d:%d:%lf", &address, &channel, &window_ms) < 2) {
        snprintf(err, err_len, "spec must be <address>:<channel>[:<window_ms>], got %s", spec);
        return NULL;
    }
    if (address < 0 || address > 7 || channel < 0 || channel > 7 || window_ms < 0.1 ||
        window_ms * E10_RATE_HZ / 1000.0 > E10_MAX_WINDOW) {
        snprintf(err, err_len, "address 0 to 7, channel 0 to 7, window 0.1 to 20 ms");
        return NULL;
    }
    found = hat_list(HAT_ID_MCC_118, list);
    if (found < 1) {
        snprintf(err, err_len, "no MCC 118 in /etc/mcc/hats: run sudo daqhats_read_eeproms, "
                               "then ls /dev/spidev0.0");
        return NULL;
    }
    r = mcc118_open((uint8_t)address);
    if (r != RESULT_SUCCESS) {
        snprintf(err, err_len, "mcc118_open(%d): %s (hat_list saw %d board(s); an open that "
                               "fails after that is the bus, see wiring.md)",
                 address, hat_error_message(r), found);
        return NULL;
    }
    p = calloc(1, sizeof(*p));
    if (p == NULL) {
        mcc118_close((uint8_t)address);
        snprintf(err, err_len, "out of memory");
        return NULL;
    }
    p->address = (uint8_t)address;
    p->channel = (uint8_t)channel;
    p->window = (uint32_t)(window_ms * E10_RATE_HZ / 1000.0 + 0.5);
    snprintf(p->tag, sizeof(p->tag), "mcc118:ch%d", channel);

    /* Attribution: the board's serial and firmware go to stderr before any window. */
    mcc118_serial(p->address, serial);
    mcc118_firmware_version(p->address, &version, &boot);
    fprintf(stderr, "mcc118: address %d, serial %s, firmware 0x%04x, boot 0x%04x, %d board(s) "
                    "in /etc/mcc/hats\n", address, serial, version, boot, found);

    /* One single-shot read, scaled and calibrated (options 0): the sanity check that the
     * input is a voltage and not a floating terminal, printed, never used as data. */
    r = mcc118_a_in_read(p->address, p->channel, 0u, &value);
    if (r != RESULT_SUCCESS) {
        mcc118_close(p->address);
        free(p);
        snprintf(err, err_len, "mcc118_a_in_read: %s", hat_error_message(r));
        return NULL;
    }
    fprintf(stderr, "mcc118: single-shot CH%d reads %.4f V before the scan\n", channel, value);

    r = mcc118_a_in_scan_actual_rate(1, E10_RATE_HZ, &p->actual_rate);
    if (r != RESULT_SUCCESS || p->actual_rate <= 0.0)
        p->actual_rate = E10_RATE_HZ;
    r = mcc118_a_in_scan_start(p->address, (uint8_t)(1u << channel), E10_SCAN_BUFFER,
                               E10_RATE_HZ, OPTS_CONTINUOUS);
    if (r != RESULT_SUCCESS) {
        mcc118_close(p->address);
        free(p);
        snprintf(err, err_len, "mcc118_a_in_scan_start: %s", hat_error_message(r));
        return NULL;
    }
    p->t0 = now_s();
    fprintf(stderr, "mcc118: scanning CH%d at %.1f S/s granted (%.1f asked), %u samples per "
                    "window\n", channel, p->actual_rate, E10_RATE_HZ, p->window);
    return p;
}

/* One window. Blocks until the board has delivered window samples or the timeout
 * passes; a hardware or buffer overrun ends the capture, because a window with a hole
 * in it is not a window. The timestamp is the board's own sample count divided by the
 * granted rate, offset to the Pi clock at scan start, so the spacing of events is the
 * HAT's pacer and not the Pi's scheduler. */
static int mcc118_src_next(mcc118_src *p, io_event *ev)
{
    uint16_t status = 0;
    uint32_t got = 0;
    int r = mcc118_a_in_scan_read(p->address, &status, (int32_t)p->window, E10_READ_TIMEOUT_S,
                                  p->buf, p->window, &got);
    if (r != RESULT_SUCCESS) {
        fprintf(stderr, "mcc118: scan read: %s\n", hat_error_message(r));
        return -1;
    }
    if (status & STATUS_HW_OVERRUN) {
        fprintf(stderr, "mcc118: hardware overrun; the capture stops here\n");
        return -1;
    }
    if (status & STATUS_BUFFER_OVERRUN) {
        fprintf(stderr, "mcc118: buffer overrun, the Pi fell behind; the capture stops here\n");
        return -1;
    }
    if (got < p->window) {
        fprintf(stderr, "mcc118: short window, %u of %u samples inside %.1f s\n", got, p->window,
                E10_READ_TIMEOUT_S);
        return -1;
    }
    memset(ev, 0, sizeof(*ev));
    ev->t_s = p->t0 + (double)p->samples / p->actual_rate;
    p->samples += got;
    snprintf(ev->tag, sizeof(ev->tag), "%s", p->tag);
    e10_summarise(p->buf, got, ev);
    return 1;
}

/* The child process of e10_main.c: windows to fd as CSV lines in the replay shape, a
 * header first, until the reader closes the pipe or a signal arrives. */
int mcc118_capture(int fd, const char *spec)
{
    char err[256];
    FILE *out;
    io_event ev;
    mcc118_src *p;
    struct sigaction sa;
    int i, rc = 0;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_stop;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);               /* the reader closing the pipe is the normal end */

    p = mcc118_src_open(spec, err, sizeof(err));
    if (p == NULL) {
        fprintf(stderr, "mcc118: %s\n", err);
        return 1;
    }
    out = fdopen(fd, "w");
    if (out == NULL) {
        mcc118_src_close(p);
        return 1;
    }
    fputs("t_s,tag,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15\n", out);
    while (!capture_stop) {
        if (mcc118_src_next(p, &ev) < 0) {
            rc = 1;
            break;
        }
        fprintf(out, "%.6f,%s", ev.t_s, ev.tag);
        for (i = 0; i < ev.n; i++)
            fprintf(out, ",%.4f", ev.v[i]);
        fputc('\n', out);
        if (fflush(out) != 0)               /* EPIPE: the loop has finished, so stop */
            break;
    }
    fprintf(stderr, "mcc118: %llu samples delivered, scan stopped\n",
            (unsigned long long)p->samples);
    mcc118_src_close(p);
    fclose(out);
    return rc;
}
