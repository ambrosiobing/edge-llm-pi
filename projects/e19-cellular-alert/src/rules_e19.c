/* rules_e19.c: the control rule of project e19 and its send gate. Two functions, both
 * deterministic, both tested, and together the only thing that decides whether the
 * board speaks. The model is not an input to either of them.
 *
 * The event is the ADXL345 as the IIO backend delivers it: v0, v1 and v2 are the x, y
 * and z accelerations in metres per second squared, in the sorted order of the
 * in_accel_*_raw attributes, each already multiplied by its scale (see
 * common/c/src/io_iio.c). The classification is the one project e05 uses on the same
 * part, with the same three thresholds: a peak above 2 g is a TAP, a magnitude below
 * 0.3 g is a FREEFALL, a change of attitude of more than 30 degrees against the
 * previous reading is a TILT, and everything else is NONE. The names below are the
 * grammar's two composable literals in their grammar order, with the quiet state
 * first; see grammars/e19_message.gbnf.
 *
 * No libm. Every comparison is a multiplication: the magnitude tests compare squared
 * magnitudes against squared thresholds, and the attitude test compares the squared
 * cosine of the angle between the two vectors against the squared cosine of 30
 * degrees. This is not an optimisation. A rules_eNN.c file is compiled a second time
 * into the shared harness tests/rules_test, whose link line carries edgellm_core and
 * nothing else, so a rule that pulls in a library stops that harness for every
 * project at once. For the same reason this file includes rules.h alone and no header
 * of its own project.
 */
#include "rules.h"

#define E19_G             9.80665    /* metres per second squared per g, the IIO unit */
#define E19_TAP_G         2.0        /* a peak above this is a TAP */
#define E19_FREEFALL_G    0.3        /* a magnitude below this is a FREEFALL */
#define E19_COS30         0.86602540 /* cosine of 30 degrees, the attitude threshold */

/* The minimum interval between two armings, in seconds. It is a constant of the rule
 * and deliberately not a command line option: an interval that can be set from the
 * unit file is an interval that can be set to zero, and the alert storm this gate
 * exists to prevent is then one command away. A longer interval is a change to this
 * file, a new case in rules_e19_test.c and a rebuild. */
#define E19_MIN_SEND_S    10.0

static const char *const rule_e19_names[] = { "NONE", "TAP", "TILT", "FREEFALL", NULL };

/* The squared magnitude of an event's first three values, in (m/s^2)^2. */
static double mag2(const io_event *ev)
{
    return ev->v[0] * ev->v[0] + ev->v[1] * ev->v[1] + ev->v[2] * ev->v[2];
}

/* 1 when the angle between this event's vector and the previous one exceeds 30
 * degrees. cos(angle) = dot / (|a| |b|), so angle > 30 degrees is dot < 0, or
 * dot^2 < cos30^2 |a|^2 |b|^2 with dot positive. A previous reading whose magnitude is
 * below the freefall threshold carries no attitude, so it answers 0 rather than a
 * number computed from noise. */
static int tilted(const rule_ctx *ctx, const io_event *ev)
{
    const io_event *pr;
    double dot, m_now, m_pr, floor2 = (E19_FREEFALL_G * E19_G) * (E19_FREEFALL_G * E19_G);
    if (ctx->n < 1)
        return 0;
    pr = &ctx->hist[ctx->n - 1];
    if (pr->n < 3)
        return 0;
    m_now = mag2(ev);
    m_pr = mag2(pr);
    if (m_now < floor2 || m_pr < floor2)
        return 0;
    dot = ev->v[0] * pr->v[0] + ev->v[1] * pr->v[1] + ev->v[2] * pr->v[2];
    if (dot <= 0.0)
        return 1;
    return dot * dot < (E19_COS30 * E19_COS30) * m_now * m_pr;
}

static int rule_e19_fn(const rule_ctx *ctx, const io_event *ev)
{
    double m2, tap2 = (E19_TAP_G * E19_G) * (E19_TAP_G * E19_G);
    double ff2 = (E19_FREEFALL_G * E19_G) * (E19_FREEFALL_G * E19_G);
    if (ev->n < 3) return 0;                     /* a short event is NONE, not a guess */
    m2 = mag2(ev);
    if (m2 < ff2) return 3;                      /* FREEFALL: magnitude below 0.3 g */
    if (m2 > tap2) return 1;                     /* TAP: peak above 2 g */
    if (tilted(ctx, ev)) return 2;               /* TILT: attitude moved over 30 deg */
    return 0;                                    /* NONE */
}

/* The send gate. The whole send rule of this project: a message may be armed only on
 * the rule's TAP or FREEFALL, and only when at least E19_MIN_SEND_S has passed since
 * the last arming. A negative t_last_arm_s means nothing has been armed yet. Index 0
 * (NONE) and index 2 (TILT) never arm, and no model output is an argument here.
 *
 * The clock counts armings and not successful sends, on purpose. An armed event costs
 * one model call and one modem dialogue whether or not the field checker lets the
 * message through, so a model that writes a bad number on every event would otherwise
 * produce exactly the storm of attempts this gate exists to stop. The cost of the
 * choice is that a refused message consumes the interval, and the refusal rate is
 * published beside the gate in the chapter's agreement table for that reason. */
int rule_e19_sends(int index, double t_s, double t_last_arm_s)
{
    if (index != 1 && index != 3)
        return 0;
    if (t_last_arm_s < 0.0)
        return 1;
    return (t_s - t_last_arm_s) >= E19_MIN_SEND_S;
}

/* The constructor below registers the rule before main() runs; rule_find("e19") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e19 = { "e19", rule_e19_fn, rule_e19_names };
RULE_REGISTER(rule_e19);
