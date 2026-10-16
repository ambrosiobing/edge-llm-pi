/* edgellm.c: the decision core over the llama.cpp C API at the tag in
 * cmake/llama_tag.cmake. Symbols verified against include/llama.h on
 * Wednesday 14 October 2026; see models/MODELS.md for the list.
 *
 * One model, one context of n_ctx tokens, one sampler chain. The system prompt is
 * decoded once at open. Per decision the context is brought back to the end of the
 * system prompt either by reloading the saved state file (state_path set) or by
 * removing every position past it with llama_memory_seq_rm (state_path NULL), the
 * sampler is reset so the grammar starts at "root" again, the user text is decoded,
 * and tokens are sampled until n_predict or an end-of-generation token.
 *
 * llama_sampler_sample already accepts the token it returns, so llama_sampler_accept
 * is not called here a second time; calling it twice would advance the grammar twice.
 */
#define _GNU_SOURCE
#include "edgellm.h"
#include "llama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef EDGELLM_LLAMA_TAG
#define EDGELLM_LLAMA_TAG "untagged"
#endif

struct edgellm {
    struct llama_model       *model;
    struct llama_context     *ctx;
    const struct llama_vocab *vocab;
    struct llama_sampler     *sampler;
    char        *grammar;
    char        *state_path;
    llama_token *sys_tokens;
    int          n_sys;
    int          n_ctx;
    int          n_predict;
    float        temperature;
    uint32_t     seed;
};

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static void seterr(char *err, size_t err_len, const char *msg)
{
    if (err != NULL && err_len > 0)
        snprintf(err, err_len, "edgellm: %s", msg);
}

static int decide_fail(const char *msg)
{
    fprintf(stderr, "edgellm: %s\n", msg);
    return -1;
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

/* add_bos: 1 for the system prompt (the model's BOS goes first), 0 for user text. */
static int tokenize(const struct llama_vocab *vocab, const char *text, int add_bos,
                    llama_token **out, int *n_out)
{
    int len = (int)strlen(text);
    int n = -llama_tokenize(vocab, text, len, NULL, 0, add_bos != 0, true);
    llama_token *tokens;
    if (n <= 0)
        return -1;
    tokens = malloc(sizeof(*tokens) * (size_t)n);
    if (tokens == NULL)
        return -1;
    if (llama_tokenize(vocab, text, len, tokens, n, add_bos != 0, true) != n) {
        free(tokens);
        return -1;
    }
    *out = tokens;
    *n_out = n;
    return 0;
}

static int build_sampler(edgellm_t *e)
{
    struct llama_sampler *grammar;
    e->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (e->sampler == NULL)
        return -1;
    grammar = llama_sampler_init_grammar(e->vocab, e->grammar, "root");
    if (grammar == NULL)
        return -1;
    llama_sampler_chain_add(e->sampler, grammar);
    if (e->temperature <= 0.0f) {
        llama_sampler_chain_add(e->sampler, llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(e->sampler, llama_sampler_init_temp(e->temperature));
        llama_sampler_chain_add(e->sampler, llama_sampler_init_dist(e->seed));
    }
    return 0;
}

edgellm_t *edgellm_open(const edgellm_config *cfg, char *err, size_t err_len)
{
    static int backend_ready;
    struct llama_model_params mp;
    struct llama_context_params cp;
    edgellm_t *e;

    if (cfg == NULL || cfg->model_path == NULL || cfg->grammar_path == NULL ||
        cfg->system_prompt == NULL) {
        seterr(err, err_len, "config needs model_path, grammar_path and system_prompt");
        return NULL;
    }
    e = calloc(1, sizeof(*e));
    if (e == NULL) {
        seterr(err, err_len, "out of memory");
        return NULL;
    }
    e->n_ctx = cfg->n_ctx > 0 ? cfg->n_ctx : 512;
    e->n_predict = cfg->n_predict > 0 ? cfg->n_predict : 8;
    e->temperature = cfg->temperature;
    e->seed = cfg->seed;
    e->grammar = read_text(cfg->grammar_path);
    if (e->grammar == NULL) {
        seterr(err, err_len, "cannot read the grammar file");
        goto fail;
    }
    if (cfg->state_path != NULL && (e->state_path = strdup(cfg->state_path)) == NULL) {
        seterr(err, err_len, "out of memory");
        goto fail;
    }
    if (!backend_ready) {
        llama_backend_init();
        backend_ready = 1;
    }
    mp = llama_model_default_params();
    e->model = llama_model_load_from_file(cfg->model_path, mp);
    if (e->model == NULL) {
        seterr(err, err_len, "cannot load the model file");
        goto fail;
    }
    e->vocab = llama_model_get_vocab(e->model);

    cp = llama_context_default_params();
    cp.n_ctx = (uint32_t)e->n_ctx;
    cp.n_batch = (uint32_t)e->n_ctx;            /* a prompt is decoded in one batch */
    cp.n_threads = cfg->n_threads > 0 ? cfg->n_threads : 4;
    cp.n_threads_batch = cp.n_threads;
    e->ctx = llama_init_from_model(e->model, cp);
    if (e->ctx == NULL) {
        seterr(err, err_len, "cannot create the context");
        goto fail;
    }
    if (tokenize(e->vocab, cfg->system_prompt, 1, &e->sys_tokens, &e->n_sys) != 0) {
        seterr(err, err_len, "cannot tokenize the system prompt");
        goto fail;
    }
    if (e->n_sys + e->n_predict + 16 > e->n_ctx) {
        seterr(err, err_len, "system prompt leaves no room in n_ctx");
        goto fail;
    }
    if (llama_decode(e->ctx, llama_batch_get_one(e->sys_tokens, e->n_sys)) != 0) {
        seterr(err, err_len, "system prompt decode failed");
        goto fail;
    }
    if (e->state_path != NULL &&
        !llama_state_save_file(e->ctx, e->state_path, e->sys_tokens, (size_t)e->n_sys)) {
        seterr(err, err_len, "cannot save the system prompt state");
        goto fail;
    }
    if (build_sampler(e) != 0) {
        seterr(err, err_len, "grammar rejected; is the root rule named root?");
        goto fail;
    }
    return e;
fail:
    edgellm_close(e);
    return NULL;
}

static int restore(edgellm_t *e)
{
    llama_memory_t mem = llama_get_memory(e->ctx);
    if (e->state_path != NULL) {
        llama_token *scratch = malloc(sizeof(*scratch) * (size_t)e->n_sys);
        size_t n_loaded = 0;
        bool ok;
        if (scratch == NULL)
            return decide_fail("out of memory");
        llama_memory_clear(mem, true);
        ok = llama_state_load_file(e->ctx, e->state_path, scratch, (size_t)e->n_sys, &n_loaded);
        free(scratch);
        if (!ok || (int)n_loaded != e->n_sys)
            return decide_fail("state restore failed");
        return 0;
    }
    if (!llama_memory_seq_rm(mem, 0, e->n_sys, -1))
        return decide_fail("cannot trim the context back to the system prompt");
    return 0;
}

int edgellm_decide(edgellm_t *e, const char *user_text, edgellm_result *out)
{
    llama_token *user = NULL;
    int n_user = 0, i;
    size_t len = 0;
    double t0;

    if (e == NULL || user_text == NULL || out == NULL)
        return -1;
    memset(out, 0, sizeof(*out));
    if (restore(e) != 0)
        return -1;
    llama_sampler_reset(e->sampler);
    if (tokenize(e->vocab, user_text, 0, &user, &n_user) != 0)
        return decide_fail("cannot tokenize the user text");
    if (e->n_sys + n_user + e->n_predict > e->n_ctx) {
        free(user);
        return decide_fail("prompt plus n_predict does not fit n_ctx");
    }
    t0 = now_ms();
    if (llama_decode(e->ctx, llama_batch_get_one(user, n_user)) != 0) {
        free(user);
        return decide_fail("prompt decode failed");
    }
    out->prompt_ms = now_ms() - t0;
    out->n_prompt_tokens = e->n_sys + n_user;
    free(user);

    t0 = now_ms();
    for (i = 0; i < e->n_predict; i++) {
        char piece[64];
        int n;
        llama_token tok = llama_sampler_sample(e->sampler, e->ctx, -1);
        if (llama_vocab_is_eog(e->vocab, tok))
            break;
        n = llama_token_to_piece(e->vocab, tok, piece, (int32_t)sizeof(piece), 0, true);
        if (n < 0)
            return decide_fail("token piece does not fit the buffer");
        if (len + (size_t)n >= sizeof(out->text))
            break;
        memcpy(out->text + len, piece, (size_t)n);
        len += (size_t)n;
        out->text[len] = '\0';
        out->n_generated++;
        if (i + 1 < e->n_predict && llama_decode(e->ctx, llama_batch_get_one(&tok, 1)) != 0)
            return decide_fail("decode failed during generation");
    }
    out->generate_ms = now_ms() - t0;
    return 0;
}

const char *edgellm_engine_version(void)
{
    return "llama.cpp " EDGELLM_LLAMA_TAG;
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
    if (e == NULL)
        return;
    if (e->sampler != NULL)
        llama_sampler_free(e->sampler);
    if (e->ctx != NULL)
        llama_free(e->ctx);
    if (e->model != NULL)
        llama_model_free(e->model);
    free(e->sys_tokens);
    free(e->state_path);
    free(e->grammar);
    free(e);
}
