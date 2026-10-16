/* compose_check.c: the field checker of project e19, the one thing standing between a
 * message the model composed and the modem.
 *
 * Why it exists. A small model under a grammar cannot write a fourth event type and
 * cannot write a character the SMS body may not carry, because the grammar's automaton
 * refuses both. What the grammar cannot refuse is a plausible number: the skeleton has
 * a slot of the shape d.dd, and 2.74 satisfies the grammar exactly as well as 2.47
 * does. A message carrying a number that is in no field of the event is the failure
 * this file is written to catch, and the acceptance test of the chapter publishes the
 * rate at which it happens.
 *
 * What it does, in order, stopping at the first refusal:
 *
 *   1. empty          nothing was composed, or only spaces
 *   2. too long       more than E19_SMS_MAX characters
 *   3. bad character  a character outside the admitted subset of the GSM alphabet
 *   4. no type        the record's event type is not named in the message
 *   5. wrong type     the event type the record is not is named as well
 *   6. no peak        the record's printed peak value is nowhere in the message
 *   7. no time        the record's printed time stamp is nowhere in the message
 *   8. invented       some number in the message is in no field of the record, which
 *                     includes a peak that is only there as part of a longer number
 *
 * The order is what a reader of a refusal line needs: the cheapest and most structural
 * reason first, the one that costs a scan of every number in the message last. Every
 * refusal is logged with its reason and counted; no refusal is retried, because a
 * second sample of the same event is a second chance to invent a number and the rule's
 * minimum interval has already decided how often the board may speak.
 *
 * Nothing here formats a number. The record carries the printed peak and the printed
 * time stamp that the loop also wrote to the log, so a rounding step can never put the
 * checker and the log on different sides of an equality.
 */
#include "compose_check.h"

#include <stdio.h>
#include <string.h>

#define E19_RUN_MAX   32
#define E19_GROUPS    4

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* The groups of the time stamp, split at ':': "00:01:35" gives "00", "01", "35". A
 * number in the message is allowed when it is the peak or one of these. */
static int stamp_groups(const char *stamp, char out[E19_GROUPS][E19_RUN_MAX])
{
    int n = 0;
    size_t k = 0;
    const char *p;
    out[0][0] = '\0';
    for (p = stamp; n < E19_GROUPS; p++) {
        if (is_digit(*p)) {
            if (k + 1 < E19_RUN_MAX)
                out[n][k++] = *p;
            continue;
        }
        if (k > 0) {
            out[n][k] = '\0';
            n++;
            k = 0;
            if (n < E19_GROUPS)
                out[n][0] = '\0';
        }
        if (*p == '\0')
            break;
    }
    return n;
}

/* Copies the next maximal run of digits and full stops starting at or after *p into
 * run, advances *p past it, and returns 1 while a run was found. Leading and trailing
 * full stops are trimmed, because the full stop that ends a sentence is punctuation
 * and not part of a number. A run with no digit left in it is returned empty, and the
 * caller skips it. */
static int next_number(const char **p, char *run, size_t len)
{
    const char *s = *p;
    const char *b;
    size_t n, i = 0;
    while (*s != '\0' && !is_digit(*s) && *s != '.')
        s++;
    if (*s == '\0') {
        *p = s;
        return 0;
    }
    b = s;
    while (*s == '.' || is_digit(*s))
        s++;
    *p = s;
    n = (size_t)(s - b);
    while (n > 0 && *b == '.') {              /* trim the leading full stops */
        b++;
        n--;
    }
    while (n > 0 && b[n - 1] == '.')          /* trim the trailing full stops */
        n--;
    for (i = 0; i < n && i + 1 < len; i++)
        run[i] = b[i];
    run[i] = '\0';
    for (i = 0; run[i] != '\0'; i++)
        if (is_digit(run[i]))
            return 1;
    run[0] = '\0';                            /* punctuation only, not a number */
    return 1;
}

void e19_record_init(e19_record *rec, const char *type, double peak_g, const char *stamp)
{
    memset(rec, 0, sizeof(*rec));
    rec->type = type;
    rec->peak_g = peak_g;
    /* The other literal of the two the rule can arm. An event of any other kind is
     * never composed for, so there is nothing to confuse it with. */
    if (type != NULL && strcmp(type, "TAP") == 0)
        rec->other = "FREEFALL";
    else if (type != NULL && strcmp(type, "FREEFALL") == 0)
        rec->other = "TAP";
    else
        rec->other = NULL;
    snprintf(rec->peak, sizeof(rec->peak), "%.2f", peak_g);
    snprintf(rec->stamp, sizeof(rec->stamp), "%s", stamp != NULL ? stamp : "");
}

e19_verdict e19_compose_check(const char *message, const e19_record *rec)
{
    char groups[E19_GROUPS][E19_RUN_MAX];
    char run[E19_RUN_MAX];
    const char *p;
    size_t len, i;
    int n_groups, found_peak = 0, blank = 1;

    if (message == NULL || rec == NULL || rec->type == NULL)
        return E19_REFUSED_EMPTY;
    len = strlen(message);
    for (i = 0; i < len; i++)
        if (message[i] != ' ')
            blank = 0;
    if (len == 0 || blank)
        return E19_REFUSED_EMPTY;
    if (len > E19_SMS_MAX)
        return E19_REFUSED_TOO_LONG;
    for (i = 0; i < len; i++)
        if (strchr(E19_SMS_CHARS, message[i]) == NULL)
            return E19_REFUSED_BAD_CHAR;
    if (strstr(message, rec->type) == NULL)
        return E19_REFUSED_NO_TYPE;
    if (rec->other != NULL && strstr(message, rec->other) != NULL)
        return E19_REFUSED_WRONG_TYPE;
    if (rec->peak[0] == '\0' || strstr(message, rec->peak) == NULL)
        return E19_REFUSED_NO_PEAK;
    if (rec->stamp[0] == '\0' || strstr(message, rec->stamp) == NULL)
        return E19_REFUSED_NO_TIME;

    /* Every number in the message must be one of the record's: the peak, or one of the
     * groups of the time stamp. The peak must also stand on its own, so a message that
     * carries 12.47 where the event's peak was 2.47 is refused even though the peak is
     * a substring of it. This is the check the grammar cannot make. */
    n_groups = stamp_groups(rec->stamp, groups);
    p = message;
    while (next_number(&p, run, sizeof(run))) {
        int ok = 0, g;
        if (run[0] == '\0')
            continue;
        if (strcmp(run, rec->peak) == 0) {
            found_peak = 1;
            ok = 1;
        }
        for (g = 0; !ok && g < n_groups; g++)
            if (strcmp(run, groups[g]) == 0)
                ok = 1;
        if (!ok)
            return E19_REFUSED_INVENTED_NUMBER;
    }
    if (!found_peak)
        return E19_REFUSED_NO_PEAK;
    return E19_PASS;
}

const char *e19_verdict_name(e19_verdict v)
{
    switch (v) {
    case E19_PASS:                     return "sent";
    case E19_REFUSED_EMPTY:            return "refused: empty";
    case E19_REFUSED_TOO_LONG:         return "refused: too long";
    case E19_REFUSED_BAD_CHAR:         return "refused: character outside the SMS set";
    case E19_REFUSED_NO_TYPE:          return "refused: event type missing";
    case E19_REFUSED_WRONG_TYPE:       return "refused: the other event type named";
    case E19_REFUSED_NO_PEAK:          return "refused: peak value missing";
    case E19_REFUSED_NO_TIME:          return "refused: time missing";
    case E19_REFUSED_INVENTED_NUMBER:  return "refused: invented number";
    default:                           return "refused: unknown";
    }
}
