/* e03_main.c: project e03, the LED status explainer, on the shared loop of
 * common/c/src/edgellm_main.c. The loop opens the event source, looks the rule up with
 * rule_find("e03"), runs it on every event, calls project_actuate with the rule's index,
 * then asks this file for the user text, asks the engine for one grammar-constrained
 * answer with the saved system-prompt state restored, and writes the log row with the
 * rule column beside the model column. This file owns the LEDs and the prompt.
 *
 * Two sources, because two things differ in kind. The level and slope are modelled: a
 * replayed CSV, the state stream of e02, is what --replay opens and what the rule
 * reads. The three Joy-it LK-LED10 modules on BCM 17, 27 and 22 are real, and the shared
 * main opens exactly one io_source, so this file takes one option of its own, --leds
 * <gpio spec>, strips it from argv before edgellm_main() sees it, opens the libgpiod 2
 * backend through io_open_live and drives the lines through io_actuate, the same call
 * the replay backend counts when --leds is absent. --live gpio would not do: the gpio
 * backend's events are the line read-backs, not a level stream. CI runs without --leds.
 *
 * The modules carry the anode on the signal pin and light when driven high, so a line
 * that is high lights. The gpio backend requests every output line inactive, which is
 * why nothing lights at start, and main() drives all three low before it returns.
 */
#include "edgellm_main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define E03_LEDS 3

static io_source *leds;              /* the gpio sink from --leds, or NULL for the replay */
static const char *lit = "GREEN";    /* what project_actuate lit, read by the prompt */

/* The rule's index lights exactly one of the three lines. The two others go low first,
 * so that two LEDs are never lit together, then the chosen one goes high. The model has
 * no path to this function: the index comes from rules_e03.c, through the shared main. */
int project_actuate(io_source *src, const rule_desc *rule, int rule_index,
                    const io_event *ev)
{
    io_source *sink = leds != NULL ? leds : src;
    char what[16];
    int i, rc = 0;
    (void)ev;
    lit = rule->names[rule_index];
    for (i = 0; i < E03_LEDS; i++) {
        if (i == rule_index)
            continue;
        snprintf(what, sizeof what, "led:%d", i);
        if (io_actuate(sink, what, 0) != 0)
            rc = -1;
    }
    snprintf(what, sizeof what, "led:%d", rule_index);
    if (io_actuate(sink, what, 1) != 0)
        rc = -1;
    return rc;
}

/* The user text names the level, the slope and the colour the rule lit, nothing else.
 * The thresholds are in prompt.txt, decoded once at start, so they cost no tokens per
 * event. The shared main calls project_actuate before this function for the same event,
 * so lit is the colour on the header when the model writes its sentence. */
const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    (void)ctx;
    snprintf(buf, len, "level %.1f, slope %.1f per second. The rule lit %s. Explain why in "
                       "one sentence.",
             ev->n > 0 ? ev->v[0] : 0.0, ev->n > 1 ? ev->v[1] : 0.0, lit);
    return buf;
}

/* Strips --leds <spec> out of argv, opens the sink, and hands the rest to the shared
 * loop. EDGELLM_NO_MAIN is defined for edgellm_main.c in this project's CMakeLists.txt,
 * so this is the one main() of edgellm-e03. */
int main(int argc, char **argv)
{
    char err[256], what[16];
    char **rest = malloc(sizeof(char *) * (size_t)(argc + 1));
    const char *spec = NULL;
    int i, n = 0, rc;

    if (rest == NULL)
        return 1;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--leds") == 0 && i + 1 < argc) {
            spec = argv[++i];
            continue;
        }
        rest[n++] = argv[i];
    }
    rest[n] = NULL;
    if (spec != NULL) {
        leds = io_open_live("gpio", spec, err, sizeof err);
        if (leds == NULL) {
            fprintf(stderr, "%s: %s\n", argv[0], err);
            free(rest);
            return 1;
        }
    }
    rc = edgellm_main(n, rest);
    if (leds != NULL) {
        for (i = 0; i < E03_LEDS; i++) {          /* nothing stays lit after the loop */
            snprintf(what, sizeof what, "led:%d", i);
            io_actuate(leds, what, 0);
        }
        io_close(leds);
    }
    free(rest);
    return rc;
}
