/* at_parser.c: the state machine behind at_parser.h. One open query, every line
 * classified before it is read, unsolicited lines counted and dropped.
 *
 * The classification order is the whole design and it is deliberately dull:
 *
 *   1. a blank line                      consumed
 *   2. a line that starts with AT        the echo of the command, consumed
 *   3. OK                                the open query is answered
 *   4. ERROR, +CME ERROR, +CMS ERROR     the open query failed
 *   5. the open query's own prefix       read into the sample, keep waiting for OK
 *   6. anything else                     unsolicited: counted, dropped
 *
 * Rule 5 carries the one subtlety worth the file: +CEREG has a read form that begins
 * with two integers, <n> then <stat>, and an unsolicited form that begins with one,
 * <stat>, followed by quoted fields. A parser that reads field 2 as the state turns
 * the URC's tracking area code into a registration state, which is a silent wrong
 * answer rather than an error. So a +CEREG line is read as the answer only when it
 * carries at least two leading integer fields, and anything else with that prefix
 * goes to rule 6 even while the CEREG query is open.
 */
#include "at_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define AT_FIELD_LEN 32

static const char *const command[AT_Q_COUNT] = {
    "AT+CSQ", "AT+CEREG?", "AT+COPS?", "AT+CPSMS?"
};
static const char *const prefix[AT_Q_COUNT] = {
    "+CSQ:", "+CEREG:", "+COPS:", "+CPSMS:"
};

/* The index is checked as unsigned so that an out-of-range query, AT_Q_COUNT included,
 * returns the empty string instead of reading past the table. */
const char *at_query_command(at_query q)
{
    return (unsigned)q < (unsigned)AT_Q_COUNT ? command[q] : "";
}

const char *at_query_prefix(at_query q)
{
    return (unsigned)q < (unsigned)AT_Q_COUNT ? prefix[q] : "";
}

/* ------------------------------------------------------------------ small helpers */

static const char *skip_space(const char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* Copies comma-separated field number index (counting from 0) of args into out,
 * without its surrounding quotes or spaces. Returns the length, 0 when the field is
 * absent or empty. Quotes hide commas, as they do in "+COPS: 0,0,"A1 Telekom",7". */
static size_t field(const char *args, int index, char *out, size_t len)
{
    int current = 0;
    int quoted = 0;
    size_t n = 0;
    const char *p = args;

    out[0] = '\0';
    for (; *p != '\0'; p++) {
        if (*p == '"') {
            quoted = !quoted;
            continue;
        }
        if (*p == ',' && !quoted) {
            if (current == index)
                break;
            current++;
            n = 0;
            continue;
        }
        if (current == index && n + 1 < len && !(n == 0 && *p == ' '))
            out[n++] = *p;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = '\0';
    return n;
}

/* 1 when the whole text is an optionally signed decimal integer. */
static int is_int(const char *text)
{
    const char *p = text;
    if (*p == '+' || *p == '-')
        p++;
    if (!isdigit((unsigned char)*p))
        return 0;
    while (isdigit((unsigned char)*p))
        p++;
    return *p == '\0';
}

/* The count of leading fields that are integers: 2 for the +CEREG read response
 * (<n>,<stat>), 1 for the unsolicited form (<stat>,"tac",...). */
static int leading_ints(const char *args)
{
    char buf[AT_FIELD_LEN];
    int i = 0;
    while (field(args, i, buf, sizeof(buf)) > 0 && is_int(buf))
        i++;
    return i;
}

static int field_int(const char *args, int index, int fallback)
{
    char buf[AT_FIELD_LEN];
    if (field(args, index, buf, sizeof(buf)) > 0 && is_int(buf)) {
        int value = 0;
        sscanf(buf, "%d", &value);
        return value;
    }
    return fallback;
}

/* ------------------------------------------------------------------ the sample */

void at_parser_init(at_parser *p)
{
    memset(p, 0, sizeof(*p));
    p->sample.csq = AT_UNKNOWN;
    p->sample.ber = AT_UNKNOWN;
    p->sample.reg_stat = AT_UNKNOWN;
    p->sample.act = AT_UNKNOWN;
    p->sample.psm = AT_UNKNOWN;
    snprintf(p->sample.oper, sizeof(p->sample.oper), "none");
    p->open = AT_Q_COUNT;
}

void at_parser_begin(at_parser *p, at_query q, double now_s, double timeout_s)
{
    p->open = q;
    p->waiting = 1;
    p->started_s = now_s;
    p->timeout_s = timeout_s;
}

int at_parser_timed_out(const at_parser *p, double now_s)
{
    return p->waiting && (now_s - p->started_s) >= p->timeout_s;
}

const at_sample *at_parser_sample(const at_parser *p)
{
    return &p->sample;
}

int at_sample_complete(const at_sample *s)
{
    return (s->fields & AT_F_ALL) == AT_F_ALL;
}

/* Reads one answer line of the open query into the sample. */
static void read_answer(at_parser *p, const char *args)
{
    at_sample *s = &p->sample;
    char buf[AT_FIELD_LEN];

    switch (p->open) {
    case AT_Q_CSQ:
        s->csq = field_int(args, 0, AT_UNKNOWN);
        s->ber = field_int(args, 1, AT_UNKNOWN);
        s->fields |= AT_F_CSQ;
        break;
    case AT_Q_CEREG:
        /* <n>,<stat>[,"tac","ci",<AcT>]: the state is field 1, never field 0. */
        s->reg_stat = field_int(args, 1, AT_UNKNOWN);
        s->act = field_int(args, 4, s->act);
        s->fields |= AT_F_REG;
        break;
    case AT_Q_COPS:
        /* <mode>[,<format>,"<oper>"[,<AcT>]]: no operator field means none. */
        if (field(args, 2, buf, sizeof(buf)) > 0)
            snprintf(s->oper, sizeof(s->oper), "%s", buf);
        s->act = field_int(args, 3, s->act);
        s->fields |= AT_F_OPER;
        break;
    case AT_Q_CPSMS:
        s->psm = field_int(args, 0, AT_UNKNOWN);
        s->fields |= AT_F_PSM;
        break;
    default:
        break;
    }
}

at_result at_parser_line(at_parser *p, const char *line)
{
    const char *text = skip_space(line);
    const char *pre;
    size_t pre_len;

    p->lines++;
    if (*text == '\0')
        return AT_MORE;
    if (strncmp(text, "AT", 2) == 0)
        return AT_MORE;                            /* the echo of our own command */
    if (strcmp(text, "OK") == 0) {
        if (!p->waiting)
            return AT_MORE;
        p->waiting = 0;
        return AT_COMPLETE;
    }
    if (strncmp(text, "ERROR", 5) == 0 || strncmp(text, "+CME ERROR", 10) == 0 ||
        strncmp(text, "+CMS ERROR", 10) == 0) {
        p->waiting = 0;
        return AT_FAILED;
    }
    pre = at_query_prefix(p->open);
    pre_len = strlen(pre);
    if (p->waiting && pre_len > 0 && strncmp(text, pre, pre_len) == 0) {
        const char *args = skip_space(text + pre_len);
        if (p->open != AT_Q_CEREG || leading_ints(args) >= 2) {
            read_answer(p, args);
            return AT_MORE;                        /* the final result code still follows */
        }
    }
    p->sample.urcs++;
    return AT_URC;
}
