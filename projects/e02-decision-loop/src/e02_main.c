/* e02_main.c: project e02 on the shared main, common/c/src/edgellm_main.c. The shared
 * main owns the loop: it opens the io_source (a replayed CSV here, a live device from
 * e03 on), looks the rule up with rule_find("e02"), runs it on every event, asks this
 * file for the user text, asks the engine for one grammar-constrained decision with
 * the saved system-prompt state restored, and writes the log row with the rule column
 * beside the model column. This file owns only the prompt.
 *
 * There is no project_actuate here: e02 drives no output, and the weak default in the
 * shared main does nothing. The user text names the level, the slope and up to three
 * previous levels, nothing else. The thresholds live in prompt.txt, the system prompt,
 * which is decoded once at start and restored per decision, so they cost no tokens per
 * event. The current event is not yet in the history when this is called.
 */
#include "edgellm_main.h"

#include <stdio.h>

const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    char hist[96] = "";
    int  k = 0, i;
    for (i = (ctx->n > 3 ? ctx->n - 3 : 0); i < ctx->n && k < (int)sizeof hist - 12; i++)
        k += snprintf(hist + k, sizeof hist - (size_t)k, "%s%.0f", k ? ", " : "",
                      ctx->hist[i].v[0]);
    snprintf(buf, len, "level %.1f, slope %.1f per second, previous levels %s. Decide.",
             ev->n > 0 ? ev->v[0] : 0.0, ev->n > 1 ? ev->v[1] : 0.0, k ? hist : "none");
    return buf;
}
