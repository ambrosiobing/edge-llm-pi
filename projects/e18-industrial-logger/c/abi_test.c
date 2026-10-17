/* abi_test.c: the ABI's three checks and the one number this project contributes to
 * the volume's variant comparison.
 *
 *     e18_abi_test --grammar <gbnf> --prompt <txt> [--model <gguf>] [--calls N]
 *
 * The three checks, each of which can fail:
 *
 *   1. The library's edgellm_abi_version() equals the EDGELLM_ABI_VERSION this test
 *      compiled against. A library replaced under a caller without the version
 *      being bumped is the failure this check exists for, and it is the pitfall the
 *      chapter names.
 *   2. A second edgellm_abi_open before a close is refused with -1, and a decide
 *      after a close is refused with -1. One engine per process, enforced rather
 *      than documented.
 *   3. An out_len one byte short of the answer returns -3 and leaves out empty, so
 *      a caller can never log half a JSON object.
 *
 * Then the measurement: N calls through edgellm_abi_decide, and N calls through
 * edgellm_decide in this process with no boundary between, one engine at a time
 * because two models do not fit on the board. The difference of the two means is
 * the cost of the boundary per call. It is printed in microseconds, with the mean
 * and the sample standard deviation of each, and it is a figure of this host: with
 * the stub the engine itself costs almost nothing, so the stub run measures the
 * boundary alone, and the run with the real model measures it against a decision.
 * Both go into the chapter's table, each with its own row and its own instrument.
 */
#define _GNU_SOURCE
#include "abi.h"

#include "edgellm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OUT_MAX 1024
#define USER_TEXT \
    "register 0 level 577 counts, span 141 counts over 30 frames, " \
    "temperature 42.4 C, pressure 5.8 bar. Decide."

static double us_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void stats(const double *v, int n, double *mean, double *sd)
{
    double s = 0.0, q = 0.0;
    int i;
    for (i = 0; i < n; i++)
        s += v[i];
    *mean = n > 0 ? s / (double)n : 0.0;
    for (i = 0; i < n; i++)
        q += (v[i] - *mean) * (v[i] - *mean);
    *sd = n > 1 ? sqrt(q / (double)(n - 1)) : 0.0;
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

/* A substring search for the literal passes on an object the generation budget cut before
 * its closing brace: { "decision": "NOMINAL holds NOMINAL. e02 lost 949 rows of 1000 to
 * exactly that on Saturday 17 October 2026, and all three counters in the repository
 * reported zero on the log. So require the quoted literal and the brace this grammar's root
 * accepts at, which is also what tools/agreement.py now does. */
static int one_complete_decision(const char *s)
{
    size_t n;

    if (strstr(s, "\"NOMINAL\"") == NULL && strstr(s, "\"DRIFT\"") == NULL)
        return 0;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'))
        n--;
    return n > 0 && s[n - 1] == '}';
}

int main(int argc, char **argv)
{
    const char *grammar = NULL, *prompt_path = NULL, *model = NULL;
    int calls = 200, i, failures = 0, rc;
    char out[OUT_MAX];
    double *abi_us = NULL, *direct_us = NULL;
    double abi_mean, abi_sd, dir_mean, dir_sd;
    char *prompt_text = NULL;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (v == NULL)
            break;
        if (strcmp(a, "--grammar") == 0) grammar = v;
        else if (strcmp(a, "--prompt") == 0) prompt_path = v;
        else if (strcmp(a, "--model") == 0) model = v;
        else if (strcmp(a, "--calls") == 0) calls = atoi(v);
        i++;
    }
    if (grammar == NULL || prompt_path == NULL || calls < 1) {
        fprintf(stderr, "usage: %s --grammar <gbnf> --prompt <txt> [--model <gguf>]"
                        " [--calls N]\n", argv[0]);
        return 2;
    }

    /* check 1: the library's version against this caller's */
    if (edgellm_abi_version() != (unsigned)EDGELLM_ABI_VERSION) {
        printf("FAIL version: header %u, library %u\n", (unsigned)EDGELLM_ABI_VERSION,
               edgellm_abi_version());
        failures++;
    } else {
        printf("ok version %u\n", edgellm_abi_version());
    }

    /* check 2a: a decide with nothing open */
    if (edgellm_abi_decide(USER_TEXT, out, sizeof(out)) != -1) {
        printf("FAIL decide before open did not return -1\n");
        failures++;
    } else {
        printf("ok decide before open refused\n");
    }

    if (edgellm_abi_open(model, grammar, prompt_path, NULL, 512, 4, 64, 0.0f) != 0) {
        printf("FAIL the engine did not open\n");
        return 1;
    }
    /* check 2b: a second open */
    if (edgellm_abi_open(model, grammar, prompt_path, NULL, 512, 4, 64, 0.0f) != -1) {
        printf("FAIL a second open was not refused\n");
        failures++;
    } else {
        printf("ok a second open refused\n");
    }

    rc = edgellm_abi_decide(USER_TEXT, out, sizeof(out));
    if (rc <= 0) {
        printf("FAIL the first decision returned %d\n", rc);
        failures++;
    } else if (!one_complete_decision(out)) {
        printf("FAIL the answer is not one complete decision: %s\n", out);
        failures++;
    } else {
        printf("ok one decision, %d bytes: %s\n", rc, out);
    }

    /* check 3: out_len one byte short */
    if (rc > 0) {
        char small[OUT_MAX];
        int r2 = edgellm_abi_decide(USER_TEXT, small, (size_t)rc);
        if (r2 != -3 || small[0] != '\0') {
            printf("FAIL a short buffer returned %d and left '%s'\n", r2, small);
            failures++;
        } else {
            printf("ok a short buffer refused, nothing written\n");
        }
    }

    /* the measurement, part one: through the boundary */
    abi_us = calloc((size_t)calls, sizeof(double));
    direct_us = calloc((size_t)calls, sizeof(double));
    if (abi_us == NULL || direct_us == NULL) {
        free(abi_us);
        free(direct_us);
        edgellm_abi_close();
        return 1;
    }
    for (i = 0; i < calls; i++) {
        double t0 = us_now();
        edgellm_abi_decide(USER_TEXT, out, sizeof(out));
        abi_us[i] = us_now() - t0;
    }
    edgellm_abi_close();

    /* part two: the same engine opened here, called with no boundary between */
    prompt_text = read_text(prompt_path);
    if (prompt_text == NULL) {
        printf("FAIL cannot read %s\n", prompt_path);
        free(abi_us);
        free(direct_us);
        return 1;
    }
    {
        edgellm_config cfg;
        edgellm_result res;
        edgellm_t *e;
        char err[256];
        edgellm_config_init(&cfg);
        cfg.model_path = model != NULL ? model : "";
        cfg.grammar_path = grammar;
        cfg.system_prompt = prompt_text;
        cfg.state_path = NULL;
        cfg.n_ctx = 512;
        cfg.n_threads = 4;
        cfg.n_predict = 64;
        cfg.temperature = 0.0f;
        e = edgellm_open(&cfg, err, sizeof(err));
        if (e == NULL) {
            printf("FAIL the in-process engine did not open: %s\n", err);
            failures++;
        } else {
            for (i = 0; i < calls; i++) {
                double t0 = us_now();
                edgellm_decide(e, USER_TEXT, &res);
                direct_us[i] = us_now() - t0;
            }
            edgellm_close(e);
        }
    }
    free(prompt_text);

    stats(abi_us, calls, &abi_mean, &abi_sd);
    stats(direct_us, calls, &dir_mean, &dir_sd);
    printf("calls %d, engine %s\n", calls, edgellm_engine_version());
    printf("through the ABI   mean %10.3f us  sd %10.3f us\n", abi_mean, abi_sd);
    printf("in process        mean %10.3f us  sd %10.3f us\n", dir_mean, dir_sd);
    printf("boundary per call      %10.3f us\n", abi_mean - dir_mean);
    free(abi_us);
    free(direct_us);

    printf("abi_test: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
