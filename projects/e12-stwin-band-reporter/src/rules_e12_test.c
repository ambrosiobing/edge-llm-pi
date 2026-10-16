/* rules_e12_test.c: six cases on the e12 rule for the shared harness tests/rules_test. The
 * event layout is the loop's: v0..v3 band energies, v4 total, v5 the watched share, v6 the
 * baseline share, v7 the sample rate, v8 dropped frames. Two cases are the two ways the
 * rule refuses to guess (a short event, a baseline not yet taken), one is the boundary
 * (exactly twice the baseline stays BASELINE, the comparison is strict), three are the
 * plain cases either side of it. The expected value is the index into the rule's name
 * table, which is also the grammar's literal order.
 */
#include "rules_test.h"

static void e12_cases(void)
{
    io_event short_ev = { 0.0, "bands", 5, { 3e5, 1e5, 6e4, 4e4, 5e5 } };
    io_event taking   = { 0.7, "bands", 9, { 3e5, 1e5, 6e4, 4e4, 5e5, 0.20, 0.00, 6000, 0 } };
    io_event quiet    = { 1.4, "bands", 9, { 3e5, 1e5, 6e4, 4e4, 5e5, 0.19, 0.20, 6000, 0 } };
    io_event at_twice = { 2.0, "bands", 9, { 2e5, 2e5, 6e4, 4e4, 5e5, 0.40, 0.20, 6000, 0 } };
    io_event over     = { 2.7, "bands", 9, { 2e5, 2e5, 6e4, 4e4, 5e5, 0.4001, 0.20, 6000, 0 } };
    io_event loud     = { 3.4, "bands", 9, { 1e5, 9e5, 6e4, 4e4, 1.1e6, 0.82, 0.05, 5990, 0 } };

    rules_test_expect("e12", "an event without a baseline column is BASELINE", &short_ev, 0);
    rules_test_expect("e12", "baseline still being taken (v6 is 0) is BASELINE", &taking, 0);
    rules_test_expect("e12", "share below the baseline is BASELINE", &quiet, 0);
    rules_test_expect("e12", "share exactly twice the baseline is still BASELINE", &at_twice, 0);
    rules_test_expect("e12", "share just above twice the baseline is BAND_SHIFT", &over, 1);
    rules_test_expect("e12", "share many times the baseline is BAND_SHIFT", &loud, 1);
}
RULES_TEST_REGISTER(e12_cases);
