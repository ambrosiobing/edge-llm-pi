/* elog.c: the per-event CSV, one row per event, flushed and synced after every row so
 * that neither a killed process nor a board that loses power takes finished decisions
 * with it.
 *
 * Columns: t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine
 * raw joins the values with ';'. The model column is quoted CSV-style (doubled inner
 * quotes, newlines replaced by spaces) because a sentence may carry commas.
 *
 * THE TWO CALLS BUY DIFFERENT THINGS AND THE DIFFERENCE COST A RUN. fflush moves bytes
 * out of this process into the kernel, which is enough for kill -9: the process is gone
 * and the kernel still writes the page. It is not enough for a board that restarts,
 * because the page may never have reached the card. On Saturday 17 October 2026 the
 * thousand-row run at a generation budget of 12 was interrupted on the bench Raspberry
 * Pi 4 Model B and left
 * projects/e02-decision-loop/evidence/e02_run_npredict12.csv at 13645 bytes of which the
 * last 562 are NUL: the size was recorded and the content never reached the card, so
 * about five finished rows were lost and 117 remained. fsync is what closes that, and
 * tools/log_tail.py is what detects it when something else goes wrong.
 *
 * The sync sits after the decision, not inside it, so it cannot move prompt_ms or
 * generate_ms. Decided by Joseph on Monday 19 October 2026 for the eighty-five minute
 * agreement run, which is the longest in this volume and the one whose finished rows
 * must survive an interruption.
 */
#define _GNU_SOURCE
#include "elog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* fsync, and fileno is POSIX; _GNU_SOURCE above exposes both */

struct elog {
    FILE *fp;
    char  engine[64];
};

/* Flush this process's buffer and then require the kernel to put it on the device.
 * Returns 0 only when both succeeded, because a row that is not durable is a row the
 * caller should be told about rather than one it should assume.
 */
static int flush_and_sync(FILE *fp)
{
    if (fflush(fp) != 0)
        return -1;
    return fsync(fileno(fp)) == 0 ? 0 : -1;
}

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
    if (flush_and_sync(fp) != 0) {
        fclose(fp);
        free(l);
        return NULL;
    }
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
    return flush_and_sync(l->fp);
}

void elog_close(elog *l)
{
    if (l == NULL)
        return;
    if (l->fp != NULL)
        fclose(l->fp);
    free(l);
}
