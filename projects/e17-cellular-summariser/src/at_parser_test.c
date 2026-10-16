/* at_parser_test.c: eleven cases on the AT parser of project e17, run by ctest as
 * e17-at-parser. No port, no modem, no clock: every case is a list of lines and a
 * number the caller passes as the clock, which is why these are the cases that make a
 * summary trustworthy rather than a bench session that happened to work once.
 *
 *     ./build/projects/e17-cellular-summariser/e17_at_parser_test
 *
 * Exit 0 when every case passes; otherwise one line per failure and exit 1. The
 * transcripts are the shapes the SIM7070G produces: the echo, the answer, the final
 * result code, and unsolicited lines wherever a modem is free to put them, which is
 * anywhere. Case 4 is the one the parser exists for, case 9 the defect it prevents,
 * case 10 the timeout that a modem with no answer must produce rather than a wait that
 * never ends, and case 11 the full round of four queries the sampler asks each minute.
 */
#include "at_parser.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

static void check(int condition, const char *label)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL %s\n", label);
    }
}

static void check_int(int got, int want, const char *label)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL %s: expected %d, got %d\n", label, want, got);
    }
}

static void check_text(const char *got, const char *want, const char *label)
{
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("FAIL %s: expected '%s', got '%s'\n", label, want, got);
    }
}

/* Feeds a NULL terminated list of lines to one open query and returns the last
 * result the parser gave, so a case reads as the transcript it is. */
static at_result feed(at_parser *p, at_query q, const char *const *lines)
{
    at_result last = AT_MORE;
    int i;
    at_parser_begin(p, q, 0.0, 2.0);
    for (i = 0; lines[i] != NULL; i++)
        last = at_parser_line(p, lines[i]);
    return last;
}

/* 1, the plain signal quality answer: echo, answer, final result code. */
static void case_csq_plain(void)
{
    static const char *const lines[] = { "AT+CSQ", "+CSQ: 18,99", "", "OK", NULL };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_CSQ, lines), AT_COMPLETE, "1 csq completes");
    check_int(at_parser_sample(&p)->csq, 18, "1 csq value");
    check_int(at_parser_sample(&p)->ber, 99, "1 ber value");
    check_int(at_parser_sample(&p)->urcs, 0, "1 no urcs");
}

/* 2, CSQ 99 is the modem's "unknown", and the parser records it as the number it is;
 * reading it as a strong signal is the rule's defect to avoid, not the parser's. */
static void case_csq_unknown(void)
{
    static const char *const lines[] = { "AT+CSQ", "+CSQ: 99,99", "OK", NULL };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_CSQ, lines), AT_COMPLETE, "2 csq 99 completes");
    check_int(at_parser_sample(&p)->csq, AT_CSQ_NONE, "2 csq 99 recorded as unknown");
}

/* 3, the CEREG read response: the state is field 1 and the access technology field 4. */
static void case_cereg_read(void)
{
    static const char *const lines[] = {
        "AT+CEREG?", "+CEREG: 2,1,\"1A2B\",\"01234567\",7", "OK", NULL
    };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_CEREG, lines), AT_COMPLETE, "3 cereg completes");
    check_int(at_parser_sample(&p)->reg_stat, 1, "3 registered on the home network");
    check_int(at_parser_sample(&p)->act, 7, "3 access technology 7, Cat-M1");
}

/* 4, the case the parser exists for: an unsolicited line between the command and its
 * answer. The SIM ready notification is dropped and counted, and the operator answer
 * that follows it is the one that reaches the sample. */
static void case_urc_mid_response(void)
{
    static const char *const lines[] = {
        "AT+COPS?", "+CPIN: READY", "+COPS: 0,0,\"A1\",7", "OK", NULL
    };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_COPS, lines), AT_COMPLETE, "4 cops completes past a urc");
    check_text(at_parser_sample(&p)->oper, "A1", "4 operator from the answer");
    check_int(at_parser_sample(&p)->act, 7, "4 access technology from the answer");
    check_int(at_parser_sample(&p)->urcs, 1, "4 one urc counted and dropped");
}

/* 5, deregistered: +COPS carries the mode alone, so there is no operator name to
 * report and the sample keeps "none" rather than the previous name. */
static void case_cops_no_operator(void)
{
    static const char *const lines[] = { "AT+COPS?", "+COPS: 2", "OK", NULL };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_COPS, lines), AT_COMPLETE, "5 cops mode only completes");
    check_text(at_parser_sample(&p)->oper, "none", "5 no operator name");
}

/* 6, power saving mode on, with the two empty fields the read response carries and the
 * two quoted timer strings the network granted. */
static void case_cpsms(void)
{
    static const char *const lines[] = {
        "AT+CPSMS?", "+CPSMS: 1,,,\"00100001\",\"00000101\"", "OK", NULL
    };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_CPSMS, lines), AT_COMPLETE, "6 cpsms completes");
    check_int(at_parser_sample(&p)->psm, 1, "6 power saving mode on");
}

/* 7, an operator name with a space in it survives, because the quotes hide the
 * separator and the field reader honours them. */
static void case_quoted_operator(void)
{
    static const char *const lines[] = {
        "AT+COPS?", "+COPS: 0,0,\"A1 Telekom\",9", "OK", NULL
    };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_COPS, lines), AT_COMPLETE, "7 quoted operator completes");
    check_text(at_parser_sample(&p)->oper, "A1 Telekom", "7 operator with a space");
    check_int(at_parser_sample(&p)->act, 9, "7 access technology 9, NB-IoT");
}

/* 8, an error for the open query: the sample keeps the field unknown and the caller
 * is told, so a failed query is never logged as a reading. */
static void case_error(void)
{
    static const char *const lines[] = { "AT+CPSMS?", "+CME ERROR: 4", NULL };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_CPSMS, lines), AT_FAILED, "8 cme error reported");
    check_int(at_parser_sample(&p)->psm, AT_UNKNOWN, "8 power saving stays unknown");
    check(!at_sample_complete(at_parser_sample(&p)), "8 sample not complete");
}

/* 9, the defect this parser prevents. A registration URC arrives while the signal
 * quality query is open. Its first field is the state and its second a tracking area
 * code, so a parser that reads the next line as the answer, or reads field 1 of this
 * line as a state, writes 1A2B into the registration state. Here it is dropped. */
static void case_cereg_urc_is_not_an_answer(void)
{
    static const char *const lines[] = {
        "AT+CSQ", "+CEREG: 1,\"1A2B\",\"01234567\",7", "+CSQ: 14,99", "OK", NULL
    };
    at_parser p;
    at_parser_init(&p);
    check_int(feed(&p, AT_Q_CSQ, lines), AT_COMPLETE, "9 csq completes");
    check_int(at_parser_sample(&p)->csq, 14, "9 signal quality from the answer");
    check_int(at_parser_sample(&p)->reg_stat, AT_UNKNOWN, "9 registration untouched");
    check_int(at_parser_sample(&p)->urcs, 1, "9 the registration urc counted");
    /* And the same line is read as the answer when the CEREG query is the open one
     * and the line carries the read form's two leading integers. */
    {
        static const char *const read_form[] = { "+CEREG: 2,5,\"1A2B\",\"01234567\",7", NULL };
        at_parser q;
        at_parser_init(&q);
        check_int(feed(&q, AT_Q_CEREG, read_form), AT_MORE, "9 read form consumed");
        check_int(at_parser_sample(&q)->reg_stat, 5, "9 roaming from the read form");
    }
}

/* 10, a modem that answers nothing: the open query times out at its own budget, and
 * the sample is not complete, which is what the sampler escalates on. */
static void case_timeout(void)
{
    at_parser p;
    at_parser_init(&p);
    at_parser_begin(&p, AT_Q_CSQ, 100.0, 2.0);
    check(!at_parser_timed_out(&p, 101.5), "10 not timed out before the budget");
    check(at_parser_timed_out(&p, 102.0), "10 timed out at the budget");
    check(at_parser_timed_out(&p, 180.0), "10 still timed out later");
    check(!at_sample_complete(at_parser_sample(&p)), "10 sample not complete");
    check_text(at_query_command(AT_Q_CEREG), "AT+CEREG?", "10 the query's command text");
}

/* All four queries in the order the sampler asks them: the sample is complete only
 * when every one of the four has answered. */
static void case_full_round(void)
{
    static const char *const csq[] = { "AT+CSQ", "+CSQ: 21,99", "OK", NULL };
    static const char *const reg[] = { "+CEREG: 2,1,\"1A2B\",\"01234567\",7", "OK", NULL };
    static const char *const ops[] = { "+COPS: 0,0,\"Magenta\",7", "OK", NULL };
    static const char *const psm[] = { "+CPSMS: 1,,,\"00100001\",\"00000101\"", "OK", NULL };
    at_parser p;
    at_parser_init(&p);
    feed(&p, AT_Q_CSQ, csq);
    check(!at_sample_complete(at_parser_sample(&p)), "round incomplete after one query");
    feed(&p, AT_Q_CEREG, reg);
    feed(&p, AT_Q_COPS, ops);
    feed(&p, AT_Q_CPSMS, psm);
    check(at_sample_complete(at_parser_sample(&p)), "round complete after four queries");
    check_int(at_parser_sample(&p)->csq, 21, "round signal quality");
    check_text(at_parser_sample(&p)->oper, "Magenta", "round operator");
    check_int(at_parser_sample(&p)->psm, 1, "round power saving mode");
}

int main(void)
{
    case_csq_plain();
    case_csq_unknown();
    case_cereg_read();
    case_urc_mid_response();
    case_cops_no_operator();
    case_cpsms();
    case_quoted_operator();
    case_error();
    case_cereg_urc_is_not_an_answer();
    case_timeout();
    case_full_round();
    printf("e17 at_parser: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
