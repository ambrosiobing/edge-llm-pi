/* rules_e10_test.c: six cases on the e10 rule for the shared harness tests/rules_test,
 * two per label, including the two boundary values that stay in the lower label on
 * purpose: a swing of exactly 0.20 V is NOISE, and a steepest change of exactly 0.4 of
 * the swing is RAMP. The expected value is the index into the rule's name table,
 * which is also the grammar's literal order: 0 NOISE, 1 RAMP, 2 STEP.
 *
 * The first case carries all sixteen values a real window has; the others carry the
 * three the rule reads, because the points are the model's and not the rule's.
 */
#include "rules_test.h"

static void e10_cases(void)
{
    io_event flat    = { 0.000, "noise:0.000", 16, { 1.000, 1.010, 0.004, 1.00, 1.01, 1.00,
                                                     1.01, 1.00, 1.01, 1.00, 1.01, 1.00,
                                                     1.01, 1.00, 1.01, 1.00 } };
    io_event band    = { 0.002, "noise:0.000",  3, { 0.000, 0.200, 0.200 } };
    io_event slow    = { 0.004, "ramp:0.201",   3, { 0.000, 0.201, 0.010 } };
    io_event at04    = { 0.006, "ramp:1.000",   3, { 1.000, 2.000, 0.400 } };
    io_event sharp   = { 0.008, "step:1.000",   3, { 1.000, 2.000, 0.410 } };
    io_event short2  = { 0.010, "noise:0.000",  2, { 0.000, 3.000 } };

    rules_test_expect("e10", "a flat window with 4 mV of noise is NOISE", &flat, 0);
    rules_test_expect("e10", "a swing of exactly 0.20 V is still NOISE", &band, 0);
    rules_test_expect("e10", "a swing just over the band taken slowly is RAMP", &slow, 1);
    rules_test_expect("e10", "a steepest change of exactly 0.4 of the swing is RAMP", &at04, 1);
    rules_test_expect("e10", "a steepest change above 0.4 of the swing is STEP", &sharp, 2);
    rules_test_expect("e10", "an event without the three summaries is NOISE", &short2, 0);
}
RULES_TEST_REGISTER(e10_cases);
