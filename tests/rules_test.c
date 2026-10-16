/* rules_test.c: calls every registered rule on fixed events and runs the cases the
 * projects registered through rules_test.h.
 *
 * Two checks for every rule, with no project input: the index on an all-zero event and
 * on a run of 24 synthetic events (so the history window wraps) must stay inside the
 * rule's name table. Then every registered case: build the history, call the rule on
 * the event, compare the index. Exit 1 on any failure, with each failure printed.
 */
#include "rules_test.h"

#include <stdio.h>
#include <string.h>

#define MAX_CASES 512

typedef struct test_case {
    const char *project;
    const char *label;
    io_event    hist[RULE_HISTORY];
    int         n_hist;
    io_event    ev;
    int         expected;
} test_case;

static test_case cases[MAX_CASES];
static int n_cases;

void rules_test_expect_hist(const char *project, const char *label, const io_event *hist,
                            int n_hist, const io_event *ev, int expected)
{
    test_case *c;
    int i;
    if (n_cases >= MAX_CASES) {
        fprintf(stderr, "rules_test: more than %d cases, '%s' dropped\n", MAX_CASES, label);
        return;
    }
    c = &cases[n_cases++];
    memset(c, 0, sizeof(*c));
    c->project = project;
    c->label = label;
    if (n_hist > RULE_HISTORY)
        n_hist = RULE_HISTORY;
    for (i = 0; i < n_hist; i++)
        c->hist[i] = hist[i];
    c->n_hist = n_hist;
    c->ev = *ev;
    c->expected = expected;
}

void rules_test_expect(const char *project, const char *label, const io_event *ev,
                       int expected)
{
    rules_test_expect_hist(project, label, NULL, 0, ev, expected);
}

static int in_range(const rule_desc *d, int idx)
{
    return idx >= 0 && idx < rule_name_count(d);
}

int main(void)
{
    int i, failures = 0, checks = 0;
    int n_rules = rule_count();

    if (n_rules == 0) {
        printf("rules_test: no rule registered\n");
        return 1;
    }
    for (i = 0; i < n_rules; i++) {
        const rule_desc *d = rule_at(i);
        rule_ctx ctx;
        io_event ev;
        int k, idx;

        memset(&ctx, 0, sizeof(ctx));
        memset(&ev, 0, sizeof(ev));
        idx = d->fn(&ctx, &ev);
        checks++;
        if (!in_range(d, idx)) {
            printf("FAIL %s: zero event gave %d, names %d\n", d->project, idx,
                   rule_name_count(d));
            failures++;
        }
        for (k = 0; k < 24; k++) {
            int j;
            memset(&ev, 0, sizeof(ev));
            ev.t_s = (double)k;
            snprintf(ev.tag, sizeof(ev.tag), "synthetic");
            ev.n = IO_MAX_VALUES;
            for (j = 0; j < IO_MAX_VALUES; j++)
                ev.v[j] = (double)((k * 7 + j * 3) % 11) * ((k % 2) ? 100.0 : -0.5);
            idx = d->fn(&ctx, &ev);
            checks++;
            if (!in_range(d, idx)) {
                printf("FAIL %s: synthetic event %d gave %d\n", d->project, k, idx);
                failures++;
            }
            rule_ctx_push(&ctx, &ev);
        }
    }
    for (i = 0; i < n_cases; i++) {
        const test_case *c = &cases[i];
        const rule_desc *d = rule_find(c->project);
        rule_ctx ctx;
        int k, idx;

        checks++;
        if (d == NULL) {
            printf("FAIL %s '%s': rule not linked into rules_test\n", c->project, c->label);
            failures++;
            continue;
        }
        memset(&ctx, 0, sizeof(ctx));
        for (k = 0; k < c->n_hist; k++)
            rule_ctx_push(&ctx, &c->hist[k]);
        idx = d->fn(&ctx, &c->ev);
        if (idx != c->expected) {
            printf("FAIL %s '%s': expected %d (%s) got %d (%s)\n", c->project, c->label,
                   c->expected, in_range(d, c->expected) ? d->names[c->expected] : "?",
                   idx, in_range(d, idx) ? d->names[idx] : "?");
            failures++;
        }
    }
    printf("rules_test: %d rules, %d cases, %d checks, %d failures\n", n_rules, n_cases,
           checks, failures);
    return failures == 0 ? 0 : 1;
}
