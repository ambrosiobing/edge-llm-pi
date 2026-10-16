/* io_replay.c: a CSV stands in for the hardware. The header line must start with
 * "t_s,tag" and may continue with v0,v1,... ; every later line is one event. Blank
 * lines and lines starting with '#' are skipped. io_next returns 0 at the end of the
 * file, which is how a replay run finishes. io_actuate records the call and returns 0,
 * so a rule's action is accepted here exactly as it is on the bench.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPLAY_LINE 1024

typedef struct replay_priv {
    FILE *fp;
    long  line_no;
    long  actuations;
} replay_priv;

static void strip_eol(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
}

static int replay_next(io_source *s, io_event *ev)
{
    replay_priv *p = (replay_priv *)s->priv;
    char line[REPLAY_LINE];

    for (;;) {
        char *cur, *field, *end;
        if (fgets(line, sizeof(line), p->fp) == NULL)
            return 0;
        p->line_no++;
        strip_eol(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        memset(ev, 0, sizeof(*ev));
        cur = line;

        field = strsep(&cur, ",");
        ev->t_s = strtod(field, &end);
        if (end == field) {
            fprintf(stderr, "replay: line %ld: t_s is not a number\n", p->line_no);
            return -1;
        }
        field = strsep(&cur, ",");
        if (field == NULL) {
            fprintf(stderr, "replay: line %ld: no tag column\n", p->line_no);
            return -1;
        }
        snprintf(ev->tag, sizeof(ev->tag), "%s", field);

        while (cur != NULL && ev->n < IO_MAX_VALUES) {
            field = strsep(&cur, ",");
            if (field[0] == '\0')
                break;
            ev->v[ev->n] = strtod(field, &end);
            if (end == field) {
                fprintf(stderr, "replay: line %ld: v%d is not a number\n", p->line_no, ev->n);
                return -1;
            }
            ev->n++;
        }
        return 1;
    }
}

static int replay_actuate(io_source *s, const char *what, int value)
{
    replay_priv *p = (replay_priv *)s->priv;
    (void)what;
    (void)value;
    p->actuations++;
    return 0;
}

static void replay_close(io_source *s)
{
    replay_priv *p = (replay_priv *)s->priv;
    if (p != NULL) {
        if (p->fp != NULL)
            fclose(p->fp);
        free(p);
    }
}

io_source *io_replay_open(const char *csv_path, char *err, size_t err_len)
{
    char header[REPLAY_LINE];
    replay_priv *p;
    io_source *s;
    FILE *fp = fopen(csv_path, "r");

    if (fp == NULL)
        return io_seterr(err, err_len, "replay: cannot open %s", csv_path), NULL;
    if (fgets(header, sizeof(header), fp) == NULL) {
        fclose(fp);
        return io_seterr(err, err_len, "replay: %s is empty", csv_path), NULL;
    }
    strip_eol(header);
    if (strncmp(header, "t_s,tag", 7) != 0) {
        fclose(fp);
        return io_seterr(err, err_len, "replay: %s header must start with t_s,tag", csv_path),
               NULL;
    }
    p = calloc(1, sizeof(*p));
    s = calloc(1, sizeof(*s));
    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        fclose(fp);
        return io_seterr(err, err_len, "replay: out of memory"), NULL;
    }
    p->fp = fp;
    p->line_no = 1;
    s->name = "replay";
    s->next = replay_next;
    s->actuate = replay_actuate;
    s->close = replay_close;
    s->priv = p;
    return s;
}
