/* rules_e19_test.c: six cases on the classification half of the e19 rule, for the
 * shared harness tests/rules_test. All four indices occur and both magnitude
 * boundaries are held: a peak of exactly 2 g is still NONE and a magnitude of exactly
 * 0.3 g is still NONE, because both thresholds are strict.
 *
 * The accelerations are written as multiples of 9.80665, the same expression the rule
 * uses for g, so a boundary case compares two identical products and cannot turn on a
 * rounding step in the last place of a decimal literal.
 *
 * The send gate, rule_e19_sends, is the other half of the rule and is tested in
 * src/compose_check_test.c, which is this project's own test binary: the shared harness
 * compares decision indices and has nowhere to put a case about time.
 */
#include "rules_test.h"

#define G 9.80665

static void e19_cases(void)
{
    io_event quiet = { 0.0, "adxl345", 3, { 0.0, 0.0, 1.0 * G } };
    io_event at2g  = { 1.0, "adxl345", 3, { 0.0, 0.0, 2.0 * G } };
    io_event tap   = { 2.0, "adxl345", 3, { 0.0, 0.0, 2.5 * G } };
    io_event at03g = { 3.0, "adxl345", 3, { 0.0, 0.0, 0.3 * G } };
    io_event fall  = { 4.0, "adxl345", 3, { 0.0, 0.0, 0.1 * G } };
    io_event onside = { 5.0, "adxl345", 3, { 1.0 * G, 0.0, 0.0 } };

    rules_test_expect("e19", "1 g at rest is NONE", &quiet, 0);
    rules_test_expect("e19", "a peak of exactly 2 g is still NONE", &at2g, 0);
    rules_test_expect("e19", "2.5 g is a TAP", &tap, 1);
    rules_test_expect("e19", "exactly 0.3 g is still NONE", &at03g, 0);
    rules_test_expect("e19", "0.1 g is a FREEFALL", &fall, 3);
    rules_test_expect_hist("e19", "90 degrees from the previous reading is a TILT",
                           &quiet, 1, &onside, 2);
}
RULES_TEST_REGISTER(e19_cases);
