/* smoke_project.c: the smallest complete project, e00. It exercises the shared main,
 * the replay backend, the rule registry, the stub engine and the logger in CI, and it
 * is the template a C project copies: one function that turns an event into the user
 * prompt, and (optionally) one that acts on the rule's decision through io_actuate.
 */
#include "edgellm_main.h"

#include <stdio.h>

const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    double prev = ctx->n > 0 ? ctx->hist[ctx->n - 1].v[0] : ev->v[0];
    snprintf(buf, len, "SoC temperature %.1f C, previous %.1f C. Classify.", ev->v[0], prev);
    return buf;
}

int project_actuate(io_source *src, const rule_desc *rule, int rule_index, const io_event *ev)
{
    (void)rule;
    (void)ev;
    /* In the replay backend this is accepted and counted; on a gpio source it would
     * light output line 0 for HOT. The rule drives it, never the model. */
    return io_actuate(src, "led:0", rule_index == 2);
}
