/* rules_e06_test.c: six cases on the e06 rule for the shared harness tests/rules_test.
 * Two per decision, with the boundary that 300 mm itself is not below 300 mm, the hold
 * that ends a falling run, and the range status that turns a near sample into no sample.
 * The expected value is the index into the rule's name table, which is also the grammar's
 * literal order: 0 CLEAR, 1 APPROACHING, 2 TOO_CLOSE. v0 is mm, v1 the range status.
 */
#include "rules_test.h"

static void e06_cases(void)
{
    io_event far      = { 0.0, "vl53l4cd", 2, { 800.0, 0.0 } };
    io_event near_one = { 0.1, "vl53l4cd", 2, { 250.0, 0.0 } };
    io_event near_two = { 0.2, "vl53l4cd", 2, { 240.0, 0.0 } };
    io_event at300    = { 0.3, "vl53l4cd", 2, { 300.0, 0.0 } };
    io_event under300 = { 0.4, "vl53l4cd", 2, { 299.0, 0.0 } };
    io_event near_bad = { 0.5, "vl53l4cd", 2, { 240.0, 4.0 } };
    io_event fall[4]  = { { 1.0, "vl53l4cd", 2, { 900.0, 0.0 } },
                          { 1.1, "vl53l4cd", 2, { 800.0, 0.0 } },
                          { 1.2, "vl53l4cd", 2, { 700.0, 0.0 } },
                          { 1.3, "vl53l4cd", 2, { 600.0, 0.0 } } };
    io_event fifth    = { 1.4, "vl53l4cd", 2, { 500.0, 0.0 } };
    io_event hold[4]  = { { 2.0, "vl53l4cd", 2, { 900.0, 0.0 } },
                          { 2.1, "vl53l4cd", 2, { 800.0, 0.0 } },
                          { 2.2, "vl53l4cd", 2, { 700.0, 0.0 } },
                          { 2.3, "vl53l4cd", 2, { 650.0, 0.0 } } };
    io_event held     = { 2.4, "vl53l4cd", 2, { 650.0, 0.0 } };

    rules_test_expect("e06", "800 mm with no history is CLEAR", &far, 0);
    rules_test_expect_hist("e06", "250 then 240 mm is TOO_CLOSE", &near_one, 1, &near_two, 2);
    rules_test_expect_hist("e06", "300 then 299 mm is still CLEAR", &at300, 1, &under300, 0);
    rules_test_expect_hist("e06", "five ranges each lower is APPROACHING", fall, 4, &fifth, 1);
    rules_test_expect_hist("e06", "a hold ends the falling run, CLEAR", hold, 4, &held, 0);
    rules_test_expect_hist("e06", "status 4 at 240 mm is no range, CLEAR", &near_one, 1,
                           &near_bad, 0);
}
RULES_TEST_REGISTER(e06_cases);
