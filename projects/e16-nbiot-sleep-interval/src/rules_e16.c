/* rules_e16.c: the control rule of project e16, the NB-IoT sleep interval. One event is
 * one scenario: a current profile reduced from a PPK2 capture of one power-saving-mode
 * cycle, a cell, a battery target, and the PSM window the network granted. The rule
 * picks the largest of the four allowed intervals whose computed battery life meets the
 * target and which fits inside that window.
 *
 * Index 0 is I60, 1 is I300, 2 is I900, 3 is I3600, which is the order of the grammar's
 * literals in grammars/e16_interval.gbnf and of battery_interval_s[] in src/battery.h.
 * I60 is first because it is the safe fallback: the shortest interval keeps the module
 * reachable most often and is always inside any window a network grants, so a profile
 * the rule cannot use at all still produces a decision the bench can act on.
 *
 * The arithmetic is not here. It is in src/battery.c, one function per line of the
 * model, with src/battery_test.c checking it against figures worked by hand; this file
 * is only the search over four candidates, and the search runs from the largest down
 * because battery_life_days() rises with the interval for every profile
 * battery_valid_profile() accepts.
 */
#include "rules.h"

#include "battery.h"

static const char *const rule_e16_names[] = { "I60", "I300", "I900", "I3600", NULL };

static int rule_e16_fn(const rule_ctx *ctx, const io_event *ev)
{
    batt_profile p;
    int i;
    (void)ctx;                                         /* one scenario per event, no history */
    if (battery_profile_from_event(ev, &p) != 0 || !battery_valid_profile(&p))
        return 0;                                      /* I60, the safe fallback, not a guess */
    for (i = E16_INTERVALS - 1; i > 0; i--)            /* largest first, down to I60 */
        if (battery_interval_s[i] <= p.granted_s &&
            battery_life_days(&p, battery_interval_s[i]) >= p.target_days)
            return i;
    return 0;                                          /* I60: inside every grant, always */
}

/* The constructor below registers the rule before main() runs; rule_find("e16") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e16 = { "e16", rule_e16_fn, rule_e16_names };
RULE_REGISTER(rule_e16);
