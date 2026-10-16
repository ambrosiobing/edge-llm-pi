/* edgellm_stub.c: the same API as edgellm.c with no model behind it, for CI and for a
 * host without a GGUF file. The answer is fixed at open from the grammar alone.
 *
 * How the answer is built, exactly, because every oracle file depends on it: starting
 * at the rule named root, the FIRST alternative of the rule is walked left to right.
 * A quoted literal appends its text (escapes \" \\ \n \t \r \xHH resolved). A reference
 * to another rule of the grammar expands that rule's first alternative in place, to a
 * depth of 8. A parenthesised group expands its own first alternative. A character
 * class [...] and every repetition mark (*, +, ?, {m,n}) append nothing. Comments
 * after '#' are skipped. For the house grammar
 *
 *     root ::= "{" ws "\"decision\"" ws ":" ws decision ws "}"
 *     decision ::= "\"OK\"" | "\"DEGRADED\"" | "\"DOWN\""
 *     ws ::= [ \t]*
 *
 * the stub answers {"decision":"OK"}: the grammar's first literal on every path. A
 * sentence grammar made of character classes yields an empty answer, and the project
 * says so in its oracle file. Timings are 0, stub is 1, the engine version is "stub".
 */
#define _GNU_SOURCE
#include "edgellm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUB_DEPTH 8

struct edgellm_s {
    char answer[512];
};

static int is_ident(int c)
{
    return isalnum(c) || c == '_' || c == '-';
}

/* Finds "name ::=" at a line start. Sets *body_b after "::=" and *body_e at the start
 * of the next rule line or the end of the text. Returns 0 when found. */
static int find_rule(const char *g, const char *name, size_t name_len, const char **body_b,
                     const char **body_e)
{
    const char *line = g;
    const char *found = NULL;
    while (*line != '\0') {
        const char *p = line, *id;
        size_t id_len;
        while (*p == ' ' || *p == '\t')
            p++;
        id = p;
        while (is_ident((unsigned char)*p))
            p++;
        id_len = (size_t)(p - id);
        while (*p == ' ' || *p == '\t')
            p++;
        if (id_len > 0 && strncmp(p, "::=", 3) == 0) {
            if (found != NULL) {
                *body_e = line;
                return 0;
            }
            if (id_len == name_len && strncmp(id, name, name_len) == 0) {
                found = p + 3;
                *body_b = found;
            }
        }
        while (*line != '\0' && *line != '\n')
            line++;
        if (*line == '\n')
            line++;
    }
    if (found == NULL)
        return -1;
    *body_e = line;
    return 0;
}

static void put(char *out, size_t cap, size_t *len, char c)
{
    if (*len + 1 < cap) {
        out[*len] = c;
        out[++*len] = '\0';
    }
}

/* Reads a quoted literal starting at the opening quote; returns the position after
 * the closing quote. */
static const char *literal(const char *p, const char *e, char *out, size_t cap, size_t *len)
{
    p++;
    while (p < e && *p != '"') {
        if (*p == '\\' && p + 1 < e) {
            p++;
            switch (*p) {
            case 'n': put(out, cap, len, '\n'); break;
            case 't': put(out, cap, len, '\t'); break;
            case 'r': put(out, cap, len, '\r'); break;
            case 'x':
                if (p + 2 < e) {
                    char hex[3] = { p[1], p[2], '\0' };
                    put(out, cap, len, (char)strtol(hex, NULL, 16));
                    p += 2;
                }
                break;
            default: put(out, cap, len, *p); break;
            }
        } else {
            put(out, cap, len, *p);
        }
        p++;
    }
    return p < e ? p + 1 : e;
}

/* Position after the item that starts at p: a literal, a class, or a group. */
static const char *skip_item(const char *p, const char *e)
{
    int depth = 0;
    char quote = 0;
    for (; p < e; p++) {
        if (quote != 0) {
            if (*p == '\\')
                p++;
            else if (*p == quote) {
                quote = 0;
                if (depth == 0)
                    return p + 1;
            }
            continue;
        }
        if (*p == '"' || *p == '[') {
            quote = (*p == '"') ? '"' : ']';
            continue;
        }
        if (*p == '(') {
            depth++;
            continue;
        }
        if (*p == ')') {
            if (--depth == 0)
                return p + 1;
            continue;
        }
        if (depth == 0)
            return p + 1;
    }
    return e;
}

static void expand(const char *g, const char *p, const char *e, char *out, size_t cap,
                   size_t *len, int depth)
{
    while (p < e) {
        char c = *p;
        if (c == '#') {
            while (p < e && *p != '\n')
                p++;
        } else if (isspace((unsigned char)c)) {
            p++;
        } else if (c == '"') {
            p = literal(p, e, out, cap, len);
        } else if (c == '[') {
            p = skip_item(p, e);
        } else if (c == '(') {
            const char *after = skip_item(p, e);
            if (depth < STUB_DEPTH)
                expand(g, p + 1, after - 1, out, cap, len, depth + 1);
            p = after;
        } else if (c == '|') {
            return;
        } else if (c == '{') {
            while (p < e && *p != '}')
                p++;
            if (p < e)
                p++;
        } else if (c == '*' || c == '+' || c == '?') {
            p++;
        } else if (is_ident((unsigned char)c)) {
            const char *id = p, *b, *x;
            while (p < e && is_ident((unsigned char)*p))
                p++;
            if (depth < STUB_DEPTH && find_rule(g, id, (size_t)(p - id), &b, &x) == 0)
                expand(g, b, x, out, cap, len, depth + 1);
        } else {
            p++;
        }
    }
}

static char *read_text(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long n;
    char *text;
    if (fp == NULL)
        return NULL;
    if (fseek(fp, 0, SEEK_END) != 0 || (n = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    text = malloc((size_t)n + 1);
    if (text == NULL || fread(text, 1, (size_t)n, fp) != (size_t)n) {
        free(text);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    text[n] = '\0';
    return text;
}

void edgellm_config_init(edgellm_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->n_ctx = 512;
    cfg->n_threads = 4;
    cfg->n_predict = 8;
    cfg->temperature = 0.0f;
    cfg->seed = 1;
}

edgellm_t *edgellm_open(const edgellm_config *cfg, char *err, size_t err_len)
{
    edgellm_t *e;
    char *g;
    const char *b, *x;
    size_t len = 0;

    if (cfg == NULL || cfg->grammar_path == NULL) {
        if (err != NULL && err_len > 0)
            snprintf(err, err_len, "stub: config needs grammar_path");
        return NULL;
    }
    g = read_text(cfg->grammar_path);
    if (g == NULL) {
        if (err != NULL && err_len > 0)
            snprintf(err, err_len, "stub: cannot read %s", cfg->grammar_path);
        return NULL;
    }
    if (find_rule(g, "root", 4, &b, &x) != 0) {
        free(g);
        if (err != NULL && err_len > 0)
            snprintf(err, err_len, "stub: %s has no root rule", cfg->grammar_path);
        return NULL;
    }
    e = calloc(1, sizeof(*e));
    if (e == NULL) {
        free(g);
        if (err != NULL && err_len > 0)
            snprintf(err, err_len, "stub: out of memory");
        return NULL;
    }
    e->answer[0] = '\0';
    expand(g, b, x, e->answer, sizeof(e->answer), &len, 0);
    free(g);
    return e;
}

int edgellm_decide(edgellm_t *e, const char *user_text, edgellm_result *out)
{
    if (e == NULL || user_text == NULL || out == NULL)
        return -1;
    memset(out, 0, sizeof(*out));
    snprintf(out->text, sizeof(out->text), "%s", e->answer);
    out->stub = 1;
    return 0;
}

const char *edgellm_engine_version(void)
{
    return "stub";
}

long edgellm_rss_kb(void)
{
    char line[128];
    long kb = -1;
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp == NULL)
        return -1;
    while (fgets(line, sizeof(line), fp) != NULL)
        if (strncmp(line, "VmRSS:", 6) == 0 && sscanf(line + 6, "%ld", &kb) == 1)
            break;
    fclose(fp);
    return kb;
}

void edgellm_close(edgellm_t *e)
{
    free(e);
}
