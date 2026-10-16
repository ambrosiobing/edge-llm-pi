/* io_internal.h: the private shape behind io.h. Every backend fills one io_source
 * with its three operations and a private pointer; io.c dispatches to them.
 * Nothing outside common/c/src includes this file.
 */
#ifndef EDGELLM_IO_INTERNAL_H
#define EDGELLM_IO_INTERNAL_H

#include "io.h"

struct io_source {
    const char *name;                                             /* "replay", "gpio", ... */
    int  (*next)(io_source *s, io_event *ev);
    int  (*actuate)(io_source *s, const char *what, int value);
    void (*close)(io_source *s);
    void *priv;
};

/* Shared helpers, defined in io.c. */
int    io_seterr(char *err, size_t err_len, const char *fmt, ...);
double io_now_s(void);                       /* CLOCK_MONOTONIC in seconds */
void   io_sleep_s(double seconds);
/* Splits "path:interval" at the last ':' when the tail parses as a number. The path is
 * copied into path_out (path_len bytes); interval_out keeps its old value otherwise. */
void   io_split_interval(const char *spec, char *path_out, size_t path_len, double *interval_out);
/* Fills ev with zeros, the live timestamp and the tag (truncated to IO_TAG_LEN - 1). */
void   io_event_init(io_event *ev, const char *tag);

/* Backend constructors. Each returns NULL and a message on failure. */
io_source *io_replay_open(const char *csv_path, char *err, size_t err_len);
io_source *io_sysfs_open(const char *spec, char *err, size_t err_len);
io_source *io_gpio_open(const char *spec, char *err, size_t err_len);
io_source *io_i2c_open(const char *spec, char *err, size_t err_len);
io_source *io_iio_open(const char *spec, char *err, size_t err_len);
io_source *io_serial_open(const char *spec, char *err, size_t err_len);
io_source *io_can_open(const char *spec, char *err, size_t err_len);

#endif /* EDGELLM_IO_INTERNAL_H */
