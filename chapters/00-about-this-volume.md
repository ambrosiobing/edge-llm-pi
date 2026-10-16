# A local model on the bench Raspberry Pis

*twenty projects checked against a two-line rule, and the channel kick-off*

Joseph Ambrose Pagaran. Wednesday 14 October 2026.

> The PDF and the HTML of this volume stay with the author; this Markdown edition and the code in this repository are the published form. The video chapters of Part II are not published.

## How to read this document

### The question behind all twenty

One question runs through the whole volume and is instanced at the head of every project:

> [!NOTE]
> **The through-line**
>
> What does a small local model add over a two-line threshold, and when does it get it wrong?

Every project therefore carries three things that make the question answerable: a control rule, two or three lines in the project's own language, that decides on its own; a decision by the model that a grammar limits to an enum or to one bounded sentence; and an agreement table over a logged run of at least 100 events, with every disagreement classified by hand. A project where the model adds nothing over the rule is a valid result and is written as one. A project where the model is wrong in an interesting way is a better result, and the chapter says what kind of wrong.

### The three marks

Three words carry a judgement throughout, and the colour never carries it alone:

- **have:** something that is on the bench, built, or measured, with the record named.
- **learn:** something that would have to be learned or verified before the chapter's run, with an estimate of how much.
- **absent:** something that is not on the bench, with what the absence costs.

### What a project chapter contains

Each of the twenty chapters follows one skeleton, enforced by the linter: the question instanced to the project; a key-facts box; the pitch; the draft idea it came from and what changed; the parts and their interfaces; the model, its rung on the ladder and the grammar verbatim; what the model decides or writes; the control rule verbatim, in the language the project chose; the three honesty bullets (real, modelled, absent); five figures (system architecture, wiring, sequence, states and data, measured data); the latency design; the steps; the variant chosen with its five-criteria paragraph; the Embedded Linux and Yocto subsection with the recipe, the unit and the overlay fragment; the acceptance test, written so that it can fail; the agreement table; the pitfalls; the languages and variants not chosen, one line each with the delta; the video potential; and the sources, every URL marked verified with its date or marked to verify.

The printed control rule, grammar and main loop in each chapter are the repository's own files, and the linter compares the printed block against the file. A chapter whose printed code drifts from the repository fails the build.

### Part II stays local

The six video chapters are channel strategy, not portfolio evidence. They are in this PDF and in the HTML edition, both of which stay in the catalogue. The public repository carries the Markdown edition of Part I and the code, nothing of Part II.

## Accessibility of this document

### What was aimed at, and what was reached

The request was for a dark-background document meeting the Web Content Accessibility Guidelines 2.2, in a PDF and in an HTML edition. The dark page is straightforward. Full conformance in a PDF produced by this toolchain is not, and the position is stated before the content rather than after it.

> [!NOTE]
> **The engine finding, established Monday 5 October 2026**
>
> The tagged route was attempted first, for the sibling volume, and does not work on this machine. The installed MiKTeX carries the LaTeX kernel of Saturday 1 November 2025 in its administrator tree and the `latex-lab` tagging bundle from June of 2026 in the user tree. The bundle will not load on that kernel: it stops at `\DebugTemplatesOff` and then at `\template_debug_typeout:n`, neither of which the older kernel defines, and the MiKTeX repository offers no newer kernel. This PDF is therefore **not a tagged PDF**, and PDF/UA is not claimed. The finding was re-checked for this volume on Wednesday 14 October 2026 and stands.

### What this PDF does satisfy

Measured, not asserted. Every ratio below was computed with the WCAG 2.2 relative-luminance formula before the first compile, by `contrast_check.py` in this folder, which reads `palette.tex` and fails the build if any value falls short.

| **Success criterion** | **How it is met here** |
| --- | --- |
| 1.4.3 and 1.4.6 Contrast | Body text 17.65:1, secondary text 12.12:1, links 10.97:1, headings 15.41:1, all against the page. The lowest text value anywhere is 9.51:1. AAA needs 7:1 |
| 1.4.11 Non-text contrast | Diagram lines 12.71:1 against the page; every coloured block in a figure is at least 14.13:1 against the page, and the black text on those blocks is at least 15.27:1 |
| 1.4.1 Use of colour | No status is shown by colour alone. The three marks each print a word: **have**, **learn**, **absent**. In figures, a solid, dashed or dotted outline carries the meaning that a colour also carries, and the model and rule blocks carry their names |
| 1.1.1 Non-text content, in part | Every figure's caption states in words everything the figure shows, including which blocks are solid, dashed and dotted, so no information exists only in a picture. This is a compensation, not a substitute: the PDF has no `/Alt` entry a screen reader can announce |
| 1.3.2 Meaningful sequence | Figures do not float. Each one sits between the paragraph that introduces it and the paragraph that follows, so the content-stream order is the reading order |
| 2.4.2 Page titled | A document title is set and the viewer is asked to display it rather than the file name |
| 2.4.4 Link purpose | Link text names its destination: a model card by its repository name, a help page by its number, a header file by its path |
| 2.4.5 Multiple ways | A table of contents, a bookmark tree, and internal cross-references |
| 3.1.1 Language of page | The document language is written into the catalogue as `en-GB` |
| 1.3.3 Sensory characteristics | No instruction depends on shape, size or position alone |

### What this PDF does not satisfy, stated plainly

- **absent:** 1.1.1 Non-text content, programmatically. There is no structure tree, so figures carry no alternative text an assistive technology can read.
- **absent:** 1.3.1 Info and relationships. Headings, lists, tables and their header cells are visually distinct but not marked up. A screen reader meets a table as a stream of cells with no idea which are headers.
- **absent:** 4.1.2 Name, role, value. Links are annotations without link structure elements.

### The HTML edition is the conformant artefact, and this time it is built

The sibling volume of Monday 5 October 2026 said that an HTML edition could be made conformant and that it was not built yet. For this volume `build.py` writes the HTML edition from the same `.tex` sources as the PDF, so the two cannot drift, and the gate `check_html.py` refuses the build when any of the following fails: the page is dark by default and switches to the light set under the reader's own `prefers-color-scheme` preference, with both sets measured by the same contrast gate; a skip link to the main content is the first focusable element; there is exactly one `h1` and no heading level is skipped; every figure is an inline SVG inside a wrapper with `role="img"` and the caption as its accessible name, with a visible `figcaption`; every table header cell carries `scope`; the focus ring is the accent colour at 3 pixels with a 2 pixel offset, measured at 15.41:1 against the dark page; no element carries an inline style; and no em or en dash appears anywhere. A reader using assistive technology should be given the HTML edition. A sighted reader gets the same content from either file.

## The spine

Written once here and applied in every chapter. A chapter that departs from any of these says so and says why.

### The through-line and the two-line rule

Every project has a control rule that decides on its own, in two or three lines, from the same inputs the model sees. The rule is the reference the model is scored against, not a fallback. Where the rule and the model disagree, the chapter does not assume the model is right; it classifies the disagreement by hand (rule wrong, model wrong, both defensible, input ambiguous) and the classification is part of the agreement table.

### The safety rule

The model is never in the path to an LED, a buzzer, a modem send, or any action that protects anything. The rule acts. The model proposes among options the rule has already declared safe, or writes the explanation of what the rule did. In e19, the one project where a message leaves the bench, the rule decides whether to send and the model only composes the text, which is checked field by field before it goes.

### The honesty rule

Carried over from the joint node volume. In every figure a **solid** outline is hardware present on the bench; a **dashed** outline is a software stand-in, such as a replayed CSV, a `vcan` node, or a model standing in for a plant; a **dotted grey** block is absent and explained, such as a motor, a torque sensor, an RS485 peer, or a microphone. Two styles are added for this volume: `llm`, a yellow outline for the language model block, whose caption carries the latest measured agreement rate or the words "not measured"; and `rulebl`, a green outline for the control rule block. Every table that carries numbers has a column that names the instrument, or reads "not measured" with the reason. Nothing modelled is presented as measured.

### The measurement rule and the protocol

Every number names its instrument or says it has not been taken. The protocol for a tokens-per-second figure, the same on every board:

**Step 1.** `vcgencmd get_throttled` must read `0x0` before the run. The bit meanings are documented at [raspberrypi.com/documentation/computers/os.html](https://www.raspberrypi.com/documentation/computers/os.html) (verified Wednesday 14 October 2026): bit 0 under-voltage, bit 1 ARM frequency capped, bit 2 currently throttled, bit 3 soft temperature limit active, and bits 16 to 19 the same four as "has occurred" since boot. Any non-zero value means the run is not taken.

**Step 2.** The upstream tool is the reference: `llama-bench -m <gguf> -p 64 -n 32 -t 1,2,3,4 -r 5 -o md`, which is 64 prompt tokens, 32 generated tokens, one to four threads, five repetitions, Markdown output. The options are documented at [github.com/ggml-org/llama.cpp/blob/master/tools/llama-bench/README.md](https://github.com/ggml-org/llama.cpp/blob/master/tools/llama-bench/README.md) (verified Wednesday 14 October 2026), where `-r` defaults to 5 and `-o` accepts `md`.

**Step 3.** `vcgencmd get_throttled` must read `0x0` after the run too.

**Step 4.** The run is repeated on a second day. A two-day spread above 15 percent on any row is reported as such and the row is not quoted as one number.

**Step 5.** The volume's own harness, which times `llama_decode` with `clock_gettime`, is priced against `llama-bench` on the same board, model and thread count. If the two disagree by more than 10 percent, nothing from the harness is published and the harness is the first suspect.

One reference point exists to test, not to copy: the pi-llm-bench collection reports, for a Raspberry Pi 4 with 4 GB and TinyLlama 1.1B at Q4\_K\_M, 2.79 to 5.82 tokens per second at one to four threads. That page was not fetched for this edition and the figures are a hypothesis for e01 to confirm or refute on this bench, not a number this volume states.

### The latency design

Shared by all twenty, varied only where a chapter says so: decisions are event-driven or on a 10 to 60 second interval, never in a tight loop; the system prompt is under 120 tokens, decoded once, its state saved with `llama_state_save_file` and restored before each decision, or kept resident and trimmed with `llama_memory_seq_rm`; `n_ctx` is 512; `n_predict` is at most 8 for an enum and 64 for one sentence; decisions are sampled greedily under the grammar, text at temperature 0.3 under a sentence grammar; four threads on the Pi 4 and the Pi 3 family, one to four measured on the NanoPi. On a Pi 3 a decision takes seconds, and the chapters say so rather than hide it.

### The control-versus-model method

The same four parts in every project:

1. **An enum.** The decision space is a short list of names, such as `CLEAR`, `APPROACHING`, `TOO_CLOSE`. Where the model writes a sentence, the sentence is bounded by a grammar and its numbers are checked against the record they claim to describe.
2. **A two-or-three-line rule** over the same inputs, printed verbatim in the project's language from the repository file.
3. **A GBNF grammar** with root symbol `root`, one file per enum under `grammars/`, so the model cannot answer outside the list. The grammar is the guarantee; the prompt is only a request.
4. **An agreement table** over at least 100 logged events: the count, the agreement rate, the confusion table rule against model, and every disagreement classified by hand. A table that has not been run reads "not measured" with the reason.

## The runtime

### llama.cpp as the engine

llama.cpp is the engine in all twenty projects, loading GGUF files at one pinned release tag that e01 records and that the repository's `models/MODELS.md` repeats. How the engine is reached is each project's own choice from the catalogue in the next section, from the C API in one process to `llama-server` over HTTP, a Rust binding, or a server on another machine. The pin matters: the API on `master` has already dropped `use_mmap` from the model parameters and the `llama_kv_self_*` family, so code copied from an older example does not compile against the current header.

The C API symbols that the C, C++ and daemon projects use, verified Wednesday 14 October 2026 from the raw header [include/llama.h at ggml-org/llama.cpp master, served raw by githubusercontent.com](https://raw.githubusercontent.com/ggml-org/llama.cpp/master/include/llama.h) (verified Wednesday 14 October 2026), where each is present and none is marked deprecated: `llama_backend_init`, `llama_model_load_from_file`, `llama_model_get_vocab`, `llama_init_from_model`, `llama_tokenize`, `llama_token_to_piece`, `llama_batch_get_one`, `llama_decode`, `llama_get_memory`, `llama_memory_clear`, `llama_memory_seq_rm`, `llama_state_save_file`, `llama_state_load_file`, `llama_sampler_chain_init`, `llama_sampler_chain_add`, `llama_sampler_init_grammar` with the signature `(const struct llama_vocab *vocab, const char *grammar_str, const char *grammar_root)`, `llama_sampler_init_greedy`, `llama_sampler_init_temp`, `llama_sampler_init_dist`, `llama_sampler_sample`, `llama_sampler_accept` and `llama_sampler_free`. On the same read, `llama_new_context_with_model` is present but marked deprecated in favour of `llama_init_from_model`, and `use_mmap` and `llama_kv_self_seq_rm` are not in the header at all. The pinned tag is what makes this list stable; a chapter that bumps the tag re-reads the header.

### Ollama, declined once

Ollama would have given model pulls by name, Modelfiles, and a chat endpoint that most tutorials assume. It is declined everywhere in this volume for four reasons that each matter on this bench: it adds a second daemon, written in Go, on boards with 1 GB of memory; it hides the thread count, the KV state and the grammar behind its own defaults, which are the three things the measurement protocol varies; it has no state save and restore, which the latency design depends on; and its endpoint is a chat, while every project here needs an enum. `llama-cpp-python` is used only where a project chose Python on the five criteria, which is e15 and e20, and the chapter says so.

### The model ladder

Sizes are the Q4\_K\_M file sizes read from the Hugging Face model cards on Wednesday 14 October 2026; the rung is where the file fits on this bench.

| **Model** | **Q4\_K\_M** | **Licence** | **Rung** | **Card** |
| --- | --- | --- | --- | --- |
| SmolLM2-135M-Instruct | 105 MB | Apache 2.0 | NanoPi floor | [bartowski/SmolLM2-135M-Instruct-GGUF](https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF) (verified Wednesday 14 October 2026) |
| SmolLM2-360M-Instruct | 0.27 GB | Apache 2.0 | Pi 3 family default | [bartowski/SmolLM2-360M-Instruct-GGUF](https://huggingface.co/bartowski/SmolLM2-360M-Instruct-GGUF) (verified Wednesday 14 October 2026) |
| Qwen2.5-0.5B-Instruct | 491 MB | Apache 2.0 | Pi 3 upper rung; Pi 4 default for enums | [Qwen/Qwen2.5-0.5B-Instruct-GGUF](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF) (verified Wednesday 14 October 2026) |
| TinyLlama-1.1B-Chat-v1.0 | 669 MB | Apache 2.0 | Pi 4 with 2 GB or more | [TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF](https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF) (verified Wednesday 14 October 2026) |
| Llama-3.2-1B-Instruct | 0.81 GB | Llama 3.2 community | Pi 4 with 2 GB or more; linked, not mirrored | [bartowski/Llama-3.2-1B-Instruct-GGUF](https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF) (verified Wednesday 14 October 2026) |
| Gemma 3 1B it | 806 MB | Gemma terms | Pi 4 with 2 GB or more; linked, not mirrored | [unsloth/gemma-3-1b-it-GGUF](https://huggingface.co/unsloth/gemma-3-1b-it-GGUF) (verified Wednesday 14 October 2026) |
| Qwen2.5-1.5B-Instruct | 1.12 GB | Apache 2.0 | Pi 4 with 4 GB or more; text projects | [Qwen/Qwen2.5-1.5B-Instruct-GGUF](https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF) (verified Wednesday 14 October 2026) |
| Phi-3-mini-4k-instruct, q4 | 2.2 GB | MIT | 8 GB only; the ceiling, unused unless e01 reads 8 GB | [microsoft/Phi-3-mini-4k-instruct-gguf](https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf) (verified Wednesday 14 October 2026) |

**The rung rule.** The file must fit under 60 percent of `MemAvailable` as reported by `free -m` on the headless board after boot, with `n_ctx` 512, or the figure the harness reports is SD-card paging and not tokens per second. The Pi 4 Model B's rung is decided by e01, which reads its memory first. Models under Apache 2.0 or MIT may be mirrored in the repository with their SHA256; Llama 3.2 and Gemma are linked only, and their recipes are not written.

## Implementation variants

Joseph's catalogue of Wednesday 14 October 2026 lists the ways of putting an inference engine on a Yocto Raspberry Pi along three dimensions: the application language, the inference engine, and how the engine is integrated into the image. It is reproduced here with one status per variant. "Built in" names the project whose chapter builds and measures it. "Evaluated and declined" gives the reason, and the check that would reopen the decision. "Not evaluated" names the one requirement that would justify the work. There is no fixed baseline: each project scored the catalogue on five criteria for itself alone (practicality, relevance, popularity, compliance, best practice), and the choice and its reason are in that chapter's **Variant chosen** paragraph.

### C and C++ engine variants, A to P

| **Variant** | **Language** | **Engine or runtime** | **Integration** | **Status** |
| --- | --- | --- | --- | --- |
| A | C++ | llama.cpp | linked directly, one process | built in e01, e09 and e12 |
| B | C++ | llama.cpp | over the `llama.h` C API, one process | built in e07; priced beside C in e02 |
| C | C | llama.cpp | plain C over the C API, one process | built in e02, e03, e06, e10, e11, e16, e17 and e19 |
| D | C client | llama.cpp | `llama-server` over HTTP, two units | built in e04 |
| E | C or C++ | custom ggml engine | own graph on the ggml library | not evaluated; justified only by a model architecture llama.cpp does not load that this bench needs |
| F | C++ | ONNX Runtime | exported graph, own tokenizer and sampler | evaluated and declined Wednesday 14 October 2026: no grammar-constrained sampler in the runtime, so the enum guarantee would be rebuilt outside the engine, and a second engine and quantisation toolchain on a 1 GB board; reopened by a Q4 export of Qwen2.5-0.5B decoding under a grammar within 10 percent of llama.cpp on the Pi 3B |
| G | C++ | ONNX Runtime GenAI | the GenAI layer's own loop | evaluated and declined Wednesday 14 October 2026: the layer owns tokenizer and sampling, so the grammar bound would sit outside it; reopened by a constrained-decoding entry point in its C header |
| H | C++ | TensorFlow Lite | own graph and sampler | evaluated and declined Wednesday 14 October 2026 for text generation: no GGUF loader and no grammar sampler; kept as a contender for e12's band classifier, where the model is not a language model |
| I | C | TensorFlow Lite C API | as H, through the C API | as H; the third contender named in e12's Future and extra exercises |
| J | C++ | OpenVINO | vendor runtime | evaluated and declined Wednesday 14 October 2026: its targets are Intel CPUs, GPUs and NPUs, and its ARM path is a CPU plugin with no quantised-kernel advantage expected on a Cortex-A53 or A72; reopened by an aarch64 build whose Q4 kernels match llama.cpp's on the Pi 4B |
| K | C++ | MNN | converted model, vendor runtime | evaluated and declined Wednesday 14 October 2026: a second model format and quantisation toolchain, no GBNF; reopened by a converted Qwen2.5-0.5B decoding under a grammar |
| L | C++ | ExecuTorch | PyTorch export per model | evaluated and declined Wednesday 14 October 2026: an export pipeline per model and a runtime shaped for mobile applications; reopened by an aarch64 Linux build with constrained decoding |
| M | C++ | custom transformer | own implementation | not evaluated; justified only by a teaching chapter on what the engine does inside |
| N | C | custom C runtime | own implementation | not evaluated; justified only by a board where the engine binary itself is the limit, which the NanoPi's 512 MB may yet show in e01 |
| O | C | llama.cpp | a separate inference executable over a Unix domain socket or shared memory | built in e08, with `Restart=on-failure` and the restart test |
| P | any client | llama.cpp on another machine | remote inference over the network | built in e20, against `llama-server` on skyhorizon's WSL |

### Rust with C and C++, R1 to R8

| **Variant** | **Language** | **Engine or runtime** | **Integration** | **Status** |
| --- | --- | --- | --- | --- |
| R1 | Rust | llama.cpp | Rust main over FFI, the `llama-cpp-2` crate | built in e13 |
| R2 | Rust | llama.cpp | Rust main with llama.cpp in a separate process | built in e14, so that e13 and e14 price R1 against R2 on the same bus |
| R3 | Rust and C | llama.cpp | a Rust device service with a C inference daemon | built in e18 |
| R4 | Rust | ONNX Runtime or TensorFlow Lite | Rust over those runtimes | not evaluated; depends on F or H being reopened first |
| R5 | Rust and C | llama.cpp | a C ABI boundary, `edgellm_decide()` | built in e18, the boundary either side can replace |
| R6 | Rust | custom kernels | own quantised kernels | not evaluated; justified only by a measured gap on the Allwinner H3's NEON that llama.cpp's kernels leave in e01 |
| R7 | Rust | llama.cpp | one static executable | built in e05, with `industrial-io` for the ADXL345 |
| R8 | Rust or Python | llama.cpp elsewhere | remote client | built in e15 (R8-shaped in Python, against a local server) and e20 |
| candle | Rust | candle, a Rust-native engine | no C toolchain in the image | not evaluated; named in e05's Future and extra exercises, justified by a Rust project whose image must carry no C toolchain |

Tally of what is built: C in ten projects, C++ in four, Rust in four, Python in two. Integration variants built: A, B, C, D, O, P, R1, R2, R3 with R5, R7 and R8. The C-not-Python house rule is overridden in e15 and e20 because the five-criteria score put Python first there, and both chapters say so.

### The qualitative matrix, as hypotheses

The catalogue rates the variants qualitatively, in Joseph's own words of Wednesday 14 October 2026: “ratings are qualitative starting points for this hardware, not benchmark results.” His matrix is reproduced first, unchanged. Every rating in it is a hypothesis for the named measurement, never a result. A variant whose rating survives measurement is said to survive; one whose rating does not is said so in the chapter that measured it.

| **Variant** | **General LLM suitability** | **Yocto effort** | **Best fit** |
| --- | --- | --- | --- |
| A. C++ plus llama.cpp | excellent starting point | medium | local GGUF inference |
| B. C++ plus llama.cpp C API | excellent starting point | medium | clean native API boundary |
| C. C plus llama.cpp | good via C API | medium to high | existing C application |
| D. Client plus llama-server | good | medium | independent inference service |
| E. Custom ggml engine | depends on implementation | high | custom operators |
| F. C++ plus ONNX Runtime | model-dependent | medium to high | ONNX pipelines |
| G. C++ plus ONNX Runtime GenAI | model-dependent | high | supported ONNX generative models |
| H. C++ plus TensorFlow Lite | limited, model-dependent | medium to high | small compatible networks |
| I. C plus TensorFlow Lite C API | limited, model-dependent | medium to high | C-based embedded applications |
| J. C++ plus OpenVINO | model-dependent | high | supported models and operators |
| K. C++ plus MNN | model-dependent | medium to high | edge and mobile inference |
| L. C++ plus ExecuTorch | model-dependent | high | PyTorch deployment pipeline |
| M. Custom C++ transformer | depends on implementation | very high | research and custom architectures |
| N. Custom C runtime | depends on implementation | very high | fixed, specialised models |
| O. Separate inference process | backend-dependent | medium | fault isolation |
| P. Remote inference | depends on remote host | low on the Pi | Pi 3B+ with limited RAM |

What the volume measures against each rating, grouped by the variants it builds:

| **Variant** | **Throughput** | **Resident memory** | **Survives the engine stopping** | **Image delta** | **What the volume measures** |
| --- | --- | --- | --- | --- | --- |
| A, B, C one process | highest expected | lowest expected | no, the process is the engine | smallest | tokens per second by the protocol; `VmRSS` from `/proc/<pid>/status`; decision latency with `clock_gettime` |
| D over HTTP | slightly lower expected | higher, two processes | yes, the client outlives the server | medium | the same three, plus the HTTP cost as the difference against C on the same board |
| O over a socket | as C expected | as D | yes | small | the same three, plus restart time after signal 9 (`SIGKILL`) to the daemon, measured by the console's own clock |
| P remote | host-bound | minimal on the Pi | yes, and the Pi keeps running without it | none on the Pi | round-trip latency, and the ranking's agreement with the rule |
| R1, R7 Rust FFI | as A expected | as A, plus the static binary | no | medium, the Rust toolchain | the same three, against e03's C binary on the same board |
| R2, R3 with R5 | as O expected | as O | yes | medium | restart time after signal 9 (`SIGKILL`) to the daemon, and the ABI's cost per call |
| R8 and Python | as the engine, through a binding | higher, the interpreter | yes | largest, the interpreter and wheels | the same three, and the import time at service start |

### The three recipe kinds

The Yocto layer `meta-edgellm` follows the catalogue's three recipe kinds exactly:

- `recipes-app/`: one recipe per project, in the class its language needs (`cmake` for C and C++, `cargo` for Rust, `setuptools3` for Python), installing the binary or package, its prompt, its grammar and its systemd unit.
- `recipes-runtime/`: llama.cpp at the pinned tag, built with CMake and the machine's `-mcpu`. The declined engines’ recipes are not written.
- `recipes-model/`: one GGUF package per Apache 2.0 or MIT model, fetched from Hugging Face with its SHA256, plus the external model partition option in the image recipe, on by default, so that the base root filesystem stays small and a model can change without a reflash.

### The layer's checklist

The catalogue's five practical considerations are the layer's checklist, and each chapter's Embedded Linux and Yocto subsection ticks them:

**Step 1.** Pin every revision: the engine tag, the model file's SHA256, the binding crate and the wheel versions.

**Step 2.** Cross-compile for the target, with the machine's `-mcpu`, never a build on the board presented as the image's binary.

**Step 3.** Keep the model out of the base root filesystem.

**Step 4.** Test clean builds, licensing, offline operation, service restarts and upgrades.

**Step 5.** Benchmark on the production image, not only on Raspberry Pi OS, and report both.

### The order of the first three projects

The catalogue's recommended starting point is a 64-bit Yocto image plus C++ llama.cpp, the baseline first, and only then Rust bindings or a separate Rust service measured against it. That recommendation is the order of e01, e02 and e13 in this volume.

The catalogue closes on a distinction this volume keeps as the last word of this section: engines are not interchangeable, because each is compiled for ARM in its own way, and on a given board what decides the figure is the token generation path, the KV cache handling, the quantised kernels and the model architecture, not the language the application is written in.

## The bench

### Boards

| **Board** | **RAM** | **Role here** | **Card and console** |
| --- | --- | --- | --- |
| Raspberry Pi 4 Model B | read it in e01 | the upper rungs, the HATs that need it, the USB sidecars | card A `eplepi` and card B; serial console works through the Renkforce USB/TTL cable at 115200 |
| Raspberry Pi 3 Model B Plus | 1 GB | the modem HATs, the Explorer700, the IKS4A1 on flying leads | SSH only; its serial console is dead on this board (pinmux) |
| Raspberry Pi 3 Model B Rev 1.2 | 1 GB | the bare-header projects, the LCD, the SIM7020E | SSH |
| NanoPi NEO Air | 512 MB | one benchmark and the e03 variant, SmolLM2-135M only | 24-pin header, no 40-pin HAT; armv7 build |

Hostnames are card identities, not board identities: the card `eplepi` (Raspberry Pi OS, Debian 13 trixie, kernel `6.18.50+rpt-rpi-v8`, user `bing`) was logged in the Pi 3B+ on Wednesday 30 September 2026 and in the Pi 4B on Wednesday 7 October 2026. e01 names which card sits in which board on the day of its run, and every later chapter repeats it.

### HAT allocation, one HAT per header per project

A 40-pin HAT owns the header it sits on. The 3.5 inch LCD's carrier covers pins 27 to 40 and the 2x20 stacking header that would free them is not on the bench. The WS-28164 is for the Pi 4B and Pi 5 only, and its overlays cannot share one `config.txt` with the MCC 118 because both claim SPI0 chip selects, which is why the Pi 4B has two cards.

| **Host** | **Header use** | **Projects** |
| --- | --- | --- |
| Pi 4B, card A (`eplepi`) | MCC 118, live since Friday 9 October 2026 | e10 |
| Pi 4B, card B (the spare Verbatim 32 GB) | WS-28164: CAN FD, classic CAN, RS232, RS485 | e13, e14, e18 |
| Pi 4B, either card | no HAT: USB devices, the DSI 7 inch touchscreen, the keyboard | e01, e02, e11, e12, e15, e20 |
| Pi 3B+ | Explorer700 | e08 |
| Pi 3B+ | SIM7070G | e17, e19 |
| Pi 3B+ | bare header, IKS4A1 on flying leads at pins 3 and 5, INT1 on pin 18 | e07 |
| Pi 3B | bare header: ADXL345 at 0x53 and 100 kHz, VL53L4CD, LEDs on BCM 17, 27, 22 | e03, e05, e06 |
| Pi 3B | 3.5 inch LCD, pins 1 to 26 still free | e09 |
| Pi 3B | SIM7020E, with the PPK2 metering the module | e16 |
| NanoPi NEO Air | 24-pin header, SmolLM2-135M only | e01 benchmark, e03 variant |

### Sensors, sidecars and instruments

- **have:** DFRobot SEN0032 ADXL345 at 0x53: CS strapped to 3V3, SDO to GND, the bus at 100 kHz on the current leads (400 kHz answered nothing on Friday 9 October 2026), the header unsoldered so every contact is friction, the part known good; VCC pin 1, GND pin 9, SDA pin 3, SCL pin 5, CS pin 17, SDO pin 6, INT1 pin 16 (GPIO23). Header pins 3 and 5 carry the Pi's fitted 1.8 kOhm pull-ups, so no resistor is bought.
- **have:** Adafruit VL53L4CD, single-zone time of flight, 1 to 1300 mm, I2C, arrived Tuesday 6 October 2026; its ST ULD licence is to verify in e06.
- **have:** X-NUCLEO-IKS4A1 on the Pi I2C bus via flying leads, measured Wednesday 30 September 2026: 0x1E LIS2MDL, 0x38 STTS22H, 0x44 SHT40, 0x5d LPS22DF, 0x6a LSM6DSO16IS, 0x6b LSM6DSV16X; mainline IIO drivers bind through the Linux volume's project 10 overlay.
- **have:** X-NUCLEO-IKS5A1, read through the NUCLEO-H7A3ZI-Q over USB CDC as in the kit labs; its ISM6HG256X supplies the high-g events of e11.
- **have:** STEVAL-STWINBX1 STWIN.box over USB-C: IIS3DWB vibration at 6 kHz for e12; its two MEMS microphones stream the vendor's own protocol, not USB Audio.
- **have:** Four Joy-it LK-LED10 modules, blue, green, yellow, red: anode on the signal pin, drive high to light; per module at 3.3 V blue 2.94 mA, green 7.39 mA, yellow 6.20 mA, red 6.34 mA, three together 16.5 mA, all PPK2 on Saturday 3 October 2026.
- **have:** The modem HATs: SIM7070G (e17, e19), SIM7020E (e16); the SIM7600E-H is on the bench and is not seated by any project here, because its LTE transmit peaks exceed 1 A.
- **have:** Sidecars: SBC-NodeMCU-ESP32 and SBC-ESP8266-PROG over USB (e15); NUCLEO-H7A3ZI-Q, shared with other volumes and reflashed before every capture (e10's DAC stimulus on PA4, e11's hub).
- **have:** nRF PPK2: source meter 0.8 to 5.0 V up to about 1 A, or ampere meter, over USB to a host. It meters the Nucleo, an ESP32 or an NB-IoT module. It cannot power or meter a Raspberry Pi under load.
- **have:** MCC 118 as a voltage witness: 8 single-ended channels, 12 bits, 100 kS/s aggregate; its 10 microsecond sampler cannot see a nanosecond pulse and its spread floor is about 3.4 microseconds.

### Not on the bench, and what each absence costs

- **absent:** A multimeter. No rail voltage can be read independently; every "3.3 V" in this volume is the nominal value, and the MCC 118 cannot witness itself.
- **absent:** An oscilloscope and a logic analyser. No bus waveform is shown; the I2C 100 kHz finding rests on what answered and what did not, not on a trace.
- **absent:** A soldering iron. The SEN0032's header stays unsoldered, so any two-condition comparison on that part is a rider, not a result.
- **absent:** A 2x20 stacking header. The LCD project e09 lives on pins 1 to 26 and no HAT shares its header.
- **absent:** A Raspberry Pi 5 and any AI accelerator. Every published result the channel competes with uses one or both; this volume's differentiator is that it uses neither.
- **absent:** A microphone. Voice is dotted in every project. The STWIN.box microphones stream a vendor protocol and are not a USB Audio device (to verify in e12). A whisper.cpp sequel is named and not built.
- **absent:** SIM cards. e16, e17 and e19 depend on a data SIM. Without one those chapters stop at AT bring-up and registration, which is still a pass of their first acceptance stage, and their agreement tables read "not measured, no SIM".

## The twenty projects

### The learning path

In reading order, which is also the building order. e01 and e02 define the instrument and the decision loop every later chapter uses; each later project adds one interface, one HAT or one integration variant.

| **No.** | **Title** | **Language** | **Integration** | **Board and HAT** | **Model** |
| --- | --- | --- | --- | --- | --- |
| e01 | Bring-up and the honest tokens per second table | C++ | A, the upstream tools plus a C++ harness | all four boards, no HAT | the whole ladder |
| e02 | The decision loop: grammar, state reuse, control harness | C | C, plain C over llama.h; A and B priced beside it | Pi 4B, no HAT | Qwen2.5-0.5B |
| e03 | LED status explainer | C | C, libgpiod 2 | Pi 3B, LEDs on BCM 17, 27, 22; NanoPi variant | SmolLM2-360M; 135M on the NanoPi |
| e04 | Pi thermal and throttle narrator | C client | D, llama-server over HTTP, libcurl | Pi 4B, no HAT | SmolLM2-360M |
| e05 | ADXL345 event reporter | Rust | R7, one static executable over llama-cpp-2 and industrial-io | Pi 3B, ADXL345 on pins 3 and 5, INT1 on pin 16 | SmolLM2-360M |
| e06 | VL53L4CD "too close" explainer | C | C, with the ST ULD in C | Pi 3B, VL53L4CD beside the ADXL345 | SmolLM2-360M or Qwen2.5-0.5B |
| e07 | IKS4A1 climate and motion narrator | C++ | B, C++17 over the C API | Pi 3B+, IKS4A1 on flying leads | Qwen2.5-0.5B |
| e08 | Explorer700 console with a model | C | O, console plus a separate inference daemon over a Unix socket | Pi 3B+, Explorer700 | SmolLM2-360M |
| e09 | LCD prompt and verdict display | C++ | A, linked directly, DRM dumb buffer | Pi 3B, 3.5 inch LCD, ADXL345 and LEDs on pins 1 to 26 | SmolLM2-360M |
| e10 | MCC 118 voltage event labeller | C | C, daqhats C library | Pi 4B card A, MCC 118, Nucleo DAC on CH0 | Qwen2.5-0.5B |
| e11 | Nucleo CDC sensor hub narrator | C | C, tinycbor, the Linux volume's project 12 hub | Pi 4B, Nucleo plus IKS5A1 over USB CDC | Qwen2.5-0.5B |
| e12 | STWIN.box vibration band reporter | C++ | A, with a C++ FFT | Pi 4B, STWIN.box over USB-C, DSI 7 inch | Qwen2.5-0.5B or 1.5B |
| e13 | CAN honesty agent | Rust | R1, Rust main over llama-cpp-2 and socketcan | Pi 4B card B, WS-28164, two jumper wires | Qwen2.5-0.5B |
| e14 | Joint monitor: CAN plus accelerometer | Rust | R2, the e13 service with llama.cpp in a separate process | Pi 4B card B, WS-28164 plus ADXL345 | Qwen2.5-0.5B |
| e15 | ESP32 sidecar with a bounded pattern proposal | Python | R8-shaped: llama-cpp-python, ppk2-api, pyserial | Pi 4B or 3B, ESP32 over USB, PPK2 in series at 3.3 V | Qwen2.5-0.5B |
| e16 | NB-IoT sleep interval from measured microamps | C | C on the logged PPK2 profile; the capture is a Python tool | Pi 3B, SIM7020E, PPK2 metering the module | SmolLM2-360M |
| e17 | Cellular status summariser | C | C, termios AT parser | Pi 3B+, SIM7070G | Qwen2.5-0.5B |
| e18 | Mini industrial serial logger | Rust service, C daemon | R3 and R5, a C ABI, `edgellm_decide` | Pi 4B card B, WS-28164 RS485 loopback and RS232 | Qwen2.5-0.5B |
| e19 | Event-driven cellular alert composer | C | C, reusing e17's parser and the ADXL345 driver | Pi 3B+, SIM7070G plus ADXL345 | Qwen2.5-0.5B |
| e20 | "What should I measure next" and the scorecard | Python | P and R8, a Python client to llama-server on skyhorizon's WSL | Pi 4B, no HAT, network | Qwen2.5-1.5B, remote |

### What each model decides, what each rule says, and what can fail

| **No.** | **Model decides or writes** | **Control rule** | **Acceptance that can fail** |
| --- | --- | --- | --- |
| e01 | nothing; this chapter is the instrument | upstream llama-bench | harness within 10 percent of llama-bench on each board; throttle bits clear before and after; two-day spread under 15 percent |
| e02 | an enum from a replayed CSV | the rules.c library | 1,000 decisions, zero free text; state restore at least twice as fast as a cold prompt |
| e03 | the one-line reason for the colour the rule chose | the rule that lit the LED | cause named correctly in a stated fraction of 200 events; LED current from the PPK2 figures of Saturday 3 October 2026 |
| e04 | COOL, WARM or THROTTLED, plus a summary | throttle bit 2 or 3 set, or thermal\_zone0 above 80 C | 95 percent agreement during an e01 run; the self-heating of narrating measured |
| e05 | NONE, TAP, TILT or FREEFALL | peak above 2 g; axis change above 30 degrees; magnitude below 0.3 g | 100 hand-labelled events, three-way agreement |
| e06 | CLEAR, APPROACHING or TOO\_CLOSE | below 300 mm twice; falling trend over five readings | 200 approaches; repeatability at 200 mm; decision latency against approach speed |
| e07 | a pressure trend enum plus three sentences | LPS22DF slope over 30 minutes | 24 hour log; hallucinated numbers counted |
| e08 | a 20-word answer to five canned questions on the OLED | DS3231 against the Pi clock | 50 of 50 answers within 1 percent of sysfs; the buzzer never on model output |
| e09 | reuses e05 | e05's rule | frame update under 200 ms; touch toggles the panes 50 of 50 |
| e10 | STEP, RAMP or NOISE | hysteresis crossing with a slope test | 300 stimuli; step amplitude within 2 percent of the commanded value |
| e11 | QUIET, LOW\_G or HIGH\_G, plus a sentence | ISM6HG256X above 16 g | 100 impacts; the CDC counter has no gaps |
| e12 | BASELINE or BAND\_SHIFT, plus two sentences | band ratio above twice the baseline | 50 plus 50 windows; the sample rate verified |
| e13 | per node PRESENT, MODELLED or ABSENT | three missed heartbeats; a vcan source is MODELLED | 1,000 periods with dropouts; bus-off recovery observed |
| e14 | OK or INSPECT | error counter rising and a vibration peak in the same minute | 100 minutes with injected errors and taps |
| e15 | PATTERN\_A to PATTERN\_D within a budget | the cheapest pattern meeting the brightness | PPK2 current per pattern repeats within 5 percent; 200 prompts |
| e16 | 60, 300, 900 or 3600 seconds, plus a sentence | the largest interval meeting the battery target within the PSM grant | PPK2 cycle profile within 10 percent over five cycles; 100 scenarios |
| e17 | OK, DEGRADED or DOWN, plus three sentences | registered, and CSQ at least 10 | 24 hours; hallucinated operator names counted |
| e18 | NOMINAL or DRIFT, plus a sentence | delta above a fraction of range | 1,000-frame RS485 echo at zero errors; replay agreement |
| e19 | a 160-character message with event, peak and time | send only on an e05 TAP or FREEFALL | 50 events checked field by field; invented numbers counted |
| e20 | the ranked next three measurements | sort by N ascending, then agreement ascending | top pick matches the rule in a stated fraction; the twenty-row scorecard complete |

### Draft ideas that did not survive as their own projects

The draft of Wednesday 14 October 2026 carried twenty ideas. Eleven are here under their own numbers, re-framed where a bench fact required it. The rest did not survive as separate projects, and the reason is recorded so that it is not rediscovered:

- **Draft 9**, a sleep-interval calculator: a deterministic C function with no decision for a model to make. Folded into e16, where the model chooses among intervals the rule has already declared safe.
- **Draft 13**, an LED state machine: a rule, and only a rule. Re-framed as e03, where the rule lights the LED and the model explains why.
- **Draft 16**, a measured Raspberry Pi power narrator: the PPK2 cannot meter a Raspberry Pi, so no Pi current exists to narrate. Re-framed as e04 on the throttle bits and the temperature, which the board does report.
- **Draft 10**, a voice console: there is no microphone on the bench. The console half became e08 on the Explorer700 OLED and the display half became e09 on the LCD; voice is dotted in both.
- **Draft 17**, a dual-radio failover: already a rule in kit lab P13, with nothing for a model to add. Folded into e17 as one of the states the summariser describes.
- **Drafts 4 and 14**, a climate narrator and a motion narrator on the same shield: one IKS4A1 on one bus is one project. Merged into e07.
- **Draft 12**, a cellular message composer: split by what the message does. Status went to e17 and the event-driven send to e19.
- **Drafts 15 and 19**, an advisor that says what to measure next and a results dashboard: the two are one question asked twice, because the ranking that orders the scorecard is the advice. Merged into e20, where the rule ranks by evidence and the model proposes three next measurements against that ranking.

## The six videos

Part II ranks six three-minute concepts to open a channel under a new topic name, each built on one of the projects above and each carrying its subject, storyline, script outline, punchline, why and why not, differentiator, format, cadence, the best practices it cites from fetched pages, and the channels it borrows from. All six are horizontal 16:9 long-form, because the help page at [support.google.com/youtube/answer/15424877](https://support.google.com/youtube/answer/15424877) (verified Wednesday 14 October 2026) classes a square or vertical upload of up to three minutes as a Short; each gets a 45 to 60 second vertical cut that points back. The top three get full outlines. Part II is local to the catalogue and is not published.

| **Rank** | **Code** | **Concept** | **Project** |
| --- | --- | --- | --- |
| 1 | v01 | A Pi 3 runs a language model offline and a two-line rule checks it | e06 and e09 |
| 2 | v02 | What is real on this CAN bus: solid, dashed, dotted, reported by a local model | e13 |
| 3 | v03 | A local model picks how long an NB-IoT module sleeps, from measured microamps | e16 |
| 4 | v04 | Cellular status explained by a local model | e17 |
| 5 | v05 | How fast is the Pi you already own at a language model, honestly | e01 |
| 6 | v06 | Three LEDs, one small model, the sentence it writes | e03 |

---

[Contents](../CONTENTS.md) &nbsp;&nbsp;|&nbsp;&nbsp; [Next](01-bring-up-and-the-honest-tokens-per-second.md)
