/* rules_e11.c: the control rule of project e11, the Nucleo CDC sensor hub narrator. One
 * frame from the hub decides: v1 to v3 are the low-g axes of the ISM6HG256X in g, v4 the
 * high-g channel's peak-hold in g, the largest magnitude seen since the previous frame.
 * v0 is the sequence counter and is not the rule's business; the loop checks it.
 *
 * Index 0 is QUIET, 1 is LOW_G_EVENT, 2 is HIGH_G_EVENT. The names below are the
 * grammar's literals letter for letter, see grammars/e11_impact.gbnf; the oracle test
 * and the agreement table depend on that equality. The magnitude is compared squared so
 * the rule needs no libm and the harness links nothing extra.
 */
#include "rules.h"

static const char *const rule_e11_names[] = { "QUIET", "LOW_G_EVENT", "HIGH_G_EVENT", NULL };

static int rule_e11_fn(const rule_ctx *ctx, const io_event *ev)
{
    double m2;
    (void)ctx;                                           /* one frame decides; no history */
    if (ev->n < 5) return 0;                             /* a short frame is QUIET, not a guess */
    if (ev->v[4] > 16.0) return 2;                       /* HIGH_G_EVENT: peak-hold above 16 g */
    m2 = ev->v[1] * ev->v[1] + ev->v[2] * ev->v[2] + ev->v[3] * ev->v[3];
    if (m2 > 4.0) return 1;                              /* LOW_G_EVENT: magnitude above 2 g */
    return 0;                                            /* QUIET */
}

/* The constructor below registers the rule before main() runs; rule_find("e11") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e11 = { "e11", rule_e11_fn, rule_e11_names };
RULE_REGISTER(rule_e11);
