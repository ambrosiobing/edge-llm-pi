/* rules_e02.c: the control rule of project e02, the decision loop. Two thresholds on a
 * synthetic two-value event: v0 is a level in arbitrary units from 0 to 100, v1 is its
 * slope in units per second. The rule is what the agreement table measures the model
 * against; the rule acts, the model only proposes.
 *
 * Index 0 is NORMAL, 1 is WATCH, 2 is ALERT. The names below are the grammar's literals
 * letter for letter, see grammars/e02_decision.gbnf; the oracle test and the agreement
 * table depend on that equality.
 */
#include "rules.h"

static const char *const rule_e02_names[] = { "NORMAL", "WATCH", "ALERT", NULL };

static int rule_e02_fn(const rule_ctx *ctx, const io_event *ev)
{
    (void)ctx;                                        /* two values decide; no history */
    if (ev->n < 2) return 0;                          /* a short event is NORMAL, not a guess */
    if (ev->v[0] > 80.0 || ev->v[1] > 10.0) return 2; /* ALERT: above 80, or slope above 10 */
    if (ev->v[0] > 60.0) return 1;                    /* WATCH: level above 60 */
    return 0;                                         /* NORMAL */
}

/* The constructor below registers the rule before main() runs; rule_find("e02") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e02 = { "e02", rule_e02_fn, rule_e02_names };
RULE_REGISTER(rule_e02);
