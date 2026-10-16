/* rules_e09_test.c: six cases on the e09 rule for the shared harness tests/rules_test.
 * Two of them sit on a boundary the rule treats strictly, 2.0 g and 0.3 g, and the
 * tilt case is 36.9 degrees from a reading at rest, which the rule sees as the oldest
 * entry of the window. The expected value is the index into the rule's name table,
 * which is also the grammar's literal order: NONE 0, TAP 1, TILT 2, FREEFALL 3.
 */
#include "rules_test.h"

static void e09_cases(void)
{
    io_event rest  = { 0.0, "adxl345", 3, { 0.01, -0.02, 1.00 } };
    io_event at2g  = { 0.1, "adxl345", 3, { 0.00,  0.00, 2.00 } };
    io_event tap   = { 0.2, "adxl345", 3, { 0.30,  0.20, 2.10 } };
    io_event at03g = { 0.3, "adxl345", 3, { 0.00,  0.00, 0.30 } };
    io_event fall  = { 0.4, "adxl345", 3, { 0.05,  0.10, 0.20 } };
    io_event tilt  = { 0.5, "adxl345", 3, { 0.60,  0.00, 0.80 } };   /* 36.9 degrees */

    rules_test_expect("e09", "at rest with no history is NONE", &rest, 0);
    rules_test_expect("e09", "magnitude exactly 2 g is still NONE", &at2g, 0);
    rules_test_expect("e09", "magnitude above 2 g is TAP", &tap, 1);
    rules_test_expect_hist("e09", "magnitude exactly 0.3 g after rest is still NONE",
                           &rest, 1, &at03g, 0);
    rules_test_expect("e09", "magnitude below 0.3 g is FREEFALL", &fall, 3);
    rules_test_expect_hist("e09", "36.9 degrees from the oldest reading is TILT",
                           &rest, 1, &tilt, 2);
}
RULES_TEST_REGISTER(e09_cases);
