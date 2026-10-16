/* abi.c: the four functions of abi.h over the shared decision core of
 * common/c/include/edgellm.h. This file is the whole boundary: it owns one
 * edgellm_t, one mutex and one buffer, and it adds no logic of its own, so that a
 * disagreement between the Rust service and a C project is never a disagreement
 * about the engine.
 *
 * The timings edgellm_decide reports are not returned across the boundary. They are
 * a structure field, and no structure crosses this ABI; the caller times its own
 * call, which is also the only honest thing to publish, since the figure the chapter
 * compares is the cost a caller actually pays. abi_test.c measures both sides of
 * that: the in-process call to edgellm_decide and the call through this file.
 */
#define _GNU_SOURCE
#include "abi.h"

#include "edgellm.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static edgellm_t      *g_engine;
static char           *g_prompt;      /* the system prompt text, owned here */

unsigned edgellm_abi_version(void)
{
    return (unsigned)EDGELLM_ABI_VERSION;
}

/* Reads a whole file and strips the trailing newlines, like the shared main does,
 * so a prompt file edited by hand does not change the prompt's token count. */
static char *read_text(const char *path)
{
    FILE *fp;
    long n;
    char *text;

    if (path == NULL)
        return NULL;
    fp = fopen(path, "rb");
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

int edgellm_abi_open(const char *model_path, const char *grammar_path,
                     const char *prompt_path, const char *state_path,
                     int n_ctx, int n_threads, int n_predict, float temperature)
{
    edgellm_config cfg;
    char err[256];
    int rc = 0;

    pthread_mutex_lock(&g_lock);
    if (g_engine != NULL) {
        pthread_mutex_unlock(&g_lock);
        return -1;                      /* one engine per process, see abi.h */
    }
    g_prompt = read_text(prompt_path);
    if (g_prompt == NULL) {
        pthread_mutex_unlock(&g_lock);
        return -2;
    }
    edgellm_config_init(&cfg);
    cfg.model_path = model_path != NULL ? model_path : "";
    cfg.grammar_path = grammar_path;
    cfg.system_prompt = g_prompt;
    cfg.state_path = state_path;
    if (n_ctx > 0)
        cfg.n_ctx = n_ctx;
    if (n_threads > 0)
        cfg.n_threads = n_threads;
    if (n_predict > 0)
        cfg.n_predict = n_predict;
    cfg.temperature = temperature;
    g_engine = edgellm_open(&cfg, err, sizeof(err));
    if (g_engine == NULL) {
        fprintf(stderr, "edgellm_abi_open: %s\n", err);
        free(g_prompt);
        g_prompt = NULL;
        rc = -2;
    }
    pthread_mutex_unlock(&g_lock);
    return rc;
}

int edgellm_abi_decide(const char *user, char *out, size_t out_len)
{
    edgellm_result res;
    size_t n;
    int rc;

    if (out == NULL || out_len == 0)
        return -3;
    out[0] = '\0';
    if (user == NULL)
        return -2;
    pthread_mutex_lock(&g_lock);
    if (g_engine == NULL) {
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    rc = edgellm_decide(g_engine, user, &res);
    pthread_mutex_unlock(&g_lock);
    if (rc != 0)
        return -2;
    n = strlen(res.text);
    if (n + 1 > out_len)
        return -3;                      /* never a truncated object, see abi.h */
    memcpy(out, res.text, n + 1);
    return (int)n;
}

void edgellm_abi_close(void)
{
    pthread_mutex_lock(&g_lock);
    edgellm_close(g_engine);
    g_engine = NULL;
    free(g_prompt);
    g_prompt = NULL;
    pthread_mutex_unlock(&g_lock);
}
