# House rules

One page. Every rule here is checked by a tool in `tools/` or a hook in `.githooks/`,
and the tool is named beside the rule.

## Languages

Bench and portfolio code is C and C++. Python appears only under `tools/` (instrument
harnesses, the agreement table, the scans) and in the two projects whose chapter
scored Python highest on its own five criteria (e15, e20); Rust in the four projects
that chose it (e05, e13, e14, e18's service). A project's language is the one its
chapter names in "Variant chosen", and its source tree holds that language only.

## Files

- ASCII only, lines at most 99 columns, LF line endings (`.gitattributes`). Checked by
  `python tools/line_check.py`.
- No em dash, no en dash, no `--` as punctuation in prose. Caught by the ASCII rule.
- Dates in full every time: weekday, day, month, year ("Wednesday 14 October 2026").
- No violent idioms; a process is stopped, a claim withdrawn, a test refused.
- No employer names; no diary, correspondence or log that names a person.
- The authoring half of the volume (`.tex`, `.pdf`, `.html`, `.aux`, `.log`, `.toc`,
  `sections/`, `figures/*.tex`, the build scripts, anything under `build/`) never
  enters the repository. `.gitignore` is the first fence, `.githooks/pre-commit`
  running `tools/no_authoring_files.py` the second, CI the third.
- Model weights (`*.gguf`) and saved states (`*.state`) are ignored; the table in
  `models/MODELS.md` says where each file comes from and what its SHA256 is.

## No attribution of tools, assistants or models, anywhere

No file, comment, commit message or commit trailer names an assistant, a model vendor,
a code-generation product or says that something was generated. The list of refused
words lives in `tools/no_trace_words.txt` and nowhere else, so that the scanner and
the hook do not themselves carry the words they look for and a search of the tree for
any of them finds exactly one file, the list. `tools/no_trace_scan.py` reads the list
and scans every file git would commit plus every message and identity in
`git log --all`; `.githooks/commit-msg` reads the same list and refuses a message
that matches any line of it, which covers the trailer that credits a second author.
CI runs the scan on every push. Install the hooks once per clone:

    sh tools/install-hooks.sh

Every commit is `ambrosio <ambrosiobing@gmail.com>`.

## Code

- The hardware is reached only through `common/c/include/io.h`. A project's loop is
  the same binary with `--replay` on the host and `--live` on the bench.
- The rule acts, through `io_actuate`; the model proposes or explains. A project in
  which the model's output reaches an LED, a buzzer, a modem or any protective action
  is wrong by construction and is not merged.
- Every rule is a function in `rules_eNN.c`, registered with `RULE_REGISTER`, and has
  cases in `rules_eNN_test.c` (`tests/rules_test.h`). Every project has an oracle
  file in `tests/oracle/` and registers its replay test with `edgellm_add_oracle_test`.
- Every number in a chapter names the instrument or says it was not measured. A
  tokens-per-second figure is published only beside `llama-bench` on the same board,
  model and thread count, within 10 percent (`tools/bench_runner.py`).
- Every URL in a chapter or a README is marked verified with its full date or
  "to verify". The same for a checksum, a licence and a package version.

## Before a commit

    python tools/line_check.py
    python tools/no_trace_scan.py
    python tools/no_authoring_files.py

and, where a compiler exists (skyhorizon's WSL, CI; never the authoring laptop):

    cmake -S . -B build -DEDGELLM_STUB_MODEL=ON
    cmake --build build
    ctest --test-dir build --output-on-failure

Nothing is pushed by anyone but Joseph.
