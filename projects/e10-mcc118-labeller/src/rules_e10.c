/* rules_e10.c: the control rule of project e10, the MCC 118 voltage event labeller. One
 * event is one 2 ms window of CH0 at 100 kS/s, summarised by the live source
 * (src/io_mcc118.c) and by the replay generator (tools/make_replay.py) the same way:
 * v0 the minimum in volts, v1 the maximum, v2 the steepest change between two
 * neighbouring samples (the slope over one 10 us step), v3 to v15 thirteen averaged
 * points in time order. The rule reads the three summaries; the points are for the
 * model's description.
 *
 * Index 0 is NOISE, 1 is RAMP, 2 is STEP: the quiet label first, as grammars/README.md
 * asks, so the CI stub answers NOISE. The names are the grammar's literals letter for
 * letter, see grammars/e10_shape.gbnf; the oracle test and the agreement table depend
 * on that equality.
 *
 * The band is a hysteresis of 0.20 V: a window whose swing (maximum minus minimum)
 * stays at or inside it holds no crossing and is NOISE whatever its shape; the HAT's
 * step is about 4.9 mV and the stimulus noise is well inside the band. A crossing is a
 * STEP when the steepest single-sample change exceeds 0.4 of the swing, which a DAC
 * step at 100 kS/s does in one or two samples; otherwise the swing took several
 * samples and the window is a RAMP.
 */
#include "rules.h"

static const char *const rule_e10_names[] = { "NOISE", "RAMP", "STEP", NULL };

static int rule_e10_fn(const rule_ctx *ctx, const io_event *ev)
{
    (void)ctx;                                              /* one window decides */
    if (ev->n < 3) return 0;                                /* no summary: NOISE, not a guess */
    if (ev->v[1] - ev->v[0] <= 0.20) return 0;              /* NOISE: swing inside the band */
    if (ev->v[2] > 0.4 * (ev->v[1] - ev->v[0])) return 2;   /* STEP: most of it in one sample */
    return 1;                                               /* RAMP: a crossing over several */
}

/* The constructor below registers the rule before main() runs; rule_find("e10") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e10 = { "e10", rule_e10_fn, rule_e10_names };
RULE_REGISTER(rule_e10);
