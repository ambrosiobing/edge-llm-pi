/* at_parser.h: the AT response parser of project e17, one status sample at a time.
 *
 * Four queries are asked in a fixed order once per sample period, and their answers
 * are collected into one at_sample:
 *
 *     AT+CSQ      -> +CSQ: <rssi>,<ber>                     signal quality
 *     AT+CEREG?   -> +CEREG: <n>,<stat>[,"tac","ci",<AcT>]  EPS registration
 *     AT+COPS?    -> +COPS: <mode>,<format>,"<oper>",<AcT>  the operator the modem reports
 *     AT+CPSMS?   -> +CPSMS: <mode>,,,"<tau>","<active>"    power saving mode
 *
 * The only reason this file exists rather than four calls to sscanf is the third line
 * of every modem transcript: an unsolicited result code can arrive between the command
 * and its answer, and a parser that takes the next line as the answer then writes a
 * paging notification into the signal quality. So the parser is a state machine with
 * exactly one open query at a time, and every line is classified before it is read:
 * the echo, the expected prefix, a final result code, or an unsolicited line that is
 * counted and dropped. The hardest case is +CEREG, which has two forms: the read
 * response starts with two integers (<n>,<stat>) and the unsolicited form with one
 * (<stat>). The discriminator is the count of leading integer fields, and a URC that
 * arrives while the CEREG query is open is still dropped rather than read as the
 * answer, because its second field is a tracking area code and not a state.
 *
 * Nothing here opens a port, allocates or blocks: the caller feeds it lines and a
 * monotonic clock, which is what makes src/at_parser_test.c eleven cases rather
 * than a bench session.
 */
#ifndef E17_AT_PARSER_H
#define E17_AT_PARSER_H

#define AT_OPER_LEN  24   /* operator names are short; longer ones are truncated */
#define AT_UNKNOWN   (-1) /* no answer has arrived for this field yet */
#define AT_CSQ_NONE  99   /* the modem's own "unknown or not detectable", not excellent */

typedef enum at_query {
    AT_Q_CSQ = 0,
    AT_Q_CEREG,
    AT_Q_COPS,
    AT_Q_CPSMS,
    AT_Q_COUNT
} at_query;

/* Which fields of the sample have arrived; at_sample_complete() wants all four. */
#define AT_F_CSQ   0x1
#define AT_F_REG   0x2
#define AT_F_OPER  0x4
#define AT_F_PSM   0x8
#define AT_F_ALL   0xf

typedef struct at_sample {
    int  csq;                 /* 0 to 31, or AT_CSQ_NONE for unknown */
    int  ber;                 /* 0 to 7, or 99 */
    int  reg_stat;            /* +CEREG <stat>: 1 home, 5 roaming, 2 searching, 3 denied */
    int  act;                 /* access technology: 7 Cat-M1 (eMTC), 9 NB-IoT */
    int  psm;                 /* +CPSMS <mode>: 1 on, 0 off */
    char oper[AT_OPER_LEN];   /* the operator name as reported, "none" when there is none */
    int  fields;              /* the AT_F_* bits that have arrived */
    int  urcs;                /* unsolicited lines dropped while building this sample */
} at_sample;

typedef enum at_result {
    AT_MORE = 0,   /* the line was consumed; the open response is not finished */
    AT_COMPLETE,   /* the final result code arrived and the open query is answered */
    AT_FAILED,     /* ERROR, +CME ERROR or +CMS ERROR for the open query */
    AT_URC         /* an unsolicited line: counted, dropped, never read as the answer */
} at_result;

typedef struct at_parser {
    at_sample sample;
    at_query  open;        /* the query whose response is open */
    int       waiting;     /* 1 from at_parser_begin() until a final result code */
    double    started_s;   /* the clock the caller passed to at_parser_begin() */
    double    timeout_s;
    long      lines;       /* every line fed in, for the log */
} at_parser;

/* Clears the parser and the sample: every field AT_UNKNOWN, the operator "none". */
void at_parser_init(at_parser *p);

/* Opens one query. The caller writes at_query_command(q) to the port right after. */
void at_parser_begin(at_parser *p, at_query q, double now_s, double timeout_s);

/* Classifies and, where it is the open query's answer, reads one line. */
at_result at_parser_line(at_parser *p, const char *line);

/* 1 when the open query has not been answered within its timeout. */
int at_parser_timed_out(const at_parser *p, double now_s);

const at_sample *at_parser_sample(const at_parser *p);

/* 1 when all four fields have arrived, so the sample may be written as one event. */
int at_sample_complete(const at_sample *s);

const char *at_query_command(at_query q);   /* "AT+CSQ", "AT+CEREG?", ... */
const char *at_query_prefix(at_query q);    /* "+CSQ:", "+CEREG:", ... */

#endif /* E17_AT_PARSER_H */
