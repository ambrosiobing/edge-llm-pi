# Models and the engine pin

Every number here was read on Wednesday 14 October 2026 from the source named beside
it. A cell reading "to verify" was not fetched and must be before it is quoted.

## The engine

- llama.cpp release tag: `v0.6.0`, read from
  `https://api.github.com/repos/ggml-org/llama.cpp/releases/latest`.
- Release date: Sunday 5 October 2026 (`published_at`).
- Commit the tag points at: `d81235049384534c167caea52b85a694f6103d14`, read with
  `git rev-parse v0.6.0^{commit}` in a clone, which is the only authority for this.
  The annotated tag object itself is `69ecae922d2b60c1fa8c590279f4e2099af75111`.
- **Not** `target_commitish` from the releases API, which for this release is
  `8345f333951c661d166b00e6f9362e553768f292`, a different commit entirely. That field
  names the branch or commit a release was cut from, not the commit the tag carries.
  It was recorded here as the pin on Wednesday 14 October 2026 and corrected on Friday
  16 October 2026, after the first workflow run failed its tag check on every job that
  reads the submodule.
- Newest build tag the same day: `b11617`, Wednesday 14 October 2026, from
  `https://api.github.com/repos/ggml-org/llama.cpp/releases`.
- Where the pin lives: `cmake/llama_tag.cmake` (compiled into
  `edgellm_engine_version`), `.gitmodules` with the submodule's checked-out commit,
  `meta-edgellm/recipes-runtime/llama-cpp/llama-cpp_git.bb` (`SRCREV`).

The release tag is pinned, not the build tag: `v0.6.0` is what GitHub reports as the
latest release and it is an annotated tag; the `bNNNNN` tags are one per merged
build. A chapter quotes a number only from a binary built at this tag.

### C API symbols, verified in `include/llama.h` on master, Wednesday 14 October 2026

Fetched from `https://raw.githubusercontent.com/ggml-org/llama.cpp/master/include/llama.h`.
Every name below is declared under exactly this name; none needed a master-side
rename. `common/c/src/edgellm.c` uses them and nothing else from the engine.

`llama_backend_init`, `llama_model_load_from_file`, `llama_model_get_vocab`,
`llama_init_from_model`, `llama_tokenize`, `llama_token_to_piece`,
`llama_batch_get_one`, `llama_decode`, `llama_get_memory`, `llama_memory_clear`,
`llama_memory_seq_rm`, `llama_state_save_file`, `llama_state_load_file`,
`llama_sampler_chain_init`, `llama_sampler_chain_add`, `llama_sampler_init_grammar`,
`llama_sampler_init_greedy`, `llama_sampler_init_temp`, `llama_sampler_init_dist`,
`llama_sampler_sample`, `llama_sampler_accept`, `llama_sampler_free`,
`llama_model_default_params`, `llama_context_default_params`,
`llama_sampler_chain_default_params`, `llama_vocab_is_eog`, `llama_get_logits_ith`.

Also used and verified the same day: `llama_sampler_reset` (the grammar restarts at
`root` per decision), `llama_model_free`, `llama_free`. Noted from the header's own
comment: `llama_sampler_sample` accepts the token it returns, so `edgellm.c` does not
call `llama_sampler_accept` a second time; `llama_batch_get_one` tracks positions
itself, which is what makes `llama_memory_seq_rm` back to the system prompt length
sufficient between decisions. The check was made against master; the tagged header
is what the build compiles against, and the first `cmake --build` on skyhorizon's WSL
is the proof for the tag.

## The ladder

Sizes and SHA256 read from `https://huggingface.co/api/models/<repo>/tree/main`, the
licence from the model card metadata at `https://huggingface.co/api/models/<repo>`,
all on Wednesday 14 October 2026. "Mirrored" means a recipe under
`meta-edgellm/recipes-model/` fetches it; "linked" means the file is downloaded by hand
and never redistributed from this repository. The card URL is
`https://huggingface.co/<repo>`.

| Rung | Model | Bytes | Licence | Recipe |
|---|---|---|---|---|
| NanoPi floor | SmolLM2-135M-Instruct | 105454432 | Apache 2.0 | mirrored |
| Pi 3 default | SmolLM2-360M-Instruct | 270590880 | Apache 2.0 | mirrored |
| Pi 3 upper, Pi 4 enums | Qwen2.5-0.5B-Instruct | 491400032 | Apache 2.0 | mirrored |
| Pi 4, 2 GB or more | TinyLlama-1.1B-Chat-v1.0 | 668788096 | Apache 2.0 | mirrored |
| Pi 4, 2 GB or more | Llama-3.2-1B-Instruct | 807694464 | Llama 3.2 Community | linked |
| Pi 4, 2 GB or more | gemma-3-1b-it | 806058272 | Gemma Terms | linked |
| Pi 4, 4 GB or more, text | Qwen2.5-1.5B-Instruct | 1117320736 | Apache 2.0 | mirrored |
| 8 GB only, the ceiling | Phi-3-mini-4k-instruct | 2393231072 | MIT | mirrored |

Repositories, in the same order: `bartowski/SmolLM2-135M-Instruct-GGUF`,
`bartowski/SmolLM2-360M-Instruct-GGUF`, `Qwen/Qwen2.5-0.5B-Instruct-GGUF`,
`TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF`, `bartowski/Llama-3.2-1B-Instruct-GGUF`,
`unsloth/gemma-3-1b-it-GGUF`, `Qwen/Qwen2.5-1.5B-Instruct-GGUF`,
`microsoft/Phi-3-mini-4k-instruct-gguf`.

File names and SHA256, in the same order (the recipe names in parentheses):

- `SmolLM2-135M-Instruct-Q4_K_M.gguf` (`model-smollm2-135m`)
  `2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`
- `SmolLM2-360M-Instruct-Q4_K_M.gguf` (`model-smollm2-360m`)
  `2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2`
- `qwen2.5-0.5b-instruct-q4_k_m.gguf` (`model-qwen2.5-0.5b`)
  `74a4da8c9fdbcd15bd1f6d01d621410d31c6fc00986f5eb687824e7b93d7a9db`
- `tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf` (`model-tinyllama-1.1b`)
  `9fecc3b3cd76bba89d504f29b616eedf7da85b96540e490ca5824d3f7d2776a0`
- `Llama-3.2-1B-Instruct-Q4_K_M.gguf` (no recipe; card licence tag `llama3.2`)
  `6f85a640a97cf2bf5b8e764087b1e83da0fdb51d7c9fab7d0fece9385611df83`
- `gemma-3-1b-it-Q4_K_M.gguf` (no recipe; card licence tag `gemma`)
  `8270790f3ab69fdfe860b7b64008d9a19986d8df7e407bb018184caa08798ebd`
- `qwen2.5-1.5b-instruct-q4_k_m.gguf` (`model-qwen2.5-1.5b`)
  `6a1a2eb6d15622bf3c96857206351ba97e1af16c30d7a74ee38970e434e9407e`
- `Phi-3-mini-4k-instruct-q4.gguf` (`model-phi3-mini`)
  `8a83c7fb9049a9b2e92266fa7ad04933bb53aa1e85136b7b30f1b8000ff2edef`

Rung rule: the file must fit under 60 percent of `MemAvailable` from `free -m` on the
headless board after boot, with `n_ctx` 512, or a tokens-per-second figure is SD-card
paging. The bench Pi 4 Model B Rev 1.4, card `eplepi`, was read on Friday 16 October 2026:
MemTotal 8,007,396 kB, MemAvailable 7,789,840 kB after boot, so the ceiling on that board is
about 4.67 GB and every rung below is admissible, Phi-3-mini included. The Pi 3 boards and
the NanoPi are unchanged and still govern their own chapters.

Measured on that board on Friday 16 October 2026, Qwen2.5-0.5B-Instruct Q4_K_M at `n_ctx`
512: resident set 542,648 kB, about 530 MiB, against a 463 MiB file. The file is 6.16 bits
per weight rather than the 4.8 a Q4_K_M usually costs, because Qwen's own GGUF keeps 133
tensors at q5_0 and 13 at q8_0; read the type histogram rather than assuming the label.

## Host-side packages

Versions read from `https://pypi.org/pypi/<name>/json` on Wednesday 14 October 2026
and pinned in `requirements.txt`: matplotlib 3.11.2 (needs Python 3.11 or newer),
ppk2-api 0.9.2 (GPLv2, released Friday 9 June 2023), pyserial 3.5, pytest 9.1.1,
llama-cpp-python 0.3.36 (MIT). The llama.cpp commit that `llama-cpp-python` 0.3.36
bundles, and the `llama-cpp-2` crate version the Rust projects use with the commit it
bundles, are recorded by the e15 and e05 authors beside their manifests: to verify.

## Downloading by hand

A model file never enters the repository (`*.gguf` is ignored). On a board or on the
build host, with the repository and file name from the lists above:

    curl -L -o models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
      https://huggingface.co/bartowski/SmolLM2-360M-Instruct-GGUF/resolve/main/<file>
    sha256sum models/SmolLM2-360M-Instruct-Q4_K_M.gguf

and compare with the list before the first run.
