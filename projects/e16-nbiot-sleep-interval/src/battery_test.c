/* battery_test.c: the unit test of the two deterministic parts of project e16, the
 * battery function of src/battery.c and the 3GPP timer decode of src/at_cycle.c that
 * hands it the granted window. Neither needs a model, a module, a SIM or a port, which
 * is the point: the arithmetic the chapter's claim rests on is checked by a program
 * that cannot be affected by any of them.
 *
 *     ctest --test-dir build -R e16_battery_test --output-on-failure
 *
 * Every expected figure below was worked by hand from the model stated in battery.h and
 * is repeated as a comment beside it, so that a reader can check the test as well as
 * the code. The two AT octets are the worked examples of the SIM7020 Series_Low Power
 * Mode_Application Note V1.03, section 3.2, and the +CEREG line is the literal reply of
 * its section 6.2.1 example.
 */
#include "at_cycle.h"
#include "battery.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

static void near(const char *label, double got, double want, double tol)
{
    checks++;
    if (fabs(got - want) > tol) {
        failures++;
        printf("FAIL %-58s got %.6f, want %.6f\n", label, got, want);
    }
}

static void equal_text(const char *label, const char *got, const char *want)
{
    checks++;
    if (got == NULL || strcmp(got, want) != 0) {
        failures++;
        printf("FAIL %-58s got %s, want %s\n", label, got != NULL ? got : "(null)", want);
    }
}

static void is_true(const char *label, int got)
{
    checks++;
    if (!got) {
        failures++;
        printf("FAIL %-58s expected true\n", label);
    }
}

int main(void)
{
    /* The reference profile of rules_e16_test.c: a 10 uA floor, a 50 uAh wake over
     * 20 s, a 2000 mAh cell, a 300 day target, an hour granted. */
    batt_profile p = { 10.0, 50.0, 20.0, 2000.0, 300.0, 3600.0 };
    batt_profile flat = { 100.0, 0.5, 30.0, 2000.0, 10.0, 3600.0 };
    at_granted g;
    char buf[64];
    char bits[9];
    int i;

    /* 10 uA over 3600 - 20 = 3580 s is 9.944444 uAh; plus the 50 uAh wake. */
    near("charge per cycle at 3600 s", battery_charge_per_cycle_uah(&p, 3600.0),
         59.944444, 1e-5);
    /* 10 uA over 300 - 20 = 280 s is 0.777778 uAh. */
    near("charge per cycle at 300 s", battery_charge_per_cycle_uah(&p, 300.0),
         50.777778, 1e-5);
    /* An interval shorter than the wake leaves no sleep at all: the wake alone. */
    near("charge per cycle at 10 s, shorter than the wake",
         battery_charge_per_cycle_uah(&p, 10.0), 50.0, 1e-9);
    near("cycles per day at 900 s", battery_cycles_per_day(900.0), 96.0, 1e-9);
    near("cycles per day at 0 s is 0, not a division", battery_cycles_per_day(0.0),
         0.0, 1e-9);
    /* 59.944444 uAh x 24 cycles is 1438.6667 uAh a day; 2 000 000 uAh of cell. */
    near("life at 3600 s, days", battery_life_days(&p, 3600.0), 1390.17609, 1e-4);
    /* 50.777778 uAh x 288 cycles is 14624.0 uAh a day. */
    near("life at 300 s, days", battery_life_days(&p, 300.0), 136.76149, 1e-4);
    /* 50.111111 uAh x 1440 cycles is 72160.0 uAh a day. */
    near("life at 60 s, days", battery_life_days(&p, 60.0), 27.71619, 1e-4);

    /* The property the rule's largest-first search depends on. */
    for (i = 1; i < E16_INTERVALS; i++)
        is_true("life does not fall as the interval grows",
                battery_life_days(&p, battery_interval_s[i]) >=
                battery_life_days(&p, battery_interval_s[i - 1]));
    is_true("the reference profile is accepted", battery_valid_profile(&p));
    is_true("a wake cheaper than its own span at the floor is refused",
            !battery_valid_profile(&flat));

    /* The application note's own worked examples: a TAU octet of unit 2 (10 hours)
     * times 7 is 70 hours, and an active-time octet of unit 0 (2 s) times 15 is 30 s. */
    near("TAU octet 01000111 is 70 hours", at_timer_decode_tau("01000111"), 252000.0, 1e-9);
    near("active octet 00001111 is 30 s", at_timer_decode_active("00001111"), 30.0, 1e-9);
    /* Unit 7 is not in either table of the note, so it decodes to -1 and the rule
     * reads the window as not granted rather than guessing at a deactivated timer. */
    near("TAU octet 11100000, unit not in the table", at_timer_decode_tau("11100000"),
         -1.0, 1e-9);
    near("a seven character octet is refused", at_timer_decode_tau("0100011"), -1.0, 1e-9);
    near("a non-binary octet is refused", at_timer_decode_active("0000111X"), -1.0, 1e-9);

    checks++;
    if (at_timer_encode(2, 7, bits) != 0)
        failures++;
    else
        equal_text("encode unit 2 value 7", bits, "01000111");
    checks++;
    if (at_timer_encode(8, 0, bits) == 0) {
        failures++;
        printf("FAIL %-58s unit 8 should be refused\n", "encode out of range");
    }

    checks++;
    if (at_cpsms_request(buf, sizeof buf, "01000111", "00001111") != 0)
        failures++;
    else
        equal_text("the CPSMS request line", buf, "AT+CPSMS=1,,,\"01000111\",\"00001111\"");

    /* The literal reply of the note's section 6.2.1, after the request was accepted. */
    checks++;
    if (at_cereg_parse("+CEREG: 4,1,\"5B57\",\"01A50B1A\",9,\"00\",,,\"00000001\",\"11100000\"",
                       &g) != 0) {
        failures++;
        printf("FAIL %-58s no octet pair found\n", "parse the granted +CEREG line");
    } else {
        equal_text("granted active time octet", g.active_bits, "00000001");
        equal_text("granted periodic TAU octet", g.tau_bits, "11100000");
        near("granted active time, seconds", g.active_s, 2.0, 1e-9);
        near("granted TAU, unit not in the table", g.tau_s, -1.0, 1e-9);
    }
    checks++;
    if (at_cereg_parse("+CEREG: 0,1", &g) == 0) {
        failures++;
        printf("FAIL %-58s a line with no octets should be refused\n", "parse a short line");
    }

    printf("e16_battery_test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
