/* at_parser.c: the AT reply parser of the cellular projects. Pure text, no file
 * descriptor and no timing, so it is testable on a host with no modem and reusable by
 * every project that drives a SIMCom module over a character device.
 *
 * Where this file belongs. Project e17, the cellular status summariser, is the project
 * that owns the parser, and e19 was written to list e17's file as a source rather than
 * copy it. At the time e19 was written projects/e17-cellular-summariser did not exist
 * yet, so the parser is here, and the moment e17 is written this file moves to
 * projects/e17-cellular-summariser/src/ and e19's CMakeLists.txt lists it by its path
 * there. Two copies of an AT parser in one repository is the outcome to avoid; one
 * file listed twice is the shape chosen.
 *
 * What a reply looks like. A SIMCom module answers a command with echo (when ATE1 is
 * set), zero or more information lines, and one final result code, each line wrapped in
 * CR LF. An unsolicited line can arrive between any two of them. A send command is the
 * exception: AT+CMGS answers with the prompt "> ", which carries no line ending at all,
 * and the body is then written followed by CTRL-Z.
 *
 *     AT+CSQ\r
 *     \r\n+CSQ: 18,99\r\n
 *     \r\nOK\r\n
 *
 * So the parser does two things and nothing else: it cuts a byte stream into lines,
 * and it says what one line is. The caller owns the port, the deadlines and the retry.
 */
#include "at_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void at_lines_init(at_lines *a)
{
    memset(a, 0, sizeof(*a));
}

/* Appends n bytes and returns the number of complete lines now waiting. A line ends at
 * CR or LF, and an empty line is dropped: the CR LF pairs that wrap every reply would
 * otherwise double every count. The prompt "> " ends no line, so it is recognised here
 * on the partial buffer and handed over as a line of its own. */
int at_lines_feed(at_lines *a, const char *bytes, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        char c = bytes[i];
        if (c == '\r' || c == '\n') {
            if (a->fill > 0) {
                a->buf[a->fill] = '\0';
                at_lines_store(a, a->buf);
                a->fill = 0;
            }
            continue;
        }
        if (a->fill + 1 < AT_LINE_MAX)
            a->buf[a->fill++] = c;
        if (a->fill == 2 && a->buf[0] == '>' && a->buf[1] == ' ') {
            a->fill = 0;
            at_lines_store(a, "> ");
        }
    }
    return a->n;
}

/* Keeps the newest AT_LINES_MAX lines. An overrun drops the oldest and is counted, so
 * a flood of unsolicited lines is visible in the summary instead of being silent. */
void at_lines_store(at_lines *a, const char *line)
{
    if (a->n >= AT_LINES_MAX) {
        int i;
        for (i = 1; i < AT_LINES_MAX; i++)
            memcpy(a->line[i - 1], a->line[i], AT_LINE_MAX);
        a->n = AT_LINES_MAX - 1;
        a->dropped++;
    }
    snprintf(a->line[a->n], AT_LINE_MAX, "%s", line);
    a->n++;
}

/* Pops the oldest waiting line into out, or returns 0 when none is waiting. */
int at_lines_pop(at_lines *a, char *out, size_t len)
{
    int i;
    if (a->n <= 0)
        return 0;
    snprintf(out, len, "%s", a->line[0]);
    for (i = 1; i < a->n; i++)
        memcpy(a->line[i - 1], a->line[i], AT_LINE_MAX);
    a->n--;
    return 1;
}

/* What one line is. The order matters: the two error forms carry a code and must be
 * read before the bare ERROR, and an information line is anything that starts with
 * '+' and is not an error. Everything else, echo included, is AT_OTHER. */
at_kind at_classify(const char *line)
{
    if (line == NULL || line[0] == '\0')
        return AT_OTHER;
    if (strcmp(line, "> ") == 0)
        return AT_PROMPT;
    if (strcmp(line, "OK") == 0)
        return AT_OK;
    if (strncmp(line, "+CME ERROR", 10) == 0)
        return AT_CME_ERROR;
    if (strncmp(line, "+CMS ERROR", 10) == 0)
        return AT_CMS_ERROR;
    if (strcmp(line, "ERROR") == 0)
        return AT_ERROR;
    if (line[0] == '+')
        return AT_INFO;
    return AT_OTHER;
}

/* The integers after the first ':' of an information line, at most want of them.
 * "+CSQ: 18,99" gives 18 and 99; "+CREG: 2,1" gives 2 and 1; "+CMGS: 42" gives 42. A
 * field that is a quoted string, as in +CREG's third and fourth, is skipped, so a
 * caller asking for two values from a long +CREG line still gets n and stat. Returns
 * how many were read. */
int at_parse_ints(const char *line, int *out, int want)
{
    const char *p = strchr(line, ':');
    int got = 0;
    if (p == NULL)
        return 0;
    for (p++; *p != '\0' && got < want; p++) {
        if (*p == '"') {                       /* a quoted field: skip it whole */
            p++;
            while (*p != '\0' && *p != '"')
                p++;
            if (*p == '\0')
                break;
            continue;
        }
        if (*p == '-' || (*p >= '0' && *p <= '9')) {
            char *end;
            long v = strtol(p, &end, 10);
            if (end == p)
                continue;
            out[got++] = (int)v;
            p = end - 1;
        }
    }
    return got;
}

/* Registration from a +CREG or +CEREG line, in either form: the query answer
 * "+CREG: <n>,<stat>" and the unsolicited "+CREG: <stat>". Status 1 is registered on
 * the home network and 5 is registered while roaming; 2 is searching, which is the
 * mid-attach state the send path queues through. Returns 1 when registered, 0 when
 * not, and -1 when the line is not a registration line. */
int at_parse_registered(const char *line)
{
    int v[2], n;
    int stat;
    if (strncmp(line, "+CREG:", 6) != 0 && strncmp(line, "+CEREG:", 7) != 0)
        return -1;
    n = at_parse_ints(line, v, 2);
    if (n <= 0)
        return -1;
    stat = (n == 1) ? v[0] : v[1];
    return (stat == 1 || stat == 5) ? 1 : 0;
}

/* The message reference of a sent message, from "+CMGS: <mr>". Returns 0 and writes
 * mr, or -1. The reference is the module's own counter and is the only thing a send
 * returns; it is not a delivery receipt, which is why the chapter reports delivery as
 * not measured without a second phone. */
int at_parse_cmgs(const char *line, int *mr)
{
    int v[1];
    if (strncmp(line, "+CMGS:", 6) != 0 || at_parse_ints(line, v, 1) != 1)
        return -1;
    *mr = v[0];
    return 0;
}

const char *at_kind_name(at_kind k)
{
    switch (k) {
    case AT_OK:        return "OK";
    case AT_ERROR:     return "ERROR";
    case AT_CME_ERROR: return "CME ERROR";
    case AT_CMS_ERROR: return "CMS ERROR";
    case AT_PROMPT:    return "prompt";
    case AT_INFO:      return "info";
    default:           return "other";
    }
}
