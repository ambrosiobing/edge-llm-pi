/* edgellm_main.h: what a C project supplies to reuse the shared main() in
 * common/c/src/edgellm_main.c, and what it gets back.
 *
 * The loop in edgellm_main.c: open the source (replay or live), look the rule up with
 * rule_find(--project), then for every event: run the rule on the history (the current
 * event is NOT yet in the history when the rule sees it; it is pushed after), act
 * through project_actuate (io_actuate only, never the model), build the user prompt
 * with project_format_prompt, ask the engine, log the row, push the event.
 */
#ifndef EDGELLM_MAIN_H
#define EDGELLM_MAIN_H

#include "io.h"
#include "rules.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Required. Writes the user prompt for this event into buf and returns buf, or returns
 * NULL to log the rule alone for this event without asking the model. */
const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len);

/* Optional; the weak default does nothing. The rule's decision index has already been
 * computed; drive an output through io_actuate here, for example
 * io_actuate(src, "led:0", rule_index == 2). Return 0, or -1 to count an error. */
int project_actuate(io_source *src, const rule_desc *rule, int rule_index,
                    const io_event *ev);

/* The loop. A C++ project with its own main() calls this; C projects get main() from
 * edgellm_main.c unless EDGELLM_NO_MAIN is defined when that file is compiled. */
int edgellm_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif
#endif /* EDGELLM_MAIN_H */
