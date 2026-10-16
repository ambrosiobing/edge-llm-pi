/* io.h: the one boundary between a project's loop and the hardware.
 *
 * A project opens exactly one io_source: a replayed CSV during development and in CI,
 * or the live device on the bench. The loop is identical in both cases, which is what
 * "ready for board wiring" means. Each backend lives in its own file under
 * common/c/src/io_*.c and is compiled in only where the project asks for it, so a
 * build on the host never needs libgpiod or SocketCAN headers unless the project does.
 *
 * Every event carries a monotonic timestamp, a tag naming the sensor or channel, and
 * up to IO_MAX_VALUES numeric values whose meaning the project's README fixes.
 */
#ifndef EDGELLM_IO_H
#define EDGELLM_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IO_MAX_VALUES 16
#define IO_TAG_LEN    32

typedef struct io_event {
    double t_s;                   /* seconds, CLOCK_MONOTONIC live, the CSV column on replay */
    char   tag[IO_TAG_LEN];       /* "adxl345", "vl53l4cd", "can0", "ttyUSB0", ... */
    int    n;                     /* how many of v[] are valid */
    double v[IO_MAX_VALUES];
} io_event;

typedef struct io_source io_source;

/* replay: a CSV with a header line "t_s,tag,v0,v1,...". Returns NULL on error. */
io_source *io_open_replay(const char *csv_path, char *err, size_t err_len);

/* live: a backend name and its spec, for example
 *   "iio",    "/sys/bus/iio/devices/iio:device0"
 *   "i2c",    "/dev/i2c-1:0x53"
 *   "gpio",   "/dev/gpiochip0:17,27,22"
 *   "serial", "/dev/serial/by-id/usb-...:115200"
 *   "can",    "can0"
 *   "sysfs",  "/sys/class/thermal/thermal_zone0/temp"
 * Only the backends compiled into this binary are accepted; others return NULL. */
io_source *io_open_live(const char *backend, const char *spec, char *err, size_t err_len);

/* Blocks until the next event. Returns 1 on an event, 0 at end of replay, -1 on error. */
int io_next(io_source *s, io_event *ev);

/* An output the rule, never the model, drives: for example an LED line. Returns 0 or -1. */
int io_actuate(io_source *s, const char *what, int value);

const char *io_backend_name(const io_source *s);
void io_close(io_source *s);

#ifdef __cplusplus
}
#endif
#endif /* EDGELLM_IO_H */
