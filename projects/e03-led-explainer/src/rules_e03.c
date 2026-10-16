/* rules_e03.c: the control rule of project e03, the LED status explainer. The same two
 * thresholds as e02 on the same two-value event, v0 a level from 0 to 100 and v1 its
 * slope in units per second, but the names are the three LED colours, because here the
 * rule's index is what lights a line: index 0 lights GREEN on BCM 17, 1 lights YELLOW on
 * BCM 27, 2 lights RED on BCM 22 (see e03_main.c, project_actuate). The model never
 * sees this function and never reaches a line; it writes the sentence that explains the
 * colour, and projects/e03-led-explainer/tools/reason_check.py scores that sentence.
 *
 * The names below are the grammar's literals letter for letter, see
 * grammars/e03_colour.gbnf; the oracle test and the agreement table depend on that
 * equality. GREEN is first because the stub answers the first literal in CI.
 */
#include "rules.h"

static const char *const rule_e03_names[] = { "GREEN", "YELLOW", "RED", NULL };

static int rule_e03_fn(const rule_ctx *ctx, const io_event *ev)
{
    (void)ctx;                                        /* two values decide; no history */
    if (ev->n < 2) return 0;                          /* a short event lights GREEN */
    if (ev->v[0] > 80.0 || ev->v[1] > 10.0) return 2; /* RED: above 80, or slope above 10 */
    if (ev->v[0] > 60.0) return 1;                    /* YELLOW: level above 60 */
    return 0;                                         /* GREEN */
}

/* The constructor below registers the rule before main() runs; rule_find("e03") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e03 = { "e03", rule_e03_fn, rule_e03_names };
RULE_REGISTER(rule_e03);
