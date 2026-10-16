# Grammars

One GBNF file per decision enum, named `eNN_<name>.gbnf`, copied or linked into the
project as `projects/eNN-slug/grammar.gbnf`. The grammar is what makes a decision an
enum rather than free text: the sampler can only emit a token the grammar allows, so
the log's model column is one of the literals or nothing.

## Conventions, enforced by the stub and by reading

- The root symbol is named `root`. `llama_sampler_init_grammar(vocab, text, "root")`
  looks for exactly that name, and so does `common/c/src/edgellm_stub.c`.
- A decision grammar produces one JSON object with one key, `decision`, whose value is
  one of the enum literals, in the SAME order as the rule's name table in
  `rules_eNN.c`. The first literal is what the stub answers in CI, so put the quiet
  state first (`COOL`, `CLEAR`, `OK`, `NONE`), never the alarm.
- Whitespace goes through one `ws` rule, `ws ::= [ \t]*`, and never `[ \t\n]*`: a
  newline inside the object costs tokens and lets a small model wander.
- No `x? x?` chains. Optional items in sequence let the model emit nothing and still
  satisfy the grammar; write the alternatives out.
- A sentence grammar bounds the length with a repetition range, for example
  `[A-Za-z0-9 ,.%-]{8,160}`, and ends the sentence with a literal `"."` so that the
  end-of-generation token is reachable only after a full stop.
- `n_predict` is set in the project, 8 for an enum and 64 for one sentence; the grammar
  never relies on it.

## The house shape

    # e06, the VL53L4CD explainer: CLEAR, APPROACHING, TOO_CLOSE
    root     ::= "{" ws "\"decision\"" ws ":" ws decision ws "}"
    decision ::= "\"CLEAR\"" | "\"APPROACHING\"" | "\"TOO_CLOSE\""
    ws       ::= [ \t]*

The stub expands this to `{"decision":"CLEAR"}` (first alternative of every rule,
see `common/c/src/edgellm_stub.c`), which is also what the oracle file expects in
its model column for every row in CI. `tools/agreement.py` reads the `decision` key
back out and compares it with the rule column letter for letter.

## Checking a grammar before a run

    ./build/bin/llama-gbnf-validator grammars/e06_too_close.gbnf sample.txt

is the upstream check (built when `EDGELLM_BUILD_LLAMA_TOOLS` is on); the stub run in
`ctest` is the cheaper one and fails when `root` is missing.
