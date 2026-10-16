/* rules_e08_test.c: six cases on the e08 rule for the shared harness tests/rules_test,
 * two per decision, including both boundary values, which are deliberately in the
 * lower band (2.0 s is IN_SYNC, 60.0 s is DRIFTING), and one negative difference, the
 * DS3231 behind the Pi, which must be read by its size and not its sign. The event
 * layout is v[0] question, v[1] DS3231 seconds, v[2] Pi seconds, v[3] pressure in kPa,
 * v[4] temperature in C; only the difference of v[1] and v[2] matters to the rule, so
 * the seconds here are small numbers rather than epoch values. The expected value is
 * the index into the rule's name table.
 */
#include "rules_test.h"

static void e08_cases(void)
{
    io_event quiet  = { 0.0, "explorer700", 5, { 0.0, 1000.40, 1000.00, 98.4, 24.1 } };
    io_event at2    = { 1.0, "explorer700", 5, { 1.0, 1002.00, 1000.00, 98.4, 24.1 } };
    io_event over2  = { 2.0, "explorer700", 5, { 2.0, 1002.01, 1000.00, 98.4, 24.1 } };
    io_event at60   = { 3.0, "explorer700", 5, { 3.0, 1060.00, 1000.00, 98.4, 24.1 } };
    io_event over60 = { 4.0, "explorer700", 5, { 4.0, 1060.01, 1000.00, 98.4, 24.1 } };
    io_event behind = { 5.0, "explorer700", 5, { 1.0,  910.00, 1000.00, 98.4, 24.1 } };

    rules_test_expect("e08", "0.4 s apart is IN_SYNC", &quiet, 0);
    rules_test_expect("e08", "exactly 2.0 s apart is still IN_SYNC", &at2, 0);
    rules_test_expect("e08", "2.01 s apart is DRIFTING", &over2, 1);
    rules_test_expect("e08", "exactly 60.0 s apart is still DRIFTING", &at60, 1);
    rules_test_expect("e08", "60.01 s apart is LOST", &over60, 2);
    rules_test_expect("e08", "DS3231 90 s behind the Pi is LOST, sign ignored", &behind, 2);
}
RULES_TEST_REGISTER(e08_cases);
