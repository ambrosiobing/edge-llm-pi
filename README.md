# edge-llm-pi

Twenty projects that run a small local language model on the Raspberry Pi 3, 3B+ and
4 already on the bench (and a NanoPi NEO Air for one benchmark) and make it act on the
parts that are there: LEDs, an accelerometer, a time-of-flight sensor, a sensor
shield, a DAQ HAT, a CAN HAT, cellular modems, a power profiler. Every project pairs
the model with a two-line control rule and publishes the agreement table between
them over a logged run of at least 100 events. The rule acts; the model proposes or
explains. A project where the model adds nothing is a valid result.

## Read a chapter

The book is in `chapters/`, one Markdown file per project, with the figures in
`figures/*.svg`. `chapters/` and `CONTENTS.md` are produced by the catalogue's publish
step from the volume's sources and are not in this tree yet; until that step runs,
the per-project `README.md` under `projects/` and the code are the record. Start with
`chapters/01-bringup.md` (the honest tokens-per-second table) and
`chapters/02-decision-loop.md` (the grammar, the state reuse and the control harness
every later chapter links), then any project whose parts you have.

## Run a project on the host

Nothing here needs a board or a model file. The stub engine answers from the grammar
alone and the whole loop runs, logs and is checked against each project's oracle file.

    git clone https://github.com/ambrosiobing/edge-llm-pi.git
    cd edge-llm-pi
    git submodule update --init --recursive
    cmake -S . -B build -DEDGELLM_STUB_MODEL=ON
    cmake --build build
    ctest --test-dir build --output-on-failure

With a model (see `models/MODELS.md` for the files, sizes and checksums) and without
the stub option, the same build links llama.cpp from `third_party/llama.cpp` at the tag
in `cmake/llama_tag.cmake` and the project runs the real engine over its replay file:

    ./build/projects/e06-too-close/edgellm-e06 --project e06 \
        --replay projects/e06-too-close/replay/approach.csv \
        --model models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
        --grammar projects/e06-too-close/grammar.gbnf \
        --prompt projects/e06-too-close/prompt.txt --log run.csv
    python tools/agreement.py run.csv

Rust projects: `cargo test --manifest-path projects/eNN-slug/Cargo.toml`. Python
projects: `python -m pytest projects/eNN-slug`. The host-side Python is pinned in
`requirements.txt` (`python -m venv .venv`, then `pip install -r requirements.txt`).

Compiling is a Linux job (skyhorizon's WSL, the board, CI); the authoring laptop has no
compiler by rule, which is why the stub and the oracle files exist.

## Wire the board

`BENCH_SITTINGS.md` is the first file to read, because each `wiring.md` below describes
its own project alone and several projects share one wiring. It groups the twenty into
eleven sittings, gives the union pin table for the largest of them (e03, e05 and e06 on
the bare header of the Pi 3B, three projects for one setup), and says which pairs look
like one sitting and are not.

Each project's `wiring.md` carries its pin table (header pin, BCM number, part pin,
wire colour where known), the pre-flight commands (`i2cdetect -y 1`, the WHO_AM_I read,
`ls /dev/spidev*`, `ip link`, `/dev/serial/by-id`), the `config.txt` lines and overlay,
the one-HAT-per-header rule for that Pi, and the acceptance command. The bench run is
the same binary with `--live <backend> <spec>` instead of `--replay`, for example

    edgellm-e05 --project e05 --live iio /sys/bus/iio/devices/iio:device0:0.1 ...
    edgellm-e13 --project e13 --live can can0 ...

A project whose events and whose outputs are different devices keeps `--replay` or its
own live source for the events and names the output separately, because a run opens one
event source. Project e03 is the case: the events are a replayed stream and the three
LEDs are real, so its line is

    edgellm-e03 --project e03 --replay replay/state_200.csv --leds /dev/gpiochip0:17,27,22 ...

and its `wiring.md` says why. Read the project's own manifest before copying a command.

The backends (`replay`, `sysfs`, `gpio`, `i2c`, `iio`, `serial`, `can`) are documented
at the top of each `common/c/src/io_*.c`; a backend is compiled in with its
`-DEDGELLM_IO_<NAME>=ON` option and a binary without it says so at `--live`.

The Yocto layer `meta-edgellm/` builds a bench image with llama.cpp, the chosen models
and the project services on top of the bench's existing kas setup:
`kas build meta-edgellm/kas/edgellm-pi4.yml` on skyhorizon's WSL (hours for the first
build, minutes after). The image build is not the first run; `scp` plus the unit on
Raspberry Pi OS is.

## Models

`models/MODELS.md`: the llama.cpp tag, the C API symbols verified against it, and the
ladder of eight GGUF files with file name, size, SHA256, licence and the rung each sits
on. Apache 2.0 and MIT models have a recipe under `meta-edgellm/recipes-model/`; Llama
3.2 and Gemma are linked only. No weights are in the repository.

## Rules

- No authoring files: the book's LaTeX, PDF and HTML stay in the catalogue; only the
  Markdown chapters and the code are here. `.gitignore`, the pre-commit hook and CI
  refuse them.
- No attribution of tools, assistants or models in any file or commit; the word list is
  `tools/no_trace_words.txt`, the scan `tools/no_trace_scan.py`, the hook
  `.githooks/commit-msg`.
- Install the hooks once per clone: `sh tools/install-hooks.sh`.
- ASCII, 99 columns, full dates, no dashes as punctuation: `tools/line_check.py` and
  `CONTRIBUTING.md`.

## Layout

    common/c/        the decision core (edgellm.h), io.h and its backends, the logger,
                     the rule registry, the shared main
    common/cpp/      edgellm.hpp, the RAII face over the C API
    common/rust/     the shared crate (e05 author)
    common/python/   the shared package (e15 author)
    cmake/           the llama.cpp pin and edgellm_add_project
    projects/        eNN-slug, one per project, in that project's chosen language
    grammars/        one GBNF per decision enum, conventions in grammars/README.md
    tests/           oracle_check, rules_test, the smoke project, tests/oracle/
    tools/           Python only: agreement, bench_runner, ppk2_capture, plot, scans
    meta-edgellm/    the Yocto layer: recipes-app, recipes-runtime, recipes-model,
                     recipes-images, kas/
    models/          MODELS.md
    third_party/     llama.cpp as a submodule at the pinned tag

Licence: MIT (`LICENSE`). The models carry their own licences, named in
`models/MODELS.md`.
