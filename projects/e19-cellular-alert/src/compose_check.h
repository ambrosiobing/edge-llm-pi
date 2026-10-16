/* compose_check.h: the field checker of project e19. A message the model composed is
 * checked against the event record it was composed from, field by field, before
 * anything is sent. Nothing here touches a modem, a file or a clock, so every case is
 * a function call in src/compose_check_test.c.
 *
 * The two gates of this project are deliberately separate. The send gate in
 * src/rules_e19.c decides whether anything may be sent at all, from the rule's index
 * and the time alone. This checker decides whether what the model wrote may be sent.
 * A message that passes the checker but was never armed by the rule is still not sent,
 * because the one call to the send path sits inside the branch the rule opened.
 */
#ifndef E19_COMPOSE_CHECK_H
#define E19_COMPOSE_CHECK_H

#include <stddef.h>

/* 160 characters is a single message body in the GSM 7 bit default alphabet. The
 * grammar's own maximum is 107 characters, which is lower on purpose: see
 * grammars/e19_message.gbnf and the chapter's latency design for the token arithmetic.
 * The checker still enforces 160, because the grammar bounds what the engine may
 * sample and the checker bounds what may leave the board. */
#define E19_SMS_MAX  160
#define E19_MSG_MAX  256

/* The single-septet subset of the GSM 7 bit default alphabet this project admits. A
 * character outside the default alphabet forces the body into UCS-2, where a single
 * message holds 70 characters and not 160; a character from the alphabet's extension
 * table costs two septets instead of one, which breaks the same arithmetic. Both are
 * refused here rather than counted. */
#define E19_SMS_CHARS \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:;!?()/+-"

typedef enum e19_verdict {
    E19_PASS = 0,
    E19_REFUSED_EMPTY,             /* nothing was composed, or only spaces */
    E19_REFUSED_TOO_LONG,          /* more than E19_SMS_MAX characters */
    E19_REFUSED_BAD_CHAR,          /* a character outside E19_SMS_CHARS */
    E19_REFUSED_NO_TYPE,           /* the event type of the record is not named */
    E19_REFUSED_WRONG_TYPE,        /* the other event type is named as well */
    E19_REFUSED_NO_PEAK,           /* the record's peak value is not carried */
    E19_REFUSED_NO_TIME,           /* the record's time stamp is not carried */
    E19_REFUSED_INVENTED_NUMBER,   /* a number that is in no field of the record */
    E19_VERDICT_COUNT
} e19_verdict;

/* One event, as the log holds it and as the message must carry it. peak and stamp are
 * the printed forms the message is checked against, so the checker never formats a
 * number itself and can never disagree with the log by a rounding step. */
typedef struct e19_record {
    const char *type;      /* the rule's literal for this event: TAP or FREEFALL */
    const char *other;     /* the literal this event is not, or NULL */
    double      peak_g;    /* the peak magnitude of the event, in g */
    char        peak[8];   /* the peak as the message must carry it, "d.dd" */
    char        stamp[16]; /* the time as the message must carry it, "hh:mm:ss" */
} e19_record;

/* Fills rec from the rule's literal, the peak in g and the printed time stamp. */
void e19_record_init(e19_record *rec, const char *type, double peak_g, const char *stamp);

/* The verdict. E19_PASS means every field of rec is in the message and no number in
 * the message is absent from rec. */
e19_verdict e19_compose_check(const char *message, const e19_record *rec);

const char *e19_verdict_name(e19_verdict v);

#endif /* E19_COMPOSE_CHECK_H */
