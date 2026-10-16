/* io_serial.c: a line-oriented serial port through termios. Spec "<device>:<baud>",
 * for example "/dev/serial/by-id/usb-STMicroelectronics_...-if02:115200". The split
 * is at the last ':' so a by-id name may carry its own colons.
 *
 * Every received line becomes one event: the line, with CR and LF removed, is the tag
 * (truncated to IO_TAG_LEN - 1 characters) and every number found in it, scanning left
 * to right, lands in v[] in order. A line such as "+CSQ: 18,99" gives v[0] 18 and
 * v[1] 99. io_actuate("write", 0) is not supported; a project that must send AT
 * commands opens the port itself and uses this source for the replies only.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define SERIAL_LINE 512

typedef struct serial_priv {
    int  fd;
    char tag[IO_TAG_LEN];
    char line[SERIAL_LINE];
    size_t fill;
} serial_priv;

static speed_t baud_code(long baud)
{
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default:     return B0;
    }
}

static void parse_numbers(const char *line, io_event *ev)
{
    const char *p = line;
    while (*p != '\0' && ev->n < IO_MAX_VALUES) {
        char *end;
        double v;
        if (!(isdigit((unsigned char)*p) ||
              ((*p == '-' || *p == '+') && isdigit((unsigned char)p[1])))) {
            p++;
            continue;
        }
        v = strtod(p, &end);
        if (end == p) {
            p++;
            continue;
        }
        ev->v[ev->n++] = v;
        p = end;
    }
}

static int serial_next(io_source *s, io_event *ev)
{
    serial_priv *p = (serial_priv *)s->priv;
    for (;;) {
        char c;
        ssize_t r = read(p->fd, &c, 1);
        if (r < 0) {
            fprintf(stderr, "serial: read failed on %s\n", p->tag);
            return -1;
        }
        if (r == 0)
            continue;
        if (c == '\n' || c == '\r') {
            if (p->fill == 0)
                continue;
            p->line[p->fill] = '\0';
            io_event_init(ev, p->line);
            parse_numbers(p->line, ev);
            p->fill = 0;
            return 1;
        }
        if (p->fill < SERIAL_LINE - 1)
            p->line[p->fill++] = c;
    }
}

static void serial_close(io_source *s)
{
    serial_priv *p = (serial_priv *)s->priv;
    if (p == NULL)
        return;
    if (p->fd >= 0)
        close(p->fd);
    free(p);
}

io_source *io_serial_open(const char *spec, char *err, size_t err_len)
{
    char dev[256];
    const char *colon = strrchr(spec, ':');
    long baud;
    speed_t code;
    struct termios tio;
    serial_priv *p;
    io_source *s;

    if (colon == NULL || (baud = strtol(colon + 1, NULL, 10)) <= 0 ||
        (code = baud_code(baud)) == B0)
        return io_seterr(err, err_len, "serial: spec needs '<device>:<baud>', standard baud"),
               NULL;
    snprintf(dev, sizeof(dev), "%.*s", (int)(colon - spec), spec);

    p = calloc(1, sizeof(*p));
    s = calloc(1, sizeof(*s));
    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "serial: out of memory"), NULL;
    }
    p->fd = open(dev, O_RDWR | O_NOCTTY);
    if (p->fd < 0) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "serial: cannot open %s", dev), NULL;
    }
    if (tcgetattr(p->fd, &tio) != 0) {
        close(p->fd);
        free(p);
        free(s);
        return io_seterr(err, err_len, "serial: %s is not a terminal", dev), NULL;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, code);
    cfsetospeed(&tio, code);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(p->fd, TCSANOW, &tio) != 0) {
        close(p->fd);
        free(p);
        free(s);
        return io_seterr(err, err_len, "serial: cannot configure %s", dev), NULL;
    }
    tcflush(p->fd, TCIFLUSH);
    {
        const char *base = strrchr(dev, '/');
        snprintf(p->tag, sizeof(p->tag), "%s", base != NULL ? base + 1 : dev);
    }
    s->name = "serial";
    s->next = serial_next;
    s->actuate = NULL;
    s->close = serial_close;
    s->priv = p;
    return s;
}
