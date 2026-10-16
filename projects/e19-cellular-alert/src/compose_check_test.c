/* compose_check_test.c: the cases of project e19's two gates, as one test binary with
 * its own exit status. Registered as the ctest test e19_compose_check in this project's
 * CMakeLists.txt.
 *
 * Thirteen cases on the field checker, including the two that are the reason it exists:
 * a number the event never carried, and a peak value that is only in the message as
 * part of a longer number. Six cases on the send gate, including the boundary at
 * exactly the minimum interval. Every case names what it asserts, so a failure line
 * reads as a sentence.
 *
 * rule_e19_sends is declared here rather than included: rules_e19.c is compiled a
 * second time into the shared harness tests/rules_test, whose include path carries the
 * core headers alone, so it can own no header of its own. The declaration below and
 * the one in e19_main.c are the two places that have to agree with it.
 */
#include "compose_check.h"

#include <stdio.h>
#include <string.h>

int rule_e19_sends(int index, double t_s, double t_last_arm_s);

static int failures;
static int checks;

static void expect(const char *label, e19_verdict got, e19_verdict want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL %s: expected %s, got %s\n", label, e19_verdict_name(want),
               e19_verdict_name(got));
    }
}

static void expect_gate(const char *label, int got, int want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL %s: expected %d, got %d\n", label, want, got);
    }
}

/* The record every message below is checked against: a TAP whose peak was 2.47 g,
 * five seconds into the run. */
static void tap_record(e19_record *rec)
{
    e19_record_init(rec, "TAP", 2.47, "00:00:05");
}

static void field_cases(void)
{
    e19_record tap, fall;
    char big[256];
    size_t n;

    tap_record(&tap);
    e19_record_init(&fall, "FREEFALL", 0.21, "00:00:20");

    expect("a message with all three fields passes",
           e19_compose_check("ALERT TAP peak 2.47 g at t+00:00:05. Single knock, no "
                             "further event.", &tap), E19_PASS);
    expect("an empty message is refused", e19_compose_check("", &tap), E19_REFUSED_EMPTY);
    expect("a message of spaces is refused", e19_compose_check("    ", &tap),
           E19_REFUSED_EMPTY);

    /* 161 characters: the fixed part is 37, so 124 letters and the full stop take it
     * one past the limit of a single message body. */
    n = (size_t)snprintf(big, sizeof(big), "ALERT TAP peak 2.47 g at t+00:00:05. ");
    memset(big + n, 'A', 161 - n - 1);
    big[160] = '.';
    big[161] = '\0';
    expect("161 characters are refused", e19_compose_check(big, &tap),
           E19_REFUSED_TOO_LONG);

    expect("a character outside the SMS set is refused",
           e19_compose_check("ALERT TAP peak 2.47 g at t+00:00:05. Knock ~ table.", &tap),
           E19_REFUSED_BAD_CHAR);
    expect("a message that does not name the event type is refused",
           e19_compose_check("ALERT event peak 2.47 g at t+00:00:05. Nothing more.", &tap),
           E19_REFUSED_NO_TYPE);
    expect("a message that also names the other event type is refused",
           e19_compose_check("ALERT TAP or FREEFALL peak 2.47 g at t+00:00:05. Unclear.",
                             &tap), E19_REFUSED_WRONG_TYPE);
    expect("the skeleton with empty fields, which the stub answers, is refused",
           e19_compose_check("ALERT TAP peak . g at t+::. .", &tap), E19_REFUSED_NO_PEAK);
    expect("a peak the event did not have is refused",
           e19_compose_check("ALERT TAP peak 2.74 g at t+00:00:05. Knock.", &tap),
           E19_REFUSED_NO_PEAK);
    expect("a time the event did not have is refused",
           e19_compose_check("ALERT TAP peak 2.47 g at t+00:00:06. Knock.", &tap),
           E19_REFUSED_NO_TIME);
    expect("a number in no field of the event is refused",
           e19_compose_check("ALERT TAP peak 2.47 g at t+00:00:05. 3 events in the last "
                             "minute.", &tap), E19_REFUSED_INVENTED_NUMBER);
    expect("a peak that is only part of a longer number is refused",
           e19_compose_check("ALERT TAP peak 12.47 g at t+00:00:05. Knock.", &tap),
           E19_REFUSED_INVENTED_NUMBER);
    expect("a TAP message for a FREEFALL event is refused",
           e19_compose_check("ALERT TAP peak 0.21 g at t+00:00:20. Dropped.", &fall),
           E19_REFUSED_NO_TYPE);
    expect("a FREEFALL message for a FREEFALL event passes",
           e19_compose_check("ALERT FREEFALL peak 0.21 g at t+00:00:20. Dropped from the "
                             "bench.", &fall), E19_PASS);
}

static void gate_cases(void)
{
    expect_gate("NONE never arms a send", rule_e19_sends(0, 5.0, -1.0), 0);
    expect_gate("TILT never arms a send", rule_e19_sends(2, 5.0, -1.0), 0);
    expect_gate("the first TAP arms a send", rule_e19_sends(1, 5.0, -1.0), 1);
    expect_gate("a FREEFALL 15 s after the last arming arms a send",
                rule_e19_sends(3, 20.0, 5.0), 1);
    expect_gate("a TAP 2 s after the last arming does not arm a send",
                rule_e19_sends(1, 7.0, 5.0), 0);
    expect_gate("a TAP at exactly the minimum interval arms a send",
                rule_e19_sends(1, 15.0, 5.0), 1);
}

int main(void)
{
    field_cases();
    gate_cases();
    printf("e19_compose_check: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
