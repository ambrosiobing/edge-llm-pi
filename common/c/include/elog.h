/* elog.h: the per-event log every project writes, one row per event, in the one CSV
 * shape tools/agreement.py reads. The rule column and the model column sit side by
 * side so the agreement table can be built without the program's help.
 *
 * Columns: t_s, tag, raw (the values joined by ';'), rule, model, prompt_ms,
 *          generate_ms, n_generated, engine
 */
#ifndef EDGELLM_ELOG_H
#define EDGELLM_ELOG_H

#include "io.h"
#include "edgellm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct elog elog;

/* elog_open TAKES AN EXCLUSIVE LOCK ON THE LOG and returns NULL, with a line on stderr,
 * when another live process already holds it. One writer per log: a launch command pasted
 * twice stops instead of interleaving two runs into one file. It still replaces a log
 * left by a run that has finished, so a restarting service reopens its own log normally;
 * the lock is taken before the file is emptied, so a refused start destroys nothing. The
 * reason, the run it cost and what it deliberately does not cover are in elog.c.
 */
elog *elog_open(const char *csv_path, const char *engine_version);
int   elog_row(elog *l, const io_event *ev, const char *rule, const edgellm_result *model);
void  elog_close(elog *l);

#ifdef __cplusplus
}
#endif
#endif /* EDGELLM_ELOG_H */
