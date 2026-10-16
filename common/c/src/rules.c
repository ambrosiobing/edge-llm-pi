/* rules.c: the history window and the rule registry.
 *
 * Registration, the one way that works with static linking and with several projects
 * in one test binary: a project's rules_eNN.c defines its rule_desc and ends with
 *
 *     RULE_REGISTER(rule_e06);
 *
 * which expands to a constructor function that calls rule_register() before main()
 * runs. The file must be listed as a source of the executable (edgellm_add_project
 * does this), never put into a static library: the linker drops an archive member
 * nothing references, and nothing references a constructor.
 */
#include "rules.h"

#include <stdio.h>
#include <string.h>

#define RULE_MAX 64

static const rule_desc *registry[RULE_MAX];
static int registry_n;

void rule_ctx_push(rule_ctx *ctx, const io_event *ev)
{
    if (ctx == NULL || ev == NULL)
        return;
    if (ctx->n == RULE_HISTORY) {
        memmove(&ctx->hist[0], &ctx->hist[1], sizeof(io_event) * (RULE_HISTORY - 1));
        ctx->n = RULE_HISTORY - 1;
    }
    ctx->hist[ctx->n++] = *ev;
}

void rule_register(const rule_desc *d)
{
    if (d == NULL || d->project == NULL || d->fn == NULL || d->names == NULL)
        return;
    if (registry_n >= RULE_MAX) {
        fprintf(stderr, "rules: registry full, %s not registered\n", d->project);
        return;
    }
    registry[registry_n++] = d;
}

const rule_desc *rule_find(const char *project)
{
    int i;
    if (project == NULL)
        return NULL;
    for (i = 0; i < registry_n; i++)
        if (strcmp(registry[i]->project, project) == 0)
            return registry[i];
    return NULL;
}

int rule_count(void)
{
    return registry_n;
}

const rule_desc *rule_at(int index)
{
    return (index >= 0 && index < registry_n) ? registry[index] : NULL;
}

int rule_name_count(const rule_desc *d)
{
    int n = 0;
    if (d == NULL || d->names == NULL)
        return 0;
    while (d->names[n] != NULL)
        n++;
    return n;
}
