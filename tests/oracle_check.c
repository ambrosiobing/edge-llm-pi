/* oracle_check.c: compares the rule and model columns of a produced log against an
 * expected CSV, row by row, with no Python in the path.
 *
 *     oracle_check <produced.csv> <expected.csv>
 *
 * Both files carry a header; the columns named "rule" and "model" are located by name
 * so the expected file may hold only t_s,tag,rule,model or the full log shape. Fields
 * are CSV: a field may be quoted with doubled inner quotes. Exit 0 when every row
 * agrees, 1 when a row differs (each difference is printed), 2 when the row counts
 * differ or a file cannot be read or lacks a column.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096
#define MAX_FIELDS 32

typedef struct row {
    char  line[MAX_LINE];
    char *field[MAX_FIELDS];
    int   n;
} row;

static int read_row(FILE *fp, row *r)
{
    char *p, *out;
    int in_quotes = 0;
    size_t n;

    if (fgets(r->line, sizeof(r->line), fp) == NULL)
        return 0;
    n = strlen(r->line);
    while (n > 0 && (r->line[n - 1] == '\n' || r->line[n - 1] == '\r'))
        r->line[--n] = '\0';
    r->n = 0;
    p = out = r->line;
    r->field[r->n++] = out;
    for (; *p != '\0'; p++) {
        if (in_quotes) {
            if (*p == '"' && p[1] == '"') {
                *out++ = '"';
                p++;
            } else if (*p == '"') {
                in_quotes = 0;
            } else {
                *out++ = *p;
            }
        } else if (*p == '"') {
            in_quotes = 1;
        } else if (*p == ',') {
            *out++ = '\0';
            if (r->n < MAX_FIELDS)
                r->field[r->n++] = out;
        } else {
            *out++ = *p;
        }
    }
    *out = '\0';
    return 1;
}

static int column(const row *header, const char *name)
{
    int i;
    for (i = 0; i < header->n; i++)
        if (strcmp(header->field[i], name) == 0)
            return i;
    return -1;
}

static const char *at(const row *r, int idx)
{
    return (idx >= 0 && idx < r->n) ? r->field[idx] : "";
}

int main(int argc, char **argv)
{
    FILE *got, *exp;
    row gh, eh, g, e;
    int g_rule, g_model, e_rule, e_model;
    long rows = 0, bad = 0;
    int have_g, have_e;

    if (argc != 3) {
        fprintf(stderr, "usage: oracle_check <produced.csv> <expected.csv>\n");
        return 2;
    }
    got = fopen(argv[1], "r");
    exp = fopen(argv[2], "r");
    if (got == NULL || exp == NULL) {
        fprintf(stderr, "oracle_check: cannot open %s\n", got == NULL ? argv[1] : argv[2]);
        return 2;
    }
    if (!read_row(got, &gh) || !read_row(exp, &eh)) {
        fprintf(stderr, "oracle_check: a file has no header\n");
        return 2;
    }
    g_rule = column(&gh, "rule");
    g_model = column(&gh, "model");
    e_rule = column(&eh, "rule");
    e_model = column(&eh, "model");
    if (g_rule < 0 || g_model < 0 || e_rule < 0 || e_model < 0) {
        fprintf(stderr, "oracle_check: both files need rule and model columns\n");
        return 2;
    }
    for (;;) {
        have_g = read_row(got, &g);
        have_e = read_row(exp, &e);
        if (!have_g || !have_e)
            break;
        rows++;
        if (strcmp(at(&g, g_rule), at(&e, e_rule)) != 0) {
            printf("row %ld: rule expected '%s' got '%s'\n", rows, at(&e, e_rule),
                   at(&g, g_rule));
            bad++;
        }
        if (strcmp(at(&g, g_model), at(&e, e_model)) != 0) {
            printf("row %ld: model expected '%s' got '%s'\n", rows, at(&e, e_model),
                   at(&g, g_model));
            bad++;
        }
    }
    fclose(got);
    fclose(exp);
    if (have_g != have_e) {
        printf("row count differs: produced has %s rows than expected after row %ld\n",
               have_g ? "more" : "fewer", rows);
        return 2;
    }
    printf("oracle_check: %ld rows, %ld differences\n", rows, bad);
    return bad == 0 ? 0 : 1;
}
