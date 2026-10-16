/* rules_e07.c: the pressure trend of project e07, the IKS4A1 climate and motion
 * narrator. Two comparisons on two numbers of the event: v[0] is the LPS22DF pressure
 * now in hPa, v[1] the pressure 30 minutes earlier in hPa, carried inside the event by
 * the reader's ring buffer in src/main.cpp. The history window of rules.h is 16
 * events, 16 minutes at this project's interval, which is why the old value travels
 * with the event rather than in the history. The names table is what
 * grammars/e07_pressure.gbnf reproduces letter for letter; STABLE is first because it
 * is the quiet state and the stub's answer in CI.
 */
#include "rules.h"

static const char *const rule_e07_names[] = { "STABLE", "RISING", "FALLING", NULL };

static int rule_e07_fn(const rule_ctx *ctx, const io_event *ev)
{
    double d;
    (void)ctx;                                  /* the 30-minute-old value is in the event */
    if (ev->n < 2) return 0;                    /* a short event is STABLE, not a guess */
    d = ev->v[0] - ev->v[1];                    /* hPa over the last 30 minutes */
    if (d > 1.0) return 1;                      /* RISING: more than 1 hPa up */
    if (d < -1.0) return 2;                     /* FALLING: more than 1 hPa down */
    return 0;                                   /* STABLE */
}

/* The constructor registers the rule before main() runs; rule_find("e07") returns it.
 * The file is compiled into the executable and into tests/rules_test, never into an
 * archive. */
const rule_desc rule_e07 = { "e07", rule_e07_fn, rule_e07_names };
RULE_REGISTER(rule_e07);
