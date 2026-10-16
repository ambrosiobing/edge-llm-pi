# common/rust/edgellm, the shared Rust crate

The decision loop of the volume in Rust, in the shape of `common/c`, written for e05 on
Wednesday 14 October 2026 and reused by e13, e14 and e18's service. One process, one model,
one grammar, one system prompt decoded once, one rule, one log; the rule's index reaches
`Project::actuate` and the log, the model's text reaches the log and nothing else.

## Modules, against the C headers

| C | Rust | What it holds |
|---|---|---|
| `edgellm.h` | `lib.rs`: `Decider`, `EngineConfig`, `Decision`, `open_decider` | the engine face |
| `edgellm.c`, `edgellm_stub.c` | `engine.rs`: `llama::Engine` (feature `engine`), `stub::Stub` (feature `stub`), `first_expansion` | the two engines |
| `io.h`, `io_replay.c`, `io_iio.c` | `io.rs`: `IoSource`, `Replay`, `Iio`, `open_live` | the hardware boundary |
| `rules.h` | `rules.rs`: `Rule`, `RuleCtx` (16 deep), `Registry`, `expect` | the control rule |
| `elog.h` | `elog.rs`: `Elog`, `format_row`, `format_g6`, `split_csv_line` | the nine-column log |
| `edgellm_main.c` | `lib.rs`: `parse_args`, `Project`, `run_loop`; `template.rs`: ChatML, Zephyr, none | the loop and the chat template |

The loop runs in the order of `edgellm_main.c`: open the engine with the wrapped system
prompt, open the source, open the log; per event run the rule on the history (the current
event is not yet in it), check the index against the name table, `actuate`, `format_prompt`,
wrap the user turn, decide, write the row, push the event. The log columns are the C
logger's, `t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine`, with the raw
values in C's `%.6g`, so `tools/agreement.py` and `tests/oracle_check` read a Rust log and
a C log alike.

## Features

- `stub` (default): the first expansion of the grammar on every call, the port of
  `edgellm_stub.c`, so `cargo test` in a project needs no model file and no C++ toolchain.
  Its unit tests hold the house grammar and the e05 grammar.
- `engine`: llama.cpp through the `llama-cpp-2` binding, linked into the one executable.

A binary built with one feature refuses the other's command line before anything is opened:
`--stub` on an engine build and a `--model` run on a stub build both exit 2.

## The binding and its engine, pinned

| What | Value | Read on |
|---|---|---|
| crate | `llama-cpp-2 = "=0.1.159"`, MIT OR Apache-2.0, published Wednesday 7 October 2026 | crates.io API, Wednesday 14 October 2026 |
| sys crate | `llama-cpp-sys-2 0.1.159`, bindgen 0.72.1 and the cmake crate at build time | the crate's own Cargo.toml on docs.rs, Wednesday 14 October 2026 |
| bundled llama.cpp | the binding's repository (`utilityai/llama-cpp-rs`, main branch) pins the submodule at commit `26394b4e`, Monday 21 September 2026; main was already at 0.1.160, so the commit inside the 0.1.159 crate is to verify from `~/.cargo/registry/src/*/llama-cpp-sys-2-0.1.159/llama.cpp` after the first build | GitHub contents API, Wednesday 14 October 2026 |
| the C projects' engine | tag `v0.6.0`, commit `d8123504`, Sunday 5 October 2026 (`cmake/llama_tag.cmake`) | `models/MODELS.md` |

The two are not the same build of the engine. A tokens per second figure from a Rust
project is therefore reported beside the C figure on the same board as two builds of the
same engine, never as the same build; `e05` prints `llama-cpp-2 0.1.159` in its engine
column for that reason, and the first `cargo build --features engine` records the bundled
commit here.

Type and method names used by `engine.rs`, read from docs.rs for 0.1.159 on Wednesday
14 October 2026: `LlamaBackend::init`, `LlamaModelParams::default`,
`LlamaModel::load_from_file`, `LlamaModel::new_context`, `LlamaModel::vocab`,
`LlamaVocab::tokenize`, `LlamaVocab::is_eog`, `LlamaVocab::token_to_piece`,
`LlamaContextParams::with_n_ctx`, `with_n_batch`, `with_n_threads`, `with_n_threads_batch`,
`LlamaBatch::get_one`, `LlamaContext::decode`, `clear_kv_cache`, `kv_cache_seq_rm`,
`LlamaSampler::grammar`, `greedy`, `temp`, `dist`, `chain_simple`, `sample`, `reset`. Seen on
docs.rs but with their signatures to verify on the first build: `LlamaContext::state_save_file`
and `state_load_file` (the `--state` route; the resident route through `kv_cache_seq_rm` is
the default and needs neither). `csv` and `serde` are not dependencies: the log and the replay
are written and read by hand so that the byte shape stays the C logger's.

## Building

Host, any platform with cargo (the stub, what CI runs):

    cargo test --manifest-path projects/e05-adxl345-events/Cargo.toml

Bench binary for the Pi 3 (aarch64), cross built in skyhorizon's WSL. The binding compiles
llama.cpp with cmake and generates its bindings with bindgen, so the target needs a C and
C++ cross compiler, cmake and libclang on the host:

    rustup target add aarch64-unknown-linux-gnu
    sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake clang libclang-dev
    export CC_aarch64_unknown_linux_gnu=aarch64-linux-gnu-gcc
    export CXX_aarch64_unknown_linux_gnu=aarch64-linux-gnu-g++
    export CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=aarch64-linux-gnu-gcc
    export BINDGEN_EXTRA_CLANG_ARGS="--sysroot=/usr/aarch64-linux-gnu"
    cargo build --release --target aarch64-unknown-linux-gnu \
      --manifest-path projects/e05-adxl345-events/Cargo.toml \
      --no-default-features --features engine

The four exports and the sysroot flag are the set the first cross build is expected to need
(to verify on skyhorizon; the error messages of bindgen and cmake name what is missing). The
alternative is a native build on the Pi 3 with `-j1`, which takes an hour or more and swap.
"One executable" means the engine is linked statically into the Rust binary against the
target's glibc; it is not a musl static binary, and `ldd` on the result lists libc, libm,
libstdc++ and libgcc_s, which the chapter prints beside the size.

## Rules for a project on this crate

- The project's `src/rules.rs` holds one `Rule` with its name table in the grammar's order
  and its cases in a `#[cfg(test)]` module using `edgellm::rules::expect`.
- `src/main.rs` holds the `Project` (the prompt and, where there is an output, `actuate`),
  registers the rule and calls `run_loop`; nothing else.
- `tests/oracle.rs` runs the stub binary over the replay and compares the rule and model
  columns with `tests/oracle/eNN.expected.csv`, written by the project's
  `tools/make_replay.py`.
- The bench binary is the same `main.rs` with `--features engine` and `--live`.
