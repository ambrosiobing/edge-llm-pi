/* rules.h: every project's control rule, one function each, two or three lines of
 * logic, deterministic, unit tested in tests/rules. The rule is what acts; the model
 * only proposes or explains. Each rule returns an index into its own name table so
 * the log and the grammar's literals agree letter for letter.
 *
 * A rule sees the current event and a short history (oldest first) the loop keeps for
 * it; it never sees the model's answer.
 */
#ifndef EDGELLM_RULES_H
#define EDGELLM_RULES_H

#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RULE_HISTORY 16

typedef struct rule_ctx {
    io_event hist[RULE_HISTORY];   /* oldest first */
    int      n;                    /* how many of hist are valid */
} rule_ctx;

/* Push ev onto ctx, dropping the oldest when full. */
void rule_ctx_push(rule_ctx *ctx, const io_event *ev);

/* A rule: returns the decision index, never negative, never past its name table. */
typedef int (*rule_fn)(const rule_ctx *ctx, const io_event *ev);

typedef struct rule_desc {
    const char        *project;    /* "e06" */
    rule_fn            fn;
    const char *const *names;      /* NULL terminated, the enum literals the grammar allows */
} rule_desc;

/* Every project registers its rule in its own rules_eNN.c; this looks it up. */
const rule_desc *rule_find(const char *project);

/* Registration. Added Wednesday 14 October 2026 because a header without it leaves no
 * way for several projects' rules to meet in one statically linked test binary: a
 * registry filled by constructors is the one form that needs neither a central list
 * nor a symbol each project would have to define twice. A project's rules_eNN.c ends
 * with RULE_REGISTER(rule_eNN); and is compiled straight into the executable, never
 * into an archive (see rules.c). */
void rule_register(const rule_desc *d);
#define RULE_REGISTER(desc) \
    __attribute__((constructor)) static void rule_register_##desc(void) \
    { rule_register(&(desc)); }

/* Enumeration for the test harness: rule_at(i) for 0 <= i < rule_count(). */
int rule_count(void);
const rule_desc *rule_at(int index);
int rule_name_count(const rule_desc *d);

#ifdef __cplusplus
}
#endif
#endif /* EDGELLM_RULES_H */
