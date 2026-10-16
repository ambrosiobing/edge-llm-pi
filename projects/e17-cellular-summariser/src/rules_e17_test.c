/* rules_e17_test.c: six cases on the e17 rule for the shared harness tests/rules_test,
 * two per decision. The event is one status sample: v0 the signal quality from AT+CSQ,
 * v1 its bit error rate, v2 the registration state from AT+CEREG?, v3 the access
 * technology, v4 the power saving mode. The expected value is the index into the rule's
 * name table, which is also the order of the grammar's literals: 0 OK, 1 DEGRADED,
 * 2 DOWN.
 *
 * Three of the six are the cases that have a wrong answer that looks right: a signal of
 * exactly 10, which the acceptance condition includes; a signal of 99, which is the
 * modem's "unknown" and not a strong signal; and a strong signal while registration was
 * denied, which is the state a modem sits in with the wrong subscription.
 */
#include "rules_test.h"

static void e17_cases(void)
{
    io_event home     = { 0.0, "A1",      5, { 18.0, 99.0, 1.0, 7.0, 1.0 } };
    io_event at_ten   = { 60.0, "Magenta", 5, { 10.0, 99.0, 5.0, 7.0, 1.0 } };
    io_event fading   = { 120.0, "A1",     5, {  9.0, 99.0, 1.0, 7.0, 1.0 } };
    io_event unknown  = { 180.0, "A1",     5, { 99.0, 99.0, 1.0, 7.0, 1.0 } };
    io_event denied   = { 240.0, "none",   5, { 31.0, 99.0, 3.0, 7.0, 0.0 } };
    io_event partial  = { 300.0, "none",   2, { 21.0, 99.0 } };

    rules_test_expect("e17", "registered at home with signal 18 is OK", &home, 0);
    rules_test_expect("e17", "roaming with signal exactly 10 is OK", &at_ten, 0);
    rules_test_expect("e17", "registered with signal 9 is DEGRADED", &fading, 1);
    rules_test_expect("e17", "signal 99 means unknown, so DEGRADED not OK", &unknown, 1);
    rules_test_expect("e17", "registration denied with signal 31 is DOWN", &denied, 2);
    rules_test_expect("e17", "a sample without a registration field is DOWN", &partial, 2);
}
RULES_TEST_REGISTER(e17_cases);
