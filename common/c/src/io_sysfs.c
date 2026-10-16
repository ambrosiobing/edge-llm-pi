/* io_sysfs.c: one sysfs attribute read at an interval. Spec "path[:interval_s]", for
 * example "/sys/class/thermal/thermal_zone0/temp:10". The file is reopened for every
 * read because sysfs attributes are generated on open. v[0] is the number the file
 * holds; the tag is the last two components of the path. The first event is returned
 * at once, every later one after the interval (1 s by default). No library needed.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sysfs_priv {
    char   path[256];
    char   tag[IO_TAG_LEN];
    double interval_s;
    double next_due_s;
} sysfs_priv;

static void tag_from_path(const char *path, char *tag, size_t len)
{
    const char *last = strrchr(path, '/');
    const char *start = path;
    if (last != NULL) {
        const char *p = last;
        while (p > path && *(p - 1) != '/')
            p--;
        if (p > path)
            p--;
        while (p > path && *(p - 1) != '/')
            p--;
        start = p;
    }
    snprintf(tag, len, "%s", start);
}

static int sysfs_next(io_source *s, io_event *ev)
{
    sysfs_priv *p = (sysfs_priv *)s->priv;
    char buf[128];
    char *end;
    FILE *fp;
    double now = io_now_s();

    if (now < p->next_due_s)
        io_sleep_s(p->next_due_s - now);
    p->next_due_s = io_now_s() + p->interval_s;

    fp = fopen(p->path, "r");
    if (fp == NULL) {
        fprintf(stderr, "sysfs: cannot open %s\n", p->path);
        return -1;
    }
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        fprintf(stderr, "sysfs: %s is empty\n", p->path);
        return -1;
    }
    fclose(fp);

    io_event_init(ev, p->tag);
    ev->v[0] = strtod(buf, &end);
    if (end == buf) {
        fprintf(stderr, "sysfs: %s does not hold a number\n", p->path);
        return -1;
    }
    ev->n = 1;
    return 1;
}

static void sysfs_close(io_source *s)
{
    free(s->priv);
}

io_source *io_sysfs_open(const char *spec, char *err, size_t err_len)
{
    sysfs_priv *p = calloc(1, sizeof(*p));
    io_source *s = calloc(1, sizeof(*s));
    FILE *fp;

    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "sysfs: out of memory"), NULL;
    }
    p->interval_s = 1.0;
    io_split_interval(spec, p->path, sizeof(p->path), &p->interval_s);
    tag_from_path(p->path, p->tag, sizeof(p->tag));

    fp = fopen(p->path, "r");
    if (fp == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "sysfs: cannot open %s", spec), NULL;
    }
    fclose(fp);

    s->name = "sysfs";
    s->next = sysfs_next;
    s->actuate = NULL;
    s->close = sysfs_close;
    s->priv = p;
    return s;
}
