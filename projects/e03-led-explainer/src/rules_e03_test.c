/* rules_e03_test.c: eight cases on the e03 rule for the shared harness tests/rules_test.
 * Two per colour, the three boundary values that stay in the lower band (60.0, 80.0 and
 * 10.0), a short event, and a history that must not change the answer. The expected
 * value is the index into the rule's name table, which is the LED index in
 * e03_main.c and the grammar's literal order: 0 GREEN, 1 YELLOW, 2 RED.
 */
#include "rules_test.h"

static void e03_cases(void)
{
    io_event quiet   = { 0.0, "state", 2, { 42.00,  0.00 } };
    io_event at60    = { 1.0, "state", 2, { 60.00,  2.00 } };
    io_event over60  = { 2.0, "state", 2, { 60.01,  0.00 } };
    io_event at80    = { 3.0, "state", 2, { 80.00, 10.00 } };
    io_event over80  = { 4.0, "state", 2, { 80.01, -3.00 } };
    io_event steep   = { 5.0, "state", 2, { 20.00, 10.50 } };
    io_event short1  = { 6.0, "state", 1, { 95.00 } };
    io_event hist[2] = { { 7.0, "state", 2, { 90.00, 12.00 } },
                         { 8.0, "state", 2, { 85.00, -5.00 } } };

    rules_test_expect("e03", "quiet level, no slope, lights GREEN", &quiet, 0);
    rules_test_expect("e03", "level exactly 60 still lights GREEN", &at60, 0);
    rules_test_expect("e03", "level just above 60 lights YELLOW", &over60, 1);
    rules_test_expect("e03", "level 80 with slope 10 still lights YELLOW", &at80, 1);
    rules_test_expect("e03", "level above 80 lights RED", &over80, 2);
    rules_test_expect("e03", "slope above 10 at a low level lights RED", &steep, 2);
    rules_test_expect("e03", "an event with one value lights GREEN, not a guess", &short1, 0);
    rules_test_expect_hist("e03", "two RED events before do not change a GREEN one",
                           hist, 2, &quiet, 0);
}
RULES_TEST_REGISTER(e03_cases);
