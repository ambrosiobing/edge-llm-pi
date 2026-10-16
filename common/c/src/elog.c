/* elog.c: the per-event CSV, one row per event, flushed after every row so that a
 * process ended with kill -9 leaves every decision it took on disk.
 *
 * Columns: t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine
 * raw joins the values with ';'. The model column is quoted CSV-style (doubled inner
 * quotes, newlines replaced by spaces) because a sentence may carry commas.
 */
#define _GNU_SOURCE
#include "elog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct elog {
    FILE *fp;
    char  engine[64];
};

static void write_quoted(FILE *fp, const char *text)
{
    fputc('"', fp);
    for (; *text != '\0'; text++) {
        if (*text == '"')
            fputs("\"\"", fp);
        else if (*text == '\n' || *text == '\r')
            fputc(' ', fp);
        else
            fputc(*text, fp);
    }
    fputc('"', fp);
}

elog *elog_open(const char *csv_path, const char *engine_version)
{
    elog *l;
    FILE *fp;
    if (csv_path == NULL)
        return NULL;
    fp = fopen(csv_path, "w");
    if (fp == NULL)
        return NULL;
    l = calloc(1, sizeof(*l));
    if (l == NULL) {
        fclose(fp);
        return NULL;
    }
    l->fp = fp;
    snprintf(l->engine, sizeof(l->engine), "%s", engine_version != NULL ? engine_version : "");
    fputs("t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine\n", fp);
    fflush(fp);
    return l;
}

int elog_row(elog *l, const io_event *ev, const char *rule, const edgellm_result *model)
{
    int i;
    if (l == NULL || ev == NULL)
        return -1;
    fprintf(l->fp, "%.6f,", ev->t_s);
    write_quoted(l->fp, ev->tag);
    fputc(',', l->fp);
    for (i = 0; i < ev->n; i++)
        fprintf(l->fp, "%s%.6g", i == 0 ? "" : ";", ev->v[i]);
    fputc(',', l->fp);
    write_quoted(l->fp, rule != NULL ? rule : "");
    fputc(',', l->fp);
    if (model != NULL) {
        write_quoted(l->fp, model->text);
        fprintf(l->fp, ",%.3f,%.3f,%d,", model->prompt_ms, model->generate_ms,
                model->n_generated);
    } else {
        fputs("\"\",,,,", l->fp);
    }
    write_quoted(l->fp, l->engine);
    fputc('\n', l->fp);
    return fflush(l->fp) == 0 ? 0 : -1;
}

void elog_close(elog *l)
{
    if (l == NULL)
        return;
    if (l->fp != NULL)
        fclose(l->fp);
    free(l);
}
