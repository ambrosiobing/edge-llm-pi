/* rules_e09.c: the control rule of project e09, which is e05's rule written in C: the
 * four event classes of the ADXL345, decided from the current reading and the oldest
 * reading in the loop's 16-sample window. Values are in g, three per event, in the
 * order the IIO backend sorts the attributes: in_accel_x_raw, in_accel_y_raw,
 * in_accel_z_raw. The loop converts the driver's m/s^2 to g before the rule sees them.
 *
 *   TAP       the magnitude of the current reading is above 2 g
 *   FREEFALL  the magnitude is below 0.3 g
 *   TILT      the angle between the current reading and the oldest reading in the
 *             window is above 30 degrees: a change of axis, not a held orientation,
 *             so once the window has caught up with a held tilt the rule says NONE
 *   NONE      otherwise, and for any event with fewer than three values
 *
 * No libm: the shared harness tests/rules_test links edgellm_core alone, so the
 * magnitudes are compared as squares and the angle through its cosine. The square
 * of cos 30 degrees is 0.75. Every comparison is strict, so 2.0 g, 0.3 g and 30
 * degrees themselves stay in the lower band, and the six cases in rules_e09_test.c
 * hold two of those boundaries.
 */
#include "rules.h"

static const char *const rule_e09_names[] = { "NONE", "TAP", "TILT", "FREEFALL", NULL };

static double mag2(const io_event *ev)
{
    return ev->v[0] * ev->v[0] + ev->v[1] * ev->v[1] + ev->v[2] * ev->v[2];
}

static int rule_e09_fn(const rule_ctx *ctx, const io_event *ev)
{
    const io_event *ref;
    double m2, r2, dot;
    if (ev->n < 3) return 0;                                 /* a short event is NONE */
    m2 = mag2(ev);
    if (m2 > 4.0) return 1;                                  /* TAP: above 2 g */
    if (m2 < 0.09) return 3;                                 /* FREEFALL: below 0.3 g */
    if (ctx->n == 0) return 0;                               /* nothing to compare with */
    ref = &ctx->hist[0];                                     /* the oldest in the window */
    r2 = mag2(ref);
    if (ref->n < 3 || r2 < 0.09) return 0;                   /* no axis in a freefall */
    dot = ev->v[0] * ref->v[0] + ev->v[1] * ref->v[1] + ev->v[2] * ref->v[2];
    if (dot < 0.0 || dot * dot < 0.75 * m2 * r2) return 2;   /* TILT: above 30 degrees */
    return 0;                                                /* NONE */
}

/* The constructor below registers the rule before main() runs; rule_find("e09") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e09 = { "e09", rule_e09_fn, rule_e09_names };
RULE_REGISTER(rule_e09);
