/* rules_e04.c: the control rule of project e04, the Pi thermal and throttle narrator.
 * The event carries three values: v0 is the SoC temperature in millidegrees Celsius as
 * /sys/class/thermal/thermal_zone0/temp prints it, v1 is the word vcgencmd get_throttled
 * printed, as a number (the CSV and the live reader both hold its decimal value), and
 * v2 is the ARM clock in hertz from vcgencmd measure_clock arm, which the rule never
 * reads. The rule is what the agreement table measures the model against; the rule
 * decides, the model narrates.
 *
 * Index 0 is COOL, 1 is WARM, 2 is THROTTLED. The names below are the grammar's
 * literals letter for letter, see grammars/e04_thermal.gbnf; the oracle test and the
 * agreement table depend on that equality. Bits 16 to 19 of the throttle word only say
 * that something has occurred since boot and never decide anything here.
 */
#include "rules.h"

static const char *const rule_e04_names[] = { "COOL", "WARM", "THROTTLED", NULL };

static int rule_e04_fn(const rule_ctx *ctx, const io_event *ev)
{
    double temp_c = ev->n > 0 ? ev->v[0] / 1000.0 : 0.0;  /* millidegrees to degrees */
    long   bits   = ev->n > 1 ? (long)ev->v[1] : 0;       /* the get_throttled word */
    (void)ctx;                                            /* one reading decides; no history */
    if ((bits & 0x4) || (bits & 0x8) || temp_c > 80.0) return 2; /* THROTTLED: bit 2 or 3 */
    if (temp_c > 65.0) return 1;                          /* WARM: above 65 C */
    return 0;                                             /* COOL */
}

/* The constructor below registers the rule before main() runs; rule_find("e04") then
 * returns it. The file is compiled into the executable, never into an archive. */
const rule_desc rule_e04 = { "e04", rule_e04_fn, rule_e04_names };
RULE_REGISTER(rule_e04);
