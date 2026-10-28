/* e04_client.c: project e04, the Pi thermal and throttle narrator, as an HTTP client
 * of llama-server (catalogue variant D). The engine lives in another process; this
 * one owns the loop: read the SoC temperature through io.h, read the throttle word
 * and the ARM clock through vcgencmd, decide with the rule, POST one grammar-bounded
 * completion to the server, pull the answer and the server's timings out of the JSON
 * with a small hand-written extractor, and log the row with the rule column beside the
 * model column. The engine header edgellm.h is not used here: its result struct is
 * reached only through elog.h, so this file links io, rules and elog and never the
 * engine.
 *
 *   edgellm-e04 --project e04 (--replay <csv> | --live sysfs <path>[:<interval_s>])
 *               --grammar <gbnf> --prompt <txt> --log <csv> --server http://host:port
 *               [--rtt <csv>] [--n-predict N] [--temperature T] [--max-events N]
 *               [--timeout-ms N] [--wait-s N] [--rule-only]
 *
 * The event: v0 is millidegrees from thermal_zone0, v1 the get_throttled word as a
 * number, v2 the ARM clock in hertz. On replay all three come from the CSV; live, v0
 * comes from the sysfs backend at its interval and v1 and v2 from two popen calls
 * made right after the reading. --rule-only logs the rule alone with an empty model
 * column at the same interval: that is the narrator-idle temperature trace, taken by
 * the same instrument. The request goes to POST /completion with the grammar in the
 * "grammar" field and the ChatML form built here, because the server applies no chat
 * template on that endpoint; the answer's "content" and "timings" are read back.
 */
#define _GNU_SOURCE
#include "elog.h"
#include "io.h"
#include "rules.h"

#include <curl/curl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PROMPT_BUF 2048
#define REQUEST_MAX 16384

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

/* ---------------------------------------------------------------- small helpers */

typedef struct buf {
    char  *data;
    size_t len, cap;
} buf;

static int buf_add(buf *b, const char *text, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        char *d;
        while (cap < b->len + n + 1)
            cap *= 2;
        d = realloc(b->data, cap);
        if (d == NULL)
            return -1;
        b->data = d;
        b->cap = cap;
    }
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int buf_add_str(buf *b, const char *text)
{
    return buf_add(b, text, strlen(text));
}

/* Appends text as a JSON string body (no surrounding quotes): the five escapes the
 * prompt and the grammar can contain, and \u00XX for any other control byte. */
static int buf_add_json(buf *b, const char *text)
{
    char esc[8];
    for (; *text != '\0'; text++) {
        unsigned char c = (unsigned char)*text;
        const char *piece = NULL;
        switch (c) {
        case '"':  piece = "\\\""; break;
        case '\\': piece = "\\\\"; break;
        case '\n': piece = "\\n"; break;
        case '\r': piece = "\\r"; break;
        case '\t': piece = "\\t"; break;
        default:
            if (c < 0x20) {
                snprintf(esc, sizeof esc, "\\u%04x", c);
                piece = esc;
            }
            break;
        }
        if (piece == NULL ? buf_add(b, (const char *)&c, 1) : buf_add_str(b, piece))
            return -1;
    }
    return 0;
}

static size_t on_body(char *ptr, size_t size, size_t nmemb, void *user)
{
    return buf_add((buf *)user, ptr, size * nmemb) == 0 ? size * nmemb : 0;
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static void sleep_s(double seconds)
{
    struct timespec ts;
    if (seconds <= 0.0)
        return;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
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
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r'))
        text[--n] = '\0';
    return text;
}

/* ---------------------------------------------------------------- the JSON extractor */

/* Finds "key" followed by ':' and returns a pointer to the first byte of its value, or
 * NULL. The server answers each key once at the top level, and the grammar admits no
 * quote inside the summary, so the first hit is the one wanted; the grammar and the
 * prompt echoed back inside the answer carry their quotes escaped and cannot match. */
static const char *json_value(const char *body, const char *key)
{
    char pat[64];
    const char *p = body;
    size_t n = (size_t)snprintf(pat, sizeof pat, "\"%s\"", key);
    while ((p = strstr(p, pat)) != NULL) {
        const char *q = p + n;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
            q++;
        if (*q == ':') {
            q++;
            while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
                q++;
            return q;
        }
        p += n;
    }
    return NULL;
}

/* Copies the string value of key into out, resolving the JSON escapes. A \uXXXX below
 * 128 becomes that byte, anything above becomes '?': the log is ASCII. Returns 0 when
 * the string closed. */
static int json_string(const char *body, const char *key, char *out, size_t len)
{
    const char *p = json_value(body, key);
    size_t k = 0;
    if (p == NULL || *p != '"')
        return -1;
    for (p++; *p != '\0' && *p != '"'; p++) {
        char c = *p;
        if (c == '\\') {
            p++;
            switch (*p) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u': {
                char hex[5] = { 0, 0, 0, 0, 0 };
                unsigned long v;
                if (strlen(p + 1) < 4)
                    return -1;
                memcpy(hex, p + 1, 4);
                v = strtoul(hex, NULL, 16);
                c = v < 128 ? (char)v : '?';
                p += 4;
                break;
            }
            case '\0': return -1;
            default: c = *p; break;          /* \" \\ and \/ */
            }
        }
        if (k + 1 < len)
            out[k++] = c;
    }
    out[k] = '\0';
    return *p == '"' ? 0 : -1;
}

static int json_number(const char *body, const char *key, double *out)
{
    const char *p = json_value(body, key);
    char *end;
    if (p == NULL)
        return -1;
    *out = strtod(p, &end);
    return end == p ? -1 : 0;
}

/* Whether the answer is a terminated JSON object, which is a different question
 * from whether a named field can be read out of it and the reason this exists.
 * This project emits "decision" first, so json_string finds it on a row whose
 * object the token budget cut off mid-summary; the run of Monday 26 October 2026
 * printed "no literal 0" while 136 of its 180 objects never closed, and that was
 * read as satisfying the clause "zero rows with no parseable object". It does not.
 *
 * Braces inside string literals do not count and backslash escapes are honoured,
 * so a summary containing { or " cannot close the object by accident. Returns 1
 * when the outermost object closes, 0 otherwise, including an empty answer. */
static int json_object_closed(const char *s)
{
    int depth = 0, in_string = 0, escaped = 0, seen_open = 0;
    if (s == NULL)
        return 0;
    for (; *s != '\0'; s++) {
        if (in_string) {
            if (escaped)
                escaped = 0;
            else if (*s == '\\')
                escaped = 1;
            else if (*s == '"')
                in_string = 0;
            continue;
        }
        if (*s == '"')
            in_string = 1;
        else if (*s == '{') {
            depth++;
            seen_open = 1;
        } else if (*s == '}') {
            if (--depth == 0 && seen_open)
                return 1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- vcgencmd, live only */

/* Runs one vcgencmd line and parses the number after '='. get_throttled prints
 * throttled=0x50000 and measure_clock arm prints frequency(48)=1500398464; strtol
 * with base 0 reads both. Returns -1 when the command or the parse failed. */
static int vcgencmd_number(const char *cmd, double *out)
{
    char line[128];
    const char *eq;
    char *end;
    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
        return -1;
    if (fgets(line, sizeof line, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    eq = strchr(line, '=');
    if (eq == NULL)
        return -1;
    *out = (double)strtol(eq + 1, &end, 0);
    return end == eq + 1 ? -1 : 0;
}

/* ---------------------------------------------------------------- the HTTP side */

typedef struct server {
    CURL              *curl;
    struct curl_slist *headers;
    char               url_completion[256];
    char               url_health[256];
    char               url_props[256];
    long               timeout_ms;
} server;

static int http_get(server *s, const char *url, buf *out, long *code)
{
    CURLcode rc;
    out->len = 0;
    if (out->data != NULL)
        out->data[0] = '\0';
    curl_easy_setopt(s->curl, CURLOPT_URL, url);
    curl_easy_setopt(s->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(s->curl, CURLOPT_TIMEOUT_MS, s->timeout_ms);
    rc = curl_easy_perform(s->curl);
    *code = 0;
    if (rc != CURLE_OK)
        return -1;
    curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, code);
    return 0;
}

static int http_post(server *s, const char *body, buf *out, long *code)
{
    CURLcode rc;
    out->len = 0;
    if (out->data != NULL)
        out->data[0] = '\0';
    curl_easy_setopt(s->curl, CURLOPT_URL, s->url_completion);
    curl_easy_setopt(s->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(s->curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, s->headers);
    curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(s->curl, CURLOPT_TIMEOUT_MS, s->timeout_ms);
    rc = curl_easy_perform(s->curl);
    *code = 0;
    if (rc != CURLE_OK) {
        fprintf(stderr, "e04: POST %s: %s\n", s->url_completion, curl_easy_strerror(rc));
        return -1;
    }
    curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, code);
    return 0;
}

/* GET /health until it answers 200 or wait_s has passed. llama-server answers 503
 * while the model is still loading, which is the state the unit ordering leaves the
 * client in for the first seconds after boot. */
static int wait_for_server(server *s, buf *out, int wait_s)
{
    double deadline = now_ms() + 1e3 * wait_s;
    for (;;) {
        long code;
        if (http_get(s, s->url_health, out, &code) == 0 && code == 200)
            return 0;
        if (now_ms() >= deadline || stop_requested)
            return -1;
        sleep_s(2.0);
    }
}

/* Builds one /completion request: the ChatML form of the system prompt and the user
 * text, the grammar, and the sampling settings of the chapter. */
static int build_request(buf *req, const char *system_prompt, const char *user,
                         const char *grammar, int n_predict, double temperature)
{
    char num[64];
    req->len = 0;
    if (buf_add_str(req, "{\"prompt\":\"<|im_start|>system\\n") ||
        buf_add_json(req, system_prompt) ||
        buf_add_str(req, "<|im_end|>\\n<|im_start|>user\\n") ||
        buf_add_json(req, user) ||
        buf_add_str(req, "<|im_end|>\\n<|im_start|>assistant\\n\",\"grammar\":\"") ||
        buf_add_json(req, grammar))
        return -1;
    snprintf(num, sizeof num, "\",\"n_predict\":%d,\"temperature\":%.3f,", n_predict,
             temperature);
    if (buf_add_str(req, num) ||
        buf_add_str(req, "\"cache_prompt\":true,\"seed\":1,\"stream\":false}"))
        return -1;
    return 0;
}

/* One narration: POST, time the round trip with the client's own clock, and read the
 * answer and the server's timings back. Fills res for the log and returns the round
 * trip in milliseconds, or a negative number on failure. */
static double narrate(server *s, const char *body, buf *out, edgellm_result *res)
{
    double t0 = now_ms(), rtt, v;
    long code;
    memset(res, 0, sizeof *res);
    if (http_post(s, body, out, &code) != 0)
        return -1.0;
    rtt = now_ms() - t0;
    if (code != 200) {
        fprintf(stderr, "e04: the server answered HTTP %ld: %.200s\n", code,
                out->data != NULL ? out->data : "");
        return -1.0;
    }
    if (json_string(out->data, "content", res->text, sizeof res->text) != 0) {
        fprintf(stderr, "e04: no \"content\" in the answer\n");
        return -1.0;
    }
    res->prompt_ms = json_number(out->data, "prompt_ms", &v) == 0 ? v : 0.0;
    res->generate_ms = json_number(out->data, "predicted_ms", &v) == 0 ? v : 0.0;
    res->n_generated = json_number(out->data, "predicted_n", &v) == 0 ? (int)v : 0;
    res->n_prompt_tokens = json_number(out->data, "prompt_n", &v) == 0 ? (int)v : 0;
    return rtt;
}

/* The user text: the three numbers the model is to narrate and up to three previous
 * temperatures, nothing else. The thresholds are in the system prompt, so they cost
 * no tokens per event. The current event is not yet in the history. */
static const char *format_prompt(const io_event *ev, const rule_ctx *ctx, char *buf,
                                 size_t len)
{
    char hist[96] = "";
    int  k = 0, i;
    for (i = (ctx->n > 3 ? ctx->n - 3 : 0); i < ctx->n && k < (int)sizeof hist - 12; i++)
        k += snprintf(hist + k, sizeof hist - (size_t)k, "%s%.1f", k ? ", " : "",
                      ctx->hist[i].v[0] / 1000.0);
    snprintf(buf, len,
             "SoC temperature %.1f C, throttle bits 0x%lx, ARM clock %.0f MHz, previous "
             "temperatures %s. Narrate.",
             ev->n > 0 ? ev->v[0] / 1000.0 : 0.0, ev->n > 1 ? (long)ev->v[1] : 0L,
             ev->n > 2 ? ev->v[2] / 1e6 : 0.0, k ? hist : "none");
    return buf;
}

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --project e04 (--replay <csv> | --live sysfs <path>[:<interval_s>])\n"
            "          --grammar <gbnf> --prompt <txt> --log <csv> --server http://host:port\n"
            "          [--rtt <csv>] [--n-predict N] [--temperature T] [--max-events N]\n"
            "          [--timeout-ms N] [--wait-s N] [--rule-only]\n",
            argv0);
    return 2;
}

int main(int argc, char **argv)
{
    const char *project = NULL, *replay = NULL, *backend = NULL, *spec = NULL;
    const char *grammar_path = NULL, *prompt_path = NULL, *log_path = NULL;
    const char *server_url = NULL, *rtt_path = NULL;
    int n_predict = 64, max_events = 0, wait_s = 120, rule_only = 0, i, r = 0, rc = 0;
    double temperature = 0.3;
    long timeout_ms = 60000;
    char *system_prompt = NULL, *grammar = NULL, *prompt_buf = NULL;
    char err[256], engine[128], decision[64];
    buf req = { NULL, 0, 0 }, out = { NULL, 0, 0 };
    server srv;
    io_source *src = NULL;
    elog *log = NULL;
    FILE *rtt_fp = NULL;
    const rule_desc *rule;
    rule_ctx ctx;
    io_event ev;
    struct sigaction sa;
    long n_events = 0, n_model = 0, n_model_errors = 0, n_vc_errors = 0;
    long n_agree = 0, n_disagree = 0, n_disagree_at_change = 0, n_no_literal = 0;
    long n_unclosed = 0;
    int prev_idx = -1;
    double sum_rtt = 0.0, sum_prompt_ms = 0.0, sum_generate_ms = 0.0;

    memset(&srv, 0, sizeof srv);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(a, "--rule-only") == 0) { rule_only = 1; continue; }
        if (v == NULL)
            return usage(argv[0]);
        if (strcmp(a, "--project") == 0) project = v;
        else if (strcmp(a, "--replay") == 0) replay = v;
        else if (strcmp(a, "--live") == 0) {
            if (i + 2 >= argc)
                return usage(argv[0]);
            backend = v;
            spec = argv[i + 2];
            i++;
        }
        else if (strcmp(a, "--grammar") == 0) grammar_path = v;
        else if (strcmp(a, "--prompt") == 0) prompt_path = v;
        else if (strcmp(a, "--log") == 0) log_path = v;
        else if (strcmp(a, "--rtt") == 0) rtt_path = v;
        else if (strcmp(a, "--server") == 0) server_url = v;
        else if (strcmp(a, "--n-predict") == 0) n_predict = atoi(v);
        else if (strcmp(a, "--temperature") == 0) temperature = atof(v);
        else if (strcmp(a, "--max-events") == 0) max_events = atoi(v);
        else if (strcmp(a, "--timeout-ms") == 0) timeout_ms = atol(v);
        else if (strcmp(a, "--wait-s") == 0) wait_s = atoi(v);
        else return usage(argv[0]);
        i++;
    }
    if (project == NULL || grammar_path == NULL || prompt_path == NULL || log_path == NULL ||
        (replay == NULL && backend == NULL) || (replay != NULL && backend != NULL) ||
        (server_url == NULL && !rule_only))
        return usage(argv[0]);
    if (backend != NULL && strcmp(backend, "sysfs") != 0) {
        fprintf(stderr, "%s: the live source of e04 is the sysfs backend\n", argv[0]);
        return 2;
    }

    rule = rule_find(project);
    if (rule == NULL) {
        fprintf(stderr, "%s: no rule registered for project %s\n", argv[0], project);
        return 1;
    }
    system_prompt = read_text(prompt_path);
    grammar = read_text(grammar_path);
    prompt_buf = malloc(PROMPT_BUF);
    if (system_prompt == NULL || grammar == NULL || prompt_buf == NULL) {
        fprintf(stderr, "%s: cannot read %s or %s\n", argv[0], prompt_path, grammar_path);
        rc = 1;
        goto out;
    }

    snprintf(engine, sizeof engine, "rule only, no server");
    if (!rule_only) {
        char model_path[256] = "", build_info[64] = "";
        const char *base;
        long code;
        curl_global_init(CURL_GLOBAL_DEFAULT);
        srv.curl = curl_easy_init();
        srv.headers = curl_slist_append(NULL, "Content-Type: application/json");
        srv.timeout_ms = timeout_ms;
        snprintf(srv.url_completion, sizeof srv.url_completion, "%s/completion", server_url);
        snprintf(srv.url_health, sizeof srv.url_health, "%s/health", server_url);
        snprintf(srv.url_props, sizeof srv.url_props, "%s/props", server_url);
        if (srv.curl == NULL || wait_for_server(&srv, &out, wait_s) != 0) {
            fprintf(stderr, "%s: %s did not answer /health with 200 within %d s\n", argv[0],
                    server_url, wait_s);
            rc = 1;
            goto out;
        }
        /* The engine column names the server build and the model file it loaded, read
         * once from /props, so a log from the mock can never be read as a bench log. */
        if (http_get(&srv, srv.url_props, &out, &code) == 0 && code == 200) {
            json_string(out.data, "build_info", build_info, sizeof build_info);
            json_string(out.data, "model_path", model_path, sizeof model_path);
        }
        base = strrchr(model_path, '/');
        snprintf(engine, sizeof engine, "llama-server %s %s",
                 build_info[0] ? build_info : "build unread",
                 base != NULL ? base + 1 : (model_path[0] ? model_path : "model unread"));
    }

    src = replay != NULL ? io_open_replay(replay, err, sizeof err)
                         : io_open_live(backend, spec, err, sizeof err);
    if (src == NULL) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        rc = 1;
        goto out;
    }
    log = elog_open(log_path, engine);
    if (log == NULL) {
        fprintf(stderr, "%s: cannot write %s\n", argv[0], log_path);
        rc = 1;
        goto out;
    }
    if (rtt_path != NULL) {
        rtt_fp = fopen(rtt_path, "w");
        if (rtt_fp == NULL) {
            fprintf(stderr, "%s: cannot write %s\n", argv[0], rtt_path);
            rc = 1;
            goto out;
        }
        fputs("t_s,rtt_ms,server_prompt_ms,server_predicted_ms,http_ms\n", rtt_fp);
        fflush(rtt_fp);
    }
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    memset(&ctx, 0, sizeof ctx);

    while (!stop_requested && (r = io_next(src, &ev)) == 1) {
        int idx, n_names;
        const char *user;
        edgellm_result res;
        const edgellm_result *logged = NULL;

        /* Live: the sysfs backend gave v0; the throttle word and the clock follow. A
         * vcgencmd that fails leaves n at 1, and the rule then decides on the
         * temperature alone; the failure is counted and printed at the end. */
        if (backend != NULL && ev.n == 1) {
            double bits, clock;
            if (vcgencmd_number("vcgencmd get_throttled", &bits) == 0 &&
                vcgencmd_number("vcgencmd measure_clock arm", &clock) == 0) {
                ev.v[1] = bits;
                ev.v[2] = clock;
                ev.n = 3;
            } else {
                n_vc_errors++;
            }
        }
        idx = rule->fn(&ctx, &ev);
        n_names = rule_name_count(rule);
        if (idx < 0 || idx >= n_names) {
            fprintf(stderr, "%s: rule %s returned %d outside its %d names\n", argv[0],
                    project, idx, n_names);
            rc = 1;
            break;
        }
        if (!rule_only) {
            user = format_prompt(&ev, &ctx, prompt_buf, PROMPT_BUF);
            if (build_request(&req, system_prompt, user, grammar, n_predict, temperature) != 0 ||
                req.len > REQUEST_MAX) {
                fprintf(stderr, "%s: request too long\n", argv[0]);
                rc = 1;
                break;
            }
            {
                double rtt = narrate(&srv, req.data, &out, &res);
                if (rtt >= 0.0) {
                    n_model++;
                    sum_rtt += rtt;
                    sum_prompt_ms += res.prompt_ms;
                    sum_generate_ms += res.generate_ms;
                    if (rtt_fp != NULL) {
                        fprintf(rtt_fp, "%.6f,%.3f,%.3f,%.3f,%.3f\n", ev.t_s, rtt,
                                res.prompt_ms, res.generate_ms,
                                rtt - res.prompt_ms - res.generate_ms);
                        fflush(rtt_fp);
                    }
                    /* The agreement, counted here as well as by tools/agreement.py,
                     * so that a disagreement on a row whose rule decision differs
                     * from the previous row's is counted apart: that is the "within
                     * one interval" allowance of the acceptance test. */
                    /* Counted separately from the literal, because the clause
                     * "zero rows with no parseable object" is about the object
                     * and not about the first field in it. */
                    if (!json_object_closed(res.text))
                        n_unclosed++;
                    if (json_string(res.text, "decision", decision, sizeof decision) != 0)
                        n_no_literal++;
                    else if (strcmp(decision, rule->names[idx]) == 0)
                        n_agree++;
                    else {
                        n_disagree++;
                        if (prev_idx >= 0 && idx != prev_idx)
                            n_disagree_at_change++;
                    }
                } else {
                    n_model_errors++;
                    memset(&res, 0, sizeof res);
                }
                logged = &res;
            }
        }
        if (elog_row(log, &ev, rule->names[idx], logged) != 0) {
            fprintf(stderr, "%s: log write failed\n", argv[0]);
            rc = 1;
            break;
        }
        rule_ctx_push(&ctx, &ev);
        prev_idx = idx;
        n_events++;
        if (max_events > 0 && n_events >= max_events)
            break;
    }
    if (rc == 0 && !stop_requested && r < 0 && (max_events == 0 || n_events < max_events))
        rc = 1;

    fprintf(stderr,
            "%s: %ld events, %ld model answers, %ld model errors, %ld vcgencmd errors, "
            "mean round trip %.1f ms, mean server prompt %.1f ms, mean server generate "
            "%.1f ms, mean HTTP cost %.1f ms, agree %ld, disagree %ld (of which %ld on a "
            "row where the rule changed), no literal %ld, engine %s, source %s, "
            "objects unclosed %ld\n",
            project, n_events, n_model, n_model_errors, n_vc_errors,
            n_model > 0 ? sum_rtt / (double)n_model : 0.0,
            n_model > 0 ? sum_prompt_ms / (double)n_model : 0.0,
            n_model > 0 ? sum_generate_ms / (double)n_model : 0.0,
            n_model > 0 ? (sum_rtt - sum_prompt_ms - sum_generate_ms) / (double)n_model : 0.0,
            n_agree, n_disagree, n_disagree_at_change, n_no_literal, engine,
            io_backend_name(src), n_unclosed);
out:
    if (rtt_fp != NULL)
        fclose(rtt_fp);
    elog_close(log);
    io_close(src);
    if (srv.headers != NULL)
        curl_slist_free_all(srv.headers);
    if (srv.curl != NULL) {
        curl_easy_cleanup(srv.curl);
        curl_global_cleanup();
    }
    free(req.data);
    free(out.data);
    free(prompt_buf);
    free(grammar);
    free(system_prompt);
    return rc;
}
