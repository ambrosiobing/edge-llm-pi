/* io_iio.c: the sysfs face of an IIO device, polled at an interval. Spec
 * "<device dir>[:interval_s]", for example "/sys/bus/iio/devices/iio:device0:0.1".
 * Because the directory name itself carries a ':', only a tail that parses as a
 * number is taken as the interval.
 *
 * At open the directory is scanned for every "in_*_raw" attribute, sorted by name, up
 * to IO_MAX_VALUES of them. Each read multiplies the raw value by the matching scale:
 * "in_accel_x_scale" when it exists, otherwise "in_accel_scale", otherwise 1. The tag
 * is the device's "name" attribute (for example "adxl345") or the directory basename.
 * The value order is the sorted attribute order; a project's README prints it.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IIO_NAME 64

typedef struct iio_priv {
    char   dir[192];
    char   tag[IO_TAG_LEN];
    char   raw[IO_MAX_VALUES][IIO_NAME];
    double scale[IO_MAX_VALUES];
    int    n;
    double interval_s;
    double next_due_s;
} iio_priv;

static int read_number(const char *dir, const char *attr, double *out)
{
    char path[256], buf[64], *end;
    FILE *fp;
    snprintf(path, sizeof(path), "%s/%s", dir, attr);
    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *out = strtod(buf, &end);
    return end == buf ? -1 : 0;
}

static int is_raw(const struct dirent *d)
{
    size_t n = strlen(d->d_name);
    return strncmp(d->d_name, "in_", 3) == 0 && n > 7 && strcmp(d->d_name + n - 4, "_raw") == 0;
}

static double find_scale(const char *dir, const char *raw)
{
    char attr[IIO_NAME + 8];
    double v;
    size_t n = strlen(raw) - 4;              /* drop "_raw" */
    char *last;

    snprintf(attr, sizeof(attr), "%.*s_scale", (int)n, raw);
    if (read_number(dir, attr, &v) == 0)
        return v;
    /* in_accel_x_raw -> in_accel_scale: remove the last '_' component */
    snprintf(attr, sizeof(attr), "%.*s", (int)n, raw);
    last = strrchr(attr, '_');
    if (last != NULL && last != attr) {
        *last = '\0';
        strcat(attr, "_scale");
        if (read_number(dir, attr, &v) == 0)
            return v;
    }
    return 1.0;
}

static int iio_next(io_source *s, io_event *ev)
{
    iio_priv *p = (iio_priv *)s->priv;
    double now = io_now_s();
    int i;

    if (now < p->next_due_s)
        io_sleep_s(p->next_due_s - now);
    p->next_due_s = io_now_s() + p->interval_s;

    io_event_init(ev, p->tag);
    for (i = 0; i < p->n; i++) {
        double raw;
        if (read_number(p->dir, p->raw[i], &raw) != 0) {
            fprintf(stderr, "iio: cannot read %s/%s\n", p->dir, p->raw[i]);
            return -1;
        }
        ev->v[i] = raw * p->scale[i];
    }
    ev->n = p->n;
    return 1;
}

static void iio_close(io_source *s)
{
    free(s->priv);
}

io_source *io_iio_open(const char *spec, char *err, size_t err_len)
{
    struct dirent **list = NULL;
    int count, i;
    char path[256], buf[64];
    FILE *fp;
    iio_priv *p = calloc(1, sizeof(*p));
    io_source *s = calloc(1, sizeof(*s));

    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "iio: out of memory"), NULL;
    }
    p->interval_s = 0.1;
    io_split_interval(spec, p->dir, sizeof(p->dir), &p->interval_s);

    count = scandir(p->dir, &list, is_raw, alphasort);
    if (count < 0) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "iio: cannot scan %s", spec), NULL;
    }
    for (i = 0; i < count; i++) {
        if (p->n < IO_MAX_VALUES) {
            snprintf(p->raw[p->n], IIO_NAME, "%s", list[i]->d_name);
            p->scale[p->n] = find_scale(p->dir, list[i]->d_name);
            p->n++;
        }
        free(list[i]);
    }
    free(list);
    if (p->n == 0) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "iio: no in_*_raw attribute under %s", p->dir), NULL;
    }

    snprintf(path, sizeof(path), "%s/name", p->dir);
    fp = fopen(path, "r");
    if (fp != NULL && fgets(buf, sizeof(buf), fp) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';
        snprintf(p->tag, sizeof(p->tag), "%s", buf);
    } else {
        const char *base = strrchr(p->dir, '/');
        snprintf(p->tag, sizeof(p->tag), "%s", base != NULL ? base + 1 : p->dir);
    }
    if (fp != NULL)
        fclose(fp);

    s->name = "iio";
    s->next = iio_next;
    s->actuate = NULL;
    s->close = iio_close;
    s->priv = p;
    return s;
}
