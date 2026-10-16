/* at_parser.h: cut an AT reply stream into lines and say what one line is. No port, no
 * clock, no retry: those belong to the caller. See at_parser.c for the reply shapes
 * this covers and for the note on where the file belongs once project e17 exists.
 */
#ifndef E19_AT_PARSER_H
#define E19_AT_PARSER_H

#include <stddef.h>

#define AT_LINE_MAX  128
#define AT_LINES_MAX 16

typedef enum at_kind {
    AT_OTHER = 0,    /* echo, or a line this parser does not name */
    AT_OK,           /* the final result code of a command that succeeded */
    AT_ERROR,        /* the bare final result code of one that did not */
    AT_CME_ERROR,    /* +CME ERROR: <code>, an equipment error */
    AT_CMS_ERROR,    /* +CMS ERROR: <code>, a message service error */
    AT_PROMPT,       /* "> ", the body prompt of AT+CMGS; it ends no line */
    AT_INFO          /* an information line, "+CSQ: 18,99" */
} at_kind;

typedef struct at_lines {
    char line[AT_LINES_MAX][AT_LINE_MAX];
    int  n;                                /* complete lines waiting */
    int  dropped;                          /* lines lost to an overrun, reported */
    char buf[AT_LINE_MAX];                 /* the line being assembled */
    size_t fill;
} at_lines;

void at_lines_init(at_lines *a);
int  at_lines_feed(at_lines *a, const char *bytes, size_t n);
void at_lines_store(at_lines *a, const char *line);
int  at_lines_pop(at_lines *a, char *out, size_t len);

at_kind at_classify(const char *line);
int at_parse_ints(const char *line, int *out, int want);
int at_parse_registered(const char *line);
int at_parse_cmgs(const char *line, int *mr);
const char *at_kind_name(at_kind k);

#endif /* E19_AT_PARSER_H */
