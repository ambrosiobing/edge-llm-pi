/* rules_e00_test.c: cases for the smoke rule, the shape every rules_eNN_test.c follows. */
#include "rules_test.h"

static void e00_cases(void)
{
    io_event cool = { 0.0, "thermal_zone0/temp", 1, { 45.0 } };
    io_event warm = { 1.0, "thermal_zone0/temp", 1, { 61.0 } };
    io_event hot = { 2.0, "thermal_zone0/temp", 1, { 80.5 } };
    io_event edge = { 3.0, "thermal_zone0/temp", 1, { 80.0 } };
    io_event empty = { 4.0, "thermal_zone0/temp", 0, { 0.0 } };

    rules_test_expect("e00", "45 C is COOL", &cool, 0);
    rules_test_expect("e00", "61 C is WARM", &warm, 1);
    rules_test_expect("e00", "80.5 C is HOT", &hot, 2);
    rules_test_expect("e00", "exactly 80 C is still WARM", &edge, 1);
    rules_test_expect("e00", "no value is COOL", &empty, 0);
    rules_test_expect_hist("e00", "history does not change the answer", &hot, 1, &cool, 0);
}
RULES_TEST_REGISTER(e00_cases);
