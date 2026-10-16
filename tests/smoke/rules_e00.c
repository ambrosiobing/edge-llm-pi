/* rules_e00.c: the smoke project's rule, the shape every rules_eNN.c follows. Two
 * lines of logic: v[0] above 80 is HOT, above 60 is WARM, else COOL. The name table
 * is the grammar's literal order in tests/data/smoke.gbnf.
 */
#include "rules.h"

static const char *const e00_names[] = { "COOL", "WARM", "HOT", NULL };

static int e00_rule(const rule_ctx *ctx, const io_event *ev)
{
    (void)ctx;
    if (ev->n < 1)
        return 0;
    return ev->v[0] > 80.0 ? 2 : (ev->v[0] > 60.0 ? 1 : 0);
}

const rule_desc rule_e00 = { "e00", e00_rule, e00_names };
RULE_REGISTER(rule_e00);
