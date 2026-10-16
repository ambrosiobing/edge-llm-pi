/* rules_e07_test.c: six cases on the e07 rule for the shared harness tests/rules_test,
 * two per decision, with the two boundary values exactly 1.0 hPa up and 1.0 hPa down,
 * which the strict comparison keeps in STABLE. v[0] is the pressure now and v[1] the
 * pressure 30 minutes earlier, both in hPa; the other values of a real event are not
 * read by the rule and are left out. The expected value is the index into the rule's
 * name table, which is also the grammar's literal order.
 */
#include "rules_test.h"

static void e07_cases(void)
{
    io_event flat  = {   0.0, "iks4a1", 2, { 1013.20, 1013.20 } };
    io_event up1   = {  60.0, "iks4a1", 2, { 1001.00, 1000.00 } };
    io_event up    = { 120.0, "iks4a1", 2, { 1001.01, 1000.00 } };
    io_event down1 = { 180.0, "iks4a1", 2, {  999.00, 1000.00 } };
    io_event down  = { 240.0, "iks4a1", 2, {  998.99, 1000.00 } };
    io_event one   = { 300.0, "iks4a1", 1, { 1013.20 } };

    rules_test_expect("e07", "no change in 30 minutes is STABLE", &flat, 0);
    rules_test_expect("e07", "exactly 1.0 hPa up is still STABLE", &up1, 0);
    rules_test_expect("e07", "1.01 hPa up is RISING", &up, 1);
    rules_test_expect("e07", "exactly 1.0 hPa down is still STABLE", &down1, 0);
    rules_test_expect("e07", "1.01 hPa down is FALLING", &down, 2);
    rules_test_expect("e07", "an event without the old value is STABLE", &one, 0);
}
RULES_TEST_REGISTER(e07_cases);
