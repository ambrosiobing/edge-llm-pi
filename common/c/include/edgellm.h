/* edgellm.h: the decision core every C project links, and the C ABI the Rust service
 * in e18 and the daemon in e08 expose. One process, one model, one grammar, one
 * saved system-prompt state; a decision is a short grammar-constrained completion.
 *
 * Built against llama.cpp at the tag recorded in models/MODELS.md. With
 * EDGELLM_STUB_MODEL defined the same API is served by a stub that returns the
 * first literal of the grammar's root rule, so the whole loop runs in CI without a
 * model file; the stub's answer is always labelled "stub" in the log.
 */
#ifndef EDGELLM_H
#define EDGELLM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The tag is edgellm_s and not edgellm on purpose. A C struct tag is a class name in C++,
 * so "struct edgellm" and the wrapper's "namespace edgellm" in common/cpp/edgellm.hpp are
 * two different kinds of entity under one name, which C++ refuses: every C++ project
 * failed to compile with "namespace edgellm redeclared as different kind of entity" and a
 * cascade of errors under it. Renamed Friday 16 October 2026. Callers are unaffected:
 * edgellm_t is the name they all use, and it has not changed. */
typedef struct edgellm_s edgellm_t;

typedef struct edgellm_config {
    const char *model_path;      /* GGUF file; ignored by the stub */
    const char *grammar_path;    /* GBNF file, root symbol "root" */
    const char *system_prompt;   /* text, under 120 tokens; decoded once, state saved */
    const char *state_path;      /* where the system-prompt KV state is saved;
                                    NULL keeps it resident */
    int         n_ctx;           /* 512 by default */
    int         n_threads;       /* 4 on a Pi 4 or Pi 3; measured on the NanoPi */
    int         n_predict;       /* at most 8 for an enum, 64 for one sentence */
    float       temperature;     /* 0 means greedy; 0.3 for a sentence */
    uint32_t    seed;
} edgellm_config;

typedef struct edgellm_result {
    char   text[512];            /* the grammar-constrained completion, NUL terminated */
    int    n_prompt_tokens;
    int    n_generated;
    double prompt_ms;            /* wall clock around the prompt decode, CLOCK_MONOTONIC */
    double generate_ms;          /* wall clock around the generation loop */
    int    stub;                 /* 1 when the stub answered */
} edgellm_result;

/* Default config: n_ctx 512, n_threads 4, n_predict 8, temperature 0, seed 1. */
void edgellm_config_init(edgellm_config *cfg);

/* Loads the model, decodes the system prompt once and saves its state. On failure
 * returns NULL and writes a one-line reason into err (if err is not NULL). */
edgellm_t *edgellm_open(const edgellm_config *cfg, char *err, size_t err_len);

/* One decision: restore the system-prompt state, decode the user text, sample under
 * the grammar until n_predict or end of text. Returns 0 on success, -1 on error. */
int edgellm_decide(edgellm_t *e, const char *user_text, edgellm_result *out);

/* The engine's own build identity (llama.cpp build number and commit), or "stub". */
const char *edgellm_engine_version(void);

/* Resident set size of this process in kB from /proc/self/status, or -1. */
long edgellm_rss_kb(void);

void edgellm_close(edgellm_t *e);

#ifdef __cplusplus
}
#endif
#endif /* EDGELLM_H */
