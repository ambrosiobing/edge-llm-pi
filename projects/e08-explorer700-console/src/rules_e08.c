/* rules_e08.c: the control rule of project e08, the Explorer700 console. One
 * difference decides: v[1] is the DS3231's time read from /sys/class/rtc/rtc0/since_epoch
 * and v[2] the Pi's own clock at the same moment, both in seconds since the epoch. The
 * rule is what sounds the buzzer; the model only writes the sentence on the OLED.
 *
 * Index 0 is IN_SYNC, 1 is DRIFTING, 2 is LOST. The thresholds are 2 s and 60 s, both
 * strict: a difference of exactly 2.0 s is IN_SYNC and exactly 60.0 s is DRIFTING. The
 * sign does not matter; a clock an hour behind is as lost as one an hour ahead. An
 * event without the clock pair is LOST, because a clock that cannot be read is not in
 * sync with anything.
 */
#include "rules.h"

static const char *const rule_e08_names[] = { "IN_SYNC", "DRIFTING", "LOST", NULL };

static int rule_e08_fn(const rule_ctx *ctx, const io_event *ev)
{
    double d;
    (void)ctx;                                 /* one difference decides; no history */
    if (ev->n < 3) return 2;                   /* no clock pair is a lost clock, not a guess */
    d = ev->v[1] - ev->v[2];                   /* DS3231 seconds minus Pi seconds */
    if (d < 0.0) d = -d;
    if (d > 60.0) return 2;                    /* LOST: more than a minute apart */
    if (d > 2.0) return 1;                     /* DRIFTING: more than two seconds apart */
    return 0;                                  /* IN_SYNC */
}

/* The constructor registers the rule before main() runs; rule_find("e08") returns it.
 * The file is compiled into the console executable, never into an archive. */
const rule_desc rule_e08 = { "e08", rule_e08_fn, rule_e08_names };
RULE_REGISTER(rule_e08);
