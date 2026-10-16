/* rules_e02_test.c: six cases on the e02 rule for the shared harness tests/rules_test,
 * two per decision, including the three boundary values that are deliberately not in
 * the upper band (60.0, 80.0 and 10.0). The expected value is the index into the
 * rule's name table, which is also the grammar's literal order.
 */
#include "rules_test.h"

static void e02_cases(void)
{
    io_event quiet   = { 0.0, "synthetic", 2, { 42.00,  0.00 } };
    io_event at60    = { 1.0, "synthetic", 2, { 60.00,  2.00 } };
    io_event over60  = { 2.0, "synthetic", 2, { 60.01,  0.00 } };
    io_event at80    = { 3.0, "synthetic", 2, { 80.00, 10.00 } };
    io_event over80  = { 4.0, "synthetic", 2, { 80.01, -3.00 } };
    io_event steep   = { 5.0, "synthetic", 2, { 20.00, 10.50 } };

    rules_test_expect("e02", "quiet level, no slope, is NORMAL", &quiet, 0);
    rules_test_expect("e02", "level exactly 60 is still NORMAL", &at60, 0);
    rules_test_expect("e02", "level just above 60 is WATCH", &over60, 1);
    rules_test_expect("e02", "level 80 with slope 10 is still WATCH", &at80, 1);
    rules_test_expect("e02", "level above 80 is ALERT", &over80, 2);
    rules_test_expect("e02", "slope above 10 at a low level is ALERT", &steep, 2);
}
RULES_TEST_REGISTER(e02_cases);
