/* rules_e16_test.c: six cases on the e16 rule for the shared harness tests/rules_test.
 * Every case is one scenario row in the shape of replay/profiles_100.csv: v0 the sleep
 * floor in microamps, v1 the charge of one wake in microamp hours, v2 the length of
 * that wake in seconds, v3 the cell in milliamp hours, v4 the battery target in days,
 * v5 the window the network granted in seconds. The expected value is the index into
 * the rule's name table, which is also the grammar's literal order: 0 I60, 1 I300,
 * 2 I900, 3 I3600.
 *
 * The lives below are worked by hand with the model of src/battery.h and are repeated
 * in src/battery_test.c as arithmetic, so a change to either file breaks both tests
 * rather than quietly moving the agreement table.
 */
#include "rules_test.h"

static void e16_cases(void)
{
    /* The reference profile: a 10 uA floor, a wake of 50 uAh over 20 s, a 2000 mAh
     * cell. Its life is 1390 days at 3600 s, 397 days at 900 s, 137 days at 300 s and
     * 28 days at 60 s, so the target and the grant are what move the decision. */
    io_event hour     = { 0.0, "ppk2_cycle", 6, { 10.0, 50.0, 20.0, 2000.0,  300.0, 3600.0 } };
    io_event quarter  = { 1.0, "ppk2_cycle", 6, { 10.0, 50.0, 20.0, 2000.0,  300.0,  900.0 } };
    io_event five_min = { 2.0, "ppk2_cycle", 6, { 10.0, 50.0, 20.0, 2000.0,  120.0,  300.0 } };
    io_event too_tight = { 3.0, "ppk2_cycle", 6, { 60.0, 80.0, 20.0, 1200.0, 2000.0, 3600.0 } };
    io_event no_grant  = { 4.0, "ppk2_cycle", 6, { 10.0, 50.0, 20.0, 2000.0,  300.0,    0.0 } };
    io_event degenerate = { 5.0, "ppk2_cycle", 6, { 100.0, 0.5, 30.0, 2000.0, 10.0, 3600.0 } };

    rules_test_expect("e16", "1390 days at an hour, target 300, grant an hour: I3600",
                      &hour, 3);
    rules_test_expect("e16", "the same profile with a 900 s grant: I900", &quarter, 2);
    rules_test_expect("e16", "the same profile, target 120, 300 s grant: I300",
                      &five_min, 1);
    rules_test_expect("e16", "358 days against a 2000 day target: I60, the fallback",
                      &too_tight, 0);
    rules_test_expect("e16", "no window granted, so no interval fits: I60", &no_grant, 0);
    rules_test_expect("e16", "a wake cheaper than its own span at the floor is refused: I60",
                      &degenerate, 0);
}
RULES_TEST_REGISTER(e16_cases);
