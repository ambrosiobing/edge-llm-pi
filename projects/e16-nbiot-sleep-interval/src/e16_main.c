/* e16_main.c: project e16, the NB-IoT sleep interval, on the shared loop of
 * common/c/src/edgellm_main.c. The loop opens the event source, looks the rule up with
 * rule_find("e16"), runs it on every scenario, asks this file for the user text, asks
 * the engine for one grammar-constrained answer with the saved system-prompt state
 * restored, and writes the log row with the rule column beside the model column.
 *
 * Nothing is decided live, and that is the design rather than a limitation: an attach
 * takes minutes and a power-saving-mode cycle takes minutes, so the PPK2 captures one
 * cycle on the module's own supply rail, tools/profile_summarise.py reduces the capture
 * to four numbers, and the loop runs over scenarios built from those numbers. The event
 * source is therefore always the replay backend.
 *
 * Two options of this project's own, both taken out of argv before the shared loop sees
 * it, both about the one number the trace cannot give:
 *
 *   --at-plan            print the AT sequence that asks for a PSM window and reads
 *                        back what the network granted, with every timer octet decoded
 *                        into seconds. Needs no port, no SIM and no module; this is
 *                        what goes into the lab book before the HAT is powered.
 *   --at <dev>:<baud>    run that sequence on the module and print the granted window.
 *                        Only in a build configured with -DEDGELLM_IO_SERIAL=ON; CI has
 *                        no port and never passes it.
 *
 * and two that set the request those two carry, --tau and --active, each an eight
 * character 3GPP timer octet. The defaults ask for a tracking area update period of one
 * hour (00100001, unit 1 of one hour times 1) and an active time of ten seconds
 * (00000101, unit 0 of two seconds times 5), which is the longest interval the rule can
 * choose and a short window after it.
 */
#include "edgellm_main.h"

#include "at_cycle.h"
#include "battery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *req_tau = "00100001";      /* 1 x 1 hour  = 3600 s */
static const char *req_active = "00000101";   /* 5 x 2 s     = 10 s   */

/* The user text names the six measured and stated numbers of the scenario and nothing
 * else. It does not carry the battery life at any interval: the question of this
 * chapter is whether a 360M model can do that division from the numbers a meter
 * produced, so the division stays the rule's and the agreement table counts the
 * difference. The thresholds and the four allowed intervals are in prompt.txt, decoded
 * once at start and restored per decision, so they cost no tokens per scenario. */
const char *project_format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                  size_t len)
{
    batt_profile p;
    (void)ctx;
    if (battery_profile_from_event(ev, &p) != 0) {
        snprintf(buf, len, "%s", "the scenario row is short. Choose.");
        return buf;
    }
    snprintf(buf, len, "sleep %.1f uA, wake %.1f uAh over %.0f s, cell %.0f mAh, "
                       "target %.0f days, network granted %.0f s. Choose.",
             p.sleep_ua, p.active_uah, p.active_s, p.capacity_mah, p.target_days,
             p.granted_s);
    return buf;
}

static void print_plan(void)
{
    const at_step *steps;
    char text[128];
    int n = 0, i;

    steps = at_cycle_plan(&n);
    printf("e16 AT plan for the SIM7020E, %d steps, 115200 8N1 on the PL011.\n", n);
    printf("requested periodic TAU %s = %.0f s, active time %s = %.0f s\n\n",
           req_tau, at_timer_decode_tau(req_tau), req_active,
           at_timer_decode_active(req_active));
    for (i = 0; i < n; i++)
        printf("%2d. %-34s expect %-8s %4.0f s, %d retries  %s\n", i + 1,
               at_cycle_text(&steps[i], req_tau, req_active, text, sizeof text),
               steps[i].expect, steps[i].timeout_s, steps[i].retries, steps[i].why);
    printf("\nThe last step's reply carries the granted octets as its last two quoted\n"
           "fields, the active time first and the periodic TAU second. Decode them with\n"
           "--at, or by hand against the two tables in src/at_cycle.c, and write the\n"
           "granted window into the scenario file's sixth column.\n");
}

static int print_granted(const char *spec)
{
    at_granted g;
    int rc = at_cycle_run(spec, req_tau, req_active, &g, stdout);

    if (rc == -2)
        return 2;
    if (rc == -1) {
        fprintf(stderr, "e16: cannot open %s\n", spec);
        return 1;
    }
    if (rc > 0) {
        fprintf(stderr, "e16: step %d of the plan had no reply; the chapter stops at AT "
                        "bring-up and says so\n", rc);
        return 1;
    }
    printf("granted active time %s = %.0f s, granted periodic TAU %s = %.0f s\n",
           g.active_bits, g.active_s, g.tau_bits, g.tau_s);
    if (g.tau_s < 0.0)
        printf("the TAU unit is not in the application note's table, so no window was "
               "granted: the scenario file's sixth column is 0 and the rule falls back "
               "to I60\n");
    return 0;
}

/* Strips this project's four options out of argv and hands the rest to the shared loop.
 * EDGELLM_NO_MAIN is defined for edgellm_main.c in this project's CMakeLists.txt, so
 * this is the one main() of edgellm-e16. */
int main(int argc, char **argv)
{
    char **rest = malloc(sizeof(char *) * (size_t)(argc + 1));
    const char *at_spec = NULL;
    int i, n = 0, plan = 0;

    if (rest == NULL)
        return 1;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--at-plan") == 0) {
            plan = 1;
            continue;
        }
        if (strcmp(argv[i], "--at") == 0 && i + 1 < argc) {
            at_spec = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--tau") == 0 && i + 1 < argc) {
            req_tau = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--active") == 0 && i + 1 < argc) {
            req_active = argv[++i];
            continue;
        }
        rest[n++] = argv[i];
    }
    rest[n] = NULL;
    if (at_timer_decode_tau(req_tau) < 0.0 || at_timer_decode_active(req_active) < 0.0) {
        fprintf(stderr, "%s: --tau and --active each want eight characters of 0 and 1 "
                        "whose unit is in the application note's tables\n", argv[0]);
        free(rest);
        return 2;
    }
    if (plan) {
        print_plan();
        free(rest);
        return 0;
    }
    if (at_spec != NULL) {
        int rc = print_granted(at_spec);
        free(rest);
        return rc;
    }
    {
        int rc = edgellm_main(n, rest);
        free(rest);
        return rc;
    }
}
