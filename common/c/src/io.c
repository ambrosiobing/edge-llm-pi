/* io.c: the dispatcher behind io.h and the helpers every backend shares.
 *
 * Which live backends exist in a binary is decided at configure time by the CMake
 * options EDGELLM_IO_GPIO, EDGELLM_IO_I2C, EDGELLM_IO_IIO, EDGELLM_IO_SERIAL and
 * EDGELLM_IO_CAN. A backend that was not compiled in is reported by name, so a bench
 * run that forgot the option fails with the reason and not with a missing device.
 * The replay and sysfs backends need no library and are always present.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int io_seterr(char *err, size_t err_len, const char *fmt, ...)
{
    va_list ap;
    if (err == NULL || err_len == 0)
        return -1;
    va_start(ap, fmt);
    vsnprintf(err, err_len, fmt, ap);
    va_end(ap);
    return -1;
}

double io_now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void io_sleep_s(double seconds)
{
    struct timespec ts;
    if (seconds <= 0.0)
        return;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) != 0)
        ;
}

void io_split_interval(const char *spec, char *path_out, size_t path_len, double *interval_out)
{
    const char *colon = strrchr(spec, ':');
    size_t n = strlen(spec);
    if (colon != NULL && colon[1] != '\0') {
        char *end = NULL;
        double v = strtod(colon + 1, &end);
        if (end != NULL && *end == '\0' && v > 0.0) {
            *interval_out = v;
            n = (size_t)(colon - spec);
        }
    }
    if (n >= path_len)
        n = path_len - 1;
    memcpy(path_out, spec, n);
    path_out[n] = '\0';
}

void io_event_init(io_event *ev, const char *tag)
{
    memset(ev, 0, sizeof(*ev));
    ev->t_s = io_now_s();
    snprintf(ev->tag, sizeof(ev->tag), "%s", tag);
}

io_source *io_open_replay(const char *csv_path, char *err, size_t err_len)
{
    if (csv_path == NULL)
        return io_seterr(err, err_len, "replay: no path"), NULL;
    return io_replay_open(csv_path, err, err_len);
}

io_source *io_open_live(const char *backend, const char *spec, char *err, size_t err_len)
{
    if (backend == NULL || spec == NULL)
        return io_seterr(err, err_len, "live: backend and spec are both required"), NULL;
    if (strcmp(backend, "sysfs") == 0)
        return io_sysfs_open(spec, err, err_len);
    if (strcmp(backend, "gpio") == 0) {
#ifdef EDGELLM_IO_GPIO
        return io_gpio_open(spec, err, err_len);
#else
        return io_seterr(err, err_len, "gpio: backend not compiled in (EDGELLM_IO_GPIO)"), NULL;
#endif
    }
    if (strcmp(backend, "i2c") == 0) {
#ifdef EDGELLM_IO_I2C
        return io_i2c_open(spec, err, err_len);
#else
        return io_seterr(err, err_len, "i2c: backend not compiled in (EDGELLM_IO_I2C)"), NULL;
#endif
    }
    if (strcmp(backend, "iio") == 0) {
#ifdef EDGELLM_IO_IIO
        return io_iio_open(spec, err, err_len);
#else
        return io_seterr(err, err_len, "iio: backend not compiled in (EDGELLM_IO_IIO)"), NULL;
#endif
    }
    if (strcmp(backend, "serial") == 0) {
#ifdef EDGELLM_IO_SERIAL
        return io_serial_open(spec, err, err_len);
#else
        return io_seterr(err, err_len, "serial: backend not compiled in (EDGELLM_IO_SERIAL)"),
               NULL;
#endif
    }
    if (strcmp(backend, "can") == 0) {
#ifdef EDGELLM_IO_CAN
        return io_can_open(spec, err, err_len);
#else
        return io_seterr(err, err_len, "can: backend not compiled in (EDGELLM_IO_CAN)"), NULL;
#endif
    }
    return io_seterr(err, err_len, "live: unknown backend '%s'", backend), NULL;
}

int io_next(io_source *s, io_event *ev)
{
    if (s == NULL || ev == NULL || s->next == NULL)
        return -1;
    return s->next(s, ev);
}

int io_actuate(io_source *s, const char *what, int value)
{
    if (s == NULL || what == NULL)
        return -1;
    if (s->actuate == NULL)
        return -1;
    return s->actuate(s, what, value);
}

const char *io_backend_name(const io_source *s)
{
    return s == NULL ? "none" : s->name;
}

void io_close(io_source *s)
{
    if (s == NULL)
        return;
    if (s->close != NULL)
        s->close(s);
    free(s);
}
