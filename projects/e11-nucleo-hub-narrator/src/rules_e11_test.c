/* rules_e11_test.c: six cases on the e11 rule for the shared harness tests/rules_test.
 * Two per decision, holding both boundary values: a low-g magnitude of exactly 2 g is
 * still QUIET, and a high-g peak-hold of exactly 16 g with the low-g channel saturated
 * at its 16 g full scale is still LOW_G_EVENT. The values are v0 sequence, v1 to v3
 * low-g in g (gravity on z at rest), v4 the high-g peak-hold in g. The expected value is
 * the index into the rule's name table, which is also the grammar's literal order.
 */
#include "rules_test.h"

static void e11_cases(void)
{
    io_event rest     = { 0.000000, "replay", 5, { 100.0,  0.003, -0.002, 1.004,  1.02 } };
    io_event at2g     = { 0.009615, "replay", 5, { 101.0,  0.000,  0.000, 2.000,  2.00 } };
    io_event over2g   = { 0.019231, "replay", 5, { 102.0,  1.200,  1.200, 1.200,  2.10 } };
    io_event at16g    = { 0.028846, "replay", 5, { 103.0, 16.000,  0.000, 0.000, 16.00 } };
    io_event over16g  = { 0.038462, "replay", 5, { 104.0, 16.000, 16.000, 0.000, 16.01 } };
    io_event shortfr  = { 0.048077, "replay", 4, { 105.0, 16.000, 16.000, 0.000 } };

    rules_test_expect("e11", "rest, gravity on z, is QUIET", &rest, 0);
    rules_test_expect("e11", "low-g magnitude exactly 2 g is still QUIET", &at2g, 0);
    rules_test_expect("e11", "low-g magnitude just above 2 g is LOW_G_EVENT", &over2g, 1);
    rules_test_expect("e11", "peak-hold exactly 16 g is still LOW_G_EVENT", &at16g, 1);
    rules_test_expect("e11", "peak-hold above 16 g is HIGH_G_EVENT", &over16g, 2);
    rules_test_expect("e11", "a frame without the peak-hold is QUIET", &shortfr, 0);
}
RULES_TEST_REGISTER(e11_cases);
