/* rules_e06.c: the control rule of project e06, the VL53L4CD "too close" explainer. One
 * event per range sample: v0 is the distance in mm, v1 the sensor's range status, where
 * only status 0 is a valid range (every other value is a sample with no distance in it).
 * The rule is what the agreement table measures the model against; the rule acts, the
 * model only proposes.
 *
 * Index 0 is CLEAR, 1 is APPROACHING, 2 is TOO_CLOSE. The names below are the grammar's
 * literals letter for letter, see grammars/e06_range.gbnf; the oracle test and the
 * agreement table depend on that equality. The history the loop keeps holds the earlier
 * samples oldest first; the current sample is not in it yet.
 */
#include "rules.h"

static const char *const rule_e06_names[] = { "CLEAR", "APPROACHING", "TOO_CLOSE", NULL };

static int is_range(const io_event *e)
{
    return e->n >= 2 && e->v[1] == 0.0;                /* status 0 is the only valid range */
}

static int rule_e06_fn(const rule_ctx *ctx, const io_event *ev)
{
    const io_event *prev = ctx->n > 0 ? &ctx->hist[ctx->n - 1] : NULL;
    int i, falling = ctx->n >= 4;                      /* four earlier samples plus this one */
    if (!is_range(ev)) return 0;                       /* no range in this sample: CLEAR */
    if (ev->v[0] < 300.0 && prev != NULL && is_range(prev) && prev->v[0] < 300.0)
        return 2;                                      /* TOO_CLOSE: below 300 mm twice running */
    for (i = ctx->n - 4; falling && i < ctx->n; i++) { /* APPROACHING: five ranges, each lower */
        const io_event *next = (i + 1 < ctx->n) ? &ctx->hist[i + 1] : ev;
        falling = is_range(&ctx->hist[i]) && next->v[0] < ctx->hist[i].v[0];
    }
    return falling ? 1 : 0;
}

/* The constructor below registers the rule before main() runs; rule_find("e06") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e06 = { "e06", rule_e06_fn, rule_e06_names };
RULE_REGISTER(rule_e06);
