/* rules_e12.c: the control rule of project e12, the STWIN.box vibration band reporter.
 * One ratio on one window: v5 is the share of the watched band (B1, 100 to 500 Hz) in the
 * total energy of the four bands, v6 is the baseline share averaged over the first fifty
 * windows by the loop, 0.0 while that average is still being taken. The rule is what the
 * agreement table measures the model against; the rule acts, the model only reports.
 *
 * Index 0 is BASELINE, 1 is BAND_SHIFT. The names below are the grammar's literals letter
 * for letter, see grammars/e12_band.gbnf; the oracle test and the agreement table depend
 * on that equality. The baseline travels inside the event because RULE_HISTORY holds
 * sixteen events and the baseline is fifty windows long.
 */
#include "rules.h"

static const char *const rule_e12_names[] = { "BASELINE", "BAND_SHIFT", NULL };

static int rule_e12_fn(const rule_ctx *ctx, const io_event *ev)
{
    (void)ctx;                                   /* the baseline travels in the event */
    if (ev->n < 7) return 0;                     /* a short event is BASELINE, not a guess */
    if (ev->v[6] <= 0.0) return 0;               /* the baseline is still being taken */
    if (ev->v[5] > 2.0 * ev->v[6]) return 1;     /* BAND_SHIFT: share above twice baseline */
    return 0;                                    /* BASELINE */
}

/* The constructor below registers the rule before main() runs; rule_find("e12") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e12 = { "e12", rule_e12_fn, rule_e12_names };
RULE_REGISTER(rule_e12);
