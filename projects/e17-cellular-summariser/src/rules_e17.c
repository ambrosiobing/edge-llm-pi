/* rules_e17.c: the control rule of project e17, the cellular status summariser. One
 * status sample decides, with no history: v0 is the signal quality from AT+CSQ (0 to
 * 31, or 99 for unknown), v1 its bit error rate, v2 the registration state from
 * AT+CEREG? (1 home, 5 roaming, anything else not registered), v3 the access
 * technology (7 Cat-M1, 9 NB-IoT) and v4 the power saving mode. The rule reads v0 and
 * v2 and nothing else; the operator name travels in the event's tag and is for the
 * report and the hallucination count, never for the decision.
 *
 * Index 0 is OK, 1 is DEGRADED, 2 is DOWN. The names below are the literals of
 * grammars/e17_status.gbnf letter for letter, and the quiet state is first because the
 * stub answers the grammar's first alternative in CI.
 *
 * The one line to read twice is the 99: AT+CSQ reports 99 for "unknown or not
 * detectable", which sorts above 31 as a number and below 0 as a signal. A rule that
 * compares it with a threshold calls a modem with no measurable signal OK, so 99 is
 * tested before the threshold, and a registered modem that cannot report its signal is
 * DEGRADED rather than OK.
 */
#include "rules.h"

static const char *const rule_e17_names[] = { "OK", "DEGRADED", "DOWN", NULL };

static int rule_e17_fn(const rule_ctx *ctx, const io_event *ev)
{
    int csq, stat;
    (void)ctx;                                     /* one sample decides; no history */
    if (ev->n < 3) return 2;                       /* an incomplete sample is DOWN, not a guess */
    csq = (int)ev->v[0];
    stat = (int)ev->v[2];
    if (stat != 1 && stat != 5) return 2;          /* DOWN: not registered, home or roaming */
    if (csq == 99 || csq < 10) return 1;           /* DEGRADED: 99 is unknown, never excellent */
    return 0;                                      /* OK: registered and CSQ at least 10 */
}

/* The constructor below registers the rule before main() runs; rule_find("e17") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e17 = { "e17", rule_e17_fn, rule_e17_names };
RULE_REGISTER(rule_e17);
