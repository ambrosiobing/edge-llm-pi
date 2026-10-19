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
 *
 * WHAT IT COSTS, MEASURED AND NOT ESTIMATED, so nobody has to measure it again. On the bench
 * Raspberry Pi 4 Model B on Monday 19 October 2026, writing to ext4 on the SD card, over two
 * hundred rows per loop with a control loop that flushed without syncing:
 *
 *     flush only      0.005 ms per row
 *     flush + fsync   6.599 ms per row
 *     the sync        6.594 ms per row
 *
 * That is 0.233 percent of a 2827 ms decision, and about 6.6 seconds added to the
 * eighty-five minute agreement run. It is also roughly thirteen hundred times the cost of the
 * fflush that was here before, which is the honest way to read the pair: the old call was
 * nearly free and bought nothing against a power loss, and the new one costs six milliseconds
 * and buys the rows. tools/fsync_cost.py takes the measurement and refuses to take it on a
 * tmpfs, where fsync reaches no device and would read as free.
 *
 * THE SECOND WAY A RUN WAS LOST was two processes writing one log, and the guard against
 * it sits in elog_open below with the reasoning for its exact shape. Durability and
 * exclusivity are separate concerns and they failed on separate days.
 */
#define _GNU_SOURCE
#include "elog.h"

#include <errno.h>
#include <fcntl.h>    /* open, O_CREAT, O_CLOEXEC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h> /* flock */
#include <unistd.h>   /* fsync, close, ftruncate; fileno is POSIX, _GNU_SOURCE exposes it */

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

/* ONE WRITER PER LOG, AND THE ORDER OF THE TWO CALLS BELOW IS THE WHOLE GUARD.
 *
 * What this prevents, which cost a run. The launch command for a long run is one line and
 * can be pasted twice. On Sunday 18 October 2026 it was: two engines started against
 * projects/e02-decision-loop/evidence/e02_run.csv at two threads each, the second
 * truncated what the first had written, and both then wrote through their own file
 * offsets into the one file. The pair had to be stopped, the log deleted and the board
 * restarted, and the run taken again from the beginning. The remedy at the time was a
 * pgrep line added to that project's hand-over sheet, which is a named pitfall and not a
 * gate: it protects the one sheet it is written on, and only while whoever reads it runs
 * the whole line. An advisory lock protects every caller of this function without being
 * read.
 *
 * LOCK FIRST, TRUNCATE SECOND. The file is opened without O_TRUNC, the lock is taken, and
 * only then is the file emptied with ftruncate. Written the obvious way round, with
 * O_TRUNC on the open or fopen(csv_path, "w"), the kernel empties the first writer's file
 * before this process finds out it may not have it, so the refusal would arrive after the
 * damage rather than instead of it. The lock is released by the kernel when the process
 * ends however it ends, including kill -9 and a power loss, so it cannot strand a path.
 *
 * WHAT THIS DOES NOT DO, stated because the alternative was tried and rejected. It does
 * not refuse a log left by a run that has FINISHED: a deliberate second run on the same
 * path still replaces it. Refusing that as well, with O_CREAT|O_EXCL, was written first
 * and then taken out, because 21 of this volume's systemd units carry Restart= with a
 * fixed --log path and none of them could have restarted, and e08's chapter demonstrates
 * its console surviving kill -9. A guard that stops the failure that happened is worth
 * more than one that also stops a second failure and breaks every restarting service.
 * What covers the finished-log case instead: the logs that figures are quoted from are
 * committed the same day, which is what lets tools/verify_e02_figures.py recount them,
 * and tools/log_tail.py reports damage when something else goes wrong.
 */
elog *elog_open(const char *csv_path, const char *engine_version)
{
    elog *l;
    FILE *fp;
    int fd;
    if (csv_path == NULL)
        return NULL;
    /* No O_TRUNC here on purpose; see the paragraph above. 0644 is what fopen's "w" gave
     * under the bench umask of 022, so the mode on the card does not change. */
    fd = open(csv_path, O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) {
        fprintf(stderr, "elog: cannot open %s: %s\n", csv_path, strerror(errno));
        return NULL;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK)
            fprintf(stderr, "elog: %s is being written by another process, so this one "
                            "will not start: do not run two on one log\n", csv_path);
        else
            fprintf(stderr, "elog: cannot lock %s: %s\n", csv_path, strerror(errno));
        close(fd);
        return NULL;
    }
    /* The lock is held, so emptying the file now can only discard this caller's own
     * earlier run and never a live one's rows. */
    if (ftruncate(fd, 0) != 0) {
        fprintf(stderr, "elog: cannot empty %s: %s\n", csv_path, strerror(errno));
        close(fd);
        return NULL;
    }
    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        return NULL;
    }
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
