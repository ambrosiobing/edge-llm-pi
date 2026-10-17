/* edgellm_main.c: the one main() every C project reuses. See edgellm_main.h for the
 * two hooks a project supplies and the order of the loop.
 *
 *   edgellm-eNN --project eNN (--replay <csv> | --live <backend> <spec>)
 *               --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]
 *               [--n-predict N] [--threads N] [--n-ctx N] [--temperature T]
 *               [--interval S] [--state <path>] [--max-events N] [--stub]
 *
 * --stub is accepted only by a binary configured with -DEDGELLM_STUB_MODEL=ON, where
 * --model is optional. A binary carrying the real engine refuses --stub with exit 2,
 * so a log can never claim an engine it did not run.
 */
#define _GNU_SOURCE
#include "edgellm_main.h"
#include "edgellm.h"
#include "elog.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PROMPT_BUF 2048

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

__attribute__((weak)) int project_actuate(io_source *src, const rule_desc *rule, int rule_index,
                                          const io_event *ev)
{
    (void)src;
    (void)rule;
    (void)rule_index;
    (void)ev;
    return 0;
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

static void sleep_s(double seconds)
{
    struct timespec ts;
    if (seconds <= 0.0)
        return;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --project eNN (--replay <csv> | --live <backend> <spec>)\n"
            "          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]\n"
            "          [--n-predict N] [--threads N] [--n-ctx N] [--temperature T]\n"
            "          [--interval S] [--state <path>] [--max-events N] [--stub]\n"
            "          [--template chatml|zephyr|none]\n",
            argv0);
    return 2;
}

/* The chat template, added Wednesday 14 October 2026. SmolLM2 and Qwen2.5, the ladder's
 * defaults, are ChatML models: a system prompt and a user turn carry the im_start and
 * im_end markers, and the answer begins after the assistant marker. prompt.txt stays plain
 * text and project_format_prompt() returns plain user text; the wrapping happens here, once,
 * so no project can forget it. TinyLlama uses the Zephyr form. "none" passes text through,
 * which is what a benchmark wants. The markers are parsed as special tokens by the engine. */
static const char *template_name = "chatml";

static char *wrap_system(const char *prompt)
{
    const char *head, *tail;
    char *out;
    if (strcmp(template_name, "zephyr") == 0) {
        head = "<|system|>\n";
        tail = "</s>\n";
    } else if (strcmp(template_name, "none") == 0) {
        head = "";
        tail = "";
    } else {
        head = "<|im_start|>system\n";
        tail = "<|im_end|>\n";
    }
    out = malloc(strlen(head) + strlen(prompt) + strlen(tail) + 1);
    if (out != NULL)
        sprintf(out, "%s%s%s", head, prompt, tail);
    return out;
}

static const char *wrap_user(const char *user, char *buf, size_t len)
{
    const char *head, *tail;
    if (strcmp(template_name, "zephyr") == 0) {
        head = "<|user|>\n";
        tail = "</s>\n<|assistant|>\n";
    } else if (strcmp(template_name, "none") == 0) {
        return user;
    } else {
        head = "<|im_start|>user\n";
        tail = "<|im_end|>\n<|im_start|>assistant\n";
    }
    if (strlen(head) + strlen(user) + strlen(tail) + 1 > len)
        return NULL;
    /* The line above has already proved the three parts fit, so this writes exactly what
     * sprintf would have. It is snprintf because len is a parameter: gcc cannot see the
     * guard through it and warned -Wformat-overflow on every build of every C project
     * here, and a build that prints warnings teaches a reader to skip them. */
    snprintf(buf, len, "%s%s%s", head, user, tail);
    return buf;
}

int edgellm_main(int argc, char **argv)
{
    const char *project = NULL, *replay = NULL, *backend = NULL, *spec = NULL;
    const char *grammar = NULL, *prompt_path = NULL, *log_path = NULL, *model = NULL;
    const char *state = NULL;
    int want_stub = 0, max_events = 0, i, r = 0, rc = 0;
    double interval = 0.0;
    edgellm_config cfg;
    edgellm_t *engine = NULL;
    io_source *src = NULL;
    elog *log = NULL;
    const rule_desc *rule;
    rule_ctx ctx;
    io_event ev;
    char err[256], *system_prompt = NULL, *prompt_buf = NULL, *wrapped_buf = NULL;
    long n_events = 0, n_model = 0, n_model_errors = 0, n_act_errors = 0;
    double sum_prompt_ms = 0.0, sum_generate_ms = 0.0;
    struct sigaction sa;

    edgellm_config_init(&cfg);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(a, "--stub") == 0) { want_stub = 1; continue; }
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
        else if (strcmp(a, "--grammar") == 0) grammar = v;
        else if (strcmp(a, "--prompt") == 0) prompt_path = v;
        else if (strcmp(a, "--log") == 0) log_path = v;
        else if (strcmp(a, "--model") == 0) model = v;
        else if (strcmp(a, "--state") == 0) state = v;
        else if (strcmp(a, "--n-predict") == 0) cfg.n_predict = atoi(v);
        else if (strcmp(a, "--threads") == 0) cfg.n_threads = atoi(v);
        else if (strcmp(a, "--n-ctx") == 0) cfg.n_ctx = atoi(v);
        else if (strcmp(a, "--temperature") == 0) cfg.temperature = (float)atof(v);
        else if (strcmp(a, "--interval") == 0) interval = atof(v);
        else if (strcmp(a, "--max-events") == 0) max_events = atoi(v);
        else if (strcmp(a, "--template") == 0) template_name = v;
        else return usage(argv[0]);
        i++;
    }
    if (strcmp(template_name, "chatml") != 0 && strcmp(template_name, "zephyr") != 0 &&
        strcmp(template_name, "none") != 0)
        return usage(argv[0]);
    if (project == NULL || grammar == NULL || prompt_path == NULL || log_path == NULL ||
        (replay == NULL && backend == NULL) || (replay != NULL && backend != NULL))
        return usage(argv[0]);
#ifdef EDGELLM_STUB_MODEL
    (void)want_stub;
#else
    if (want_stub) {
        fprintf(stderr, "%s: built with the real engine; configure with "
                        "-DEDGELLM_STUB_MODEL=ON for --stub\n", argv[0]);
        return 2;
    }
    if (model == NULL) {
        fprintf(stderr, "%s: --model is required with the real engine\n", argv[0]);
        return 2;
    }
#endif

    rule = rule_find(project);
    if (rule == NULL) {
        fprintf(stderr, "%s: no rule registered for project %s\n", argv[0], project);
        return 1;
    }
    {
        char *plain = read_text(prompt_path);
        if (plain == NULL) {
            fprintf(stderr, "%s: cannot read %s\n", argv[0], prompt_path);
            return 1;
        }
        system_prompt = wrap_system(plain);
        free(plain);
        if (system_prompt == NULL)
            return 1;
    }
    cfg.model_path = model != NULL ? model : "";
    cfg.grammar_path = grammar;
    cfg.system_prompt = system_prompt;
    cfg.state_path = state;

    engine = edgellm_open(&cfg, err, sizeof(err));
    if (engine == NULL) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        rc = 1;
        goto out;
    }
    src = replay != NULL ? io_open_replay(replay, err, sizeof(err))
                         : io_open_live(backend, spec, err, sizeof(err));
    if (src == NULL) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        rc = 1;
        goto out;
    }
    log = elog_open(log_path, edgellm_engine_version());
    if (log == NULL) {
        fprintf(stderr, "%s: cannot write %s\n", argv[0], log_path);
        rc = 1;
        goto out;
    }
    prompt_buf = malloc(PROMPT_BUF);
    wrapped_buf = malloc(PROMPT_BUF + 128);
    if (prompt_buf == NULL || wrapped_buf == NULL) {
        rc = 1;
        goto out;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    memset(&ctx, 0, sizeof(ctx));

    while (!stop_requested && (r = io_next(src, &ev)) == 1) {
        int idx = rule->fn(&ctx, &ev);
        int n_names = rule_name_count(rule);
        const char *user;
        edgellm_result res;
        const edgellm_result *logged = NULL;

        if (idx < 0 || idx >= n_names) {
            fprintf(stderr, "%s: rule %s returned %d outside its %d names\n", argv[0],
                    project, idx, n_names);
            rc = 1;
            break;
        }
        if (project_actuate(src, rule, idx, &ev) != 0)
            n_act_errors++;
        user = project_format_prompt(&ev, &ctx, prompt_buf, PROMPT_BUF);
        if (user != NULL)
            user = wrap_user(user, wrapped_buf, PROMPT_BUF + 128);
        if (user != NULL) {
            if (edgellm_decide(engine, user, &res) == 0) {
                n_model++;
                sum_prompt_ms += res.prompt_ms;
                sum_generate_ms += res.generate_ms;
            } else {
                n_model_errors++;
                memset(&res, 0, sizeof(res));
            }
            logged = &res;
        }
        if (elog_row(log, &ev, rule->names[idx], logged) != 0) {
            fprintf(stderr, "%s: log write failed\n", argv[0]);
            rc = 1;
            break;
        }
        rule_ctx_push(&ctx, &ev);
        n_events++;
        if (max_events > 0 && n_events >= max_events)
            break;
        if (interval > 0.0)
            sleep_s(interval);
    }
    if (rc == 0 && !stop_requested && r < 0 && (max_events == 0 || n_events < max_events))
        rc = 1;

    fprintf(stderr, "%s: %ld events, %ld model answers, %ld model errors, %ld actuation "
                    "errors, mean prompt %.1f ms, mean generate %.1f ms, rss %ld kB, engine "
                    "%s, source %s\n",
            project, n_events, n_model, n_model_errors, n_act_errors,
            n_model > 0 ? sum_prompt_ms / (double)n_model : 0.0,
            n_model > 0 ? sum_generate_ms / (double)n_model : 0.0, edgellm_rss_kb(),
            edgellm_engine_version(), io_backend_name(src));
out:
    free(wrapped_buf);
    free(prompt_buf);
    elog_close(log);
    io_close(src);
    edgellm_close(engine);
    free(system_prompt);
    return rc;
}

#ifndef EDGELLM_NO_MAIN
int main(int argc, char **argv)
{
    return edgellm_main(argc, argv);
}
#endif
