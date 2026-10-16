/* rules_e04_test.c: six cases on the e04 rule for the shared harness tests/rules_test,
 * two per decision, including the two boundary temperatures that are deliberately not
 * in the upper band (65.000 C and 80.000 C) and one event where the temperature alone
 * would say WARM but bit 2 of the throttle word says THROTTLED. v0 is millidegrees, v1
 * the throttle word as a number, v2 the ARM clock in hertz. The expected value is the
 * index into the rule's name table, which is also the grammar's literal order.
 */
#include "rules_test.h"

static void e04_cases(void)
{
    io_event cool    = {   0.0, "thermal_zone0/temp", 3, { 42000.0,      0.0, 1500000000.0 } };
    io_event at65    = {  60.0, "thermal_zone0/temp", 3, { 65000.0,      0.0, 1500000000.0 } };
    io_event over65  = { 120.0, "thermal_zone0/temp", 3, { 65001.0,      0.0, 1500000000.0 } };
    io_event at80    = { 180.0, "thermal_zone0/temp", 3, { 80000.0,      0.0, 1500000000.0 } };
    io_event over80  = { 240.0, "thermal_zone0/temp", 3, { 80001.0,      0.0, 1500000000.0 } };
    io_event bit2    = { 300.0, "thermal_zone0/temp", 3, { 71000.0, 262148.0, 1000000000.0 } };

    rules_test_expect("e04", "42 C with no bits is COOL", &cool, 0);
    rules_test_expect("e04", "exactly 65 C is still COOL", &at65, 0);
    rules_test_expect("e04", "just above 65 C is WARM", &over65, 1);
    rules_test_expect("e04", "exactly 80 C with no bits is still WARM", &at80, 1);
    rules_test_expect("e04", "just above 80 C is THROTTLED", &over80, 2);
    rules_test_expect("e04", "71 C with bit 2 set (0x40004) is THROTTLED", &bit2, 2);
}
RULES_TEST_REGISTER(e04_cases);
