/* io_i2c.c: one register block from one I2C device through i2c-dev, at an interval.
 * Spec, segments separated by ':' after the bus device:
 *
 *   "/dev/i2c-1:0x53"                             reads register 0x00, one byte
 *   "/dev/i2c-1:0x53:reg=0x32,len=6:fmt=s16le:interval=0.1"
 *
 * The small table a project configures is this spec: the register, the length, and how
 * the bytes become values. fmt=u8 (default) puts one byte in each v[]; fmt=s16le pairs
 * them little-endian into signed 16-bit values, which is the ADXL345 DATAX0..DATAZ1
 * layout. The tag is "i2c-0x<addr>". The bus speed is the kernel's (100 kHz on the
 * bench leads, see the volume); nothing here changes it.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct i2c_priv {
    int      fd;
    int      addr;
    unsigned reg;
    size_t   len;
    int      s16le;
    char     tag[IO_TAG_LEN];
    double   interval_s;
    double   next_due_s;
} i2c_priv;

static int i2c_next(io_source *s, io_event *ev)
{
    i2c_priv *p = (i2c_priv *)s->priv;
    unsigned char reg = (unsigned char)p->reg;
    unsigned char buf[2 * IO_MAX_VALUES];
    double now = io_now_s();
    size_t i;

    if (now < p->next_due_s)
        io_sleep_s(p->next_due_s - now);
    p->next_due_s = io_now_s() + p->interval_s;

    if (write(p->fd, &reg, 1) != 1) {
        fprintf(stderr, "i2c: %s: register write refused\n", p->tag);
        return -1;
    }
    if (read(p->fd, buf, p->len) != (ssize_t)p->len) {
        fprintf(stderr, "i2c: %s: short read\n", p->tag);
        return -1;
    }
    io_event_init(ev, p->tag);
    if (p->s16le) {
        for (i = 0; i + 1 < p->len; i += 2) {
            int v = (int)buf[i] | ((int)buf[i + 1] << 8);
            if (v >= 32768)
                v -= 65536;
            ev->v[ev->n++] = v;
        }
    } else {
        for (i = 0; i < p->len; i++)
            ev->v[ev->n++] = buf[i];
    }
    return 1;
}

static void i2c_close(io_source *s)
{
    i2c_priv *p = (i2c_priv *)s->priv;
    if (p == NULL)
        return;
    if (p->fd >= 0)
        close(p->fd);
    free(p);
}

io_source *io_i2c_open(const char *spec, char *err, size_t err_len)
{
    char buf[256];
    char *cur = buf, *seg, *end;
    const char *dev, *addr_text;
    i2c_priv *p = calloc(1, sizeof(*p));
    io_source *s = calloc(1, sizeof(*s));

    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "i2c: out of memory"), NULL;
    }
    p->fd = -1;
    p->len = 1;
    p->interval_s = 0.1;
    snprintf(buf, sizeof(buf), "%s", spec);
    dev = strsep(&cur, ":");
    addr_text = strsep(&cur, ":");
    if (addr_text == NULL || (p->addr = (int)strtol(addr_text, &end, 0), *end != '\0') ||
        p->addr < 0x03 || p->addr > 0x77) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "i2c: spec needs '<dev>:<7-bit address>'"), NULL;
    }
    while ((seg = strsep(&cur, ":")) != NULL) {
        char *inner = seg, *kv;
        while ((kv = strsep(&inner, ",")) != NULL) {
            if (strncmp(kv, "reg=", 4) == 0)
                p->reg = (unsigned)strtoul(kv + 4, NULL, 0);
            else if (strncmp(kv, "len=", 4) == 0)
                p->len = (size_t)strtoul(kv + 4, NULL, 0);
            else if (strcmp(kv, "fmt=s16le") == 0)
                p->s16le = 1;
            else if (strcmp(kv, "fmt=u8") == 0)
                p->s16le = 0;
            else if (strncmp(kv, "interval=", 9) == 0)
                p->interval_s = strtod(kv + 9, NULL);
            else {
                free(p);
                free(s);
                return io_seterr(err, err_len, "i2c: unknown spec item '%s'", kv), NULL;
            }
        }
    }
    if (p->len < 1 || p->len > 2 * IO_MAX_VALUES || p->interval_s <= 0.0) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "i2c: len must be 1..%d", 2 * IO_MAX_VALUES), NULL;
    }
    snprintf(p->tag, sizeof(p->tag), "i2c-0x%02x", p->addr);

    p->fd = open(dev, O_RDWR);
    if (p->fd < 0) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "i2c: cannot open %s", dev), NULL;
    }
    if (ioctl(p->fd, I2C_SLAVE, p->addr) < 0) {
        close(p->fd);
        free(p);
        free(s);
        return io_seterr(err, err_len, "i2c: cannot select address 0x%02x", p->addr), NULL;
    }
    s->name = "i2c";
    s->next = i2c_next;
    s->actuate = NULL;
    s->close = i2c_close;
    s->priv = p;
    return s;
}
