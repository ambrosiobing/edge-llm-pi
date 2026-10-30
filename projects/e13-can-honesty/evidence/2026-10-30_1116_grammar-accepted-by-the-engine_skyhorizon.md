# e13: the grammar put to the engine that refused its predecessor, Friday 30 October 2026, 11:16

    host       skyhorizon, WSL side, not a board and not the authoring laptop
    engine     llama-server from the submodule at d81235049384534c167caea52b85a694f6103d14,
               the volume's pin; bound to 127.0.0.1 so no port was exposed
    model      qwen2.5-1.5b-instruct-q4_k_m.gguf, the file verified by sha256 on
               Thursday 29 October 2026; the model is incidental here, the parser is not
    toolchain  cargo 1.99.0, rustc 1.99.0, against this manifest's pinned floor of 1.75
    subject    grammars/e13_presence.gbnf as rewritten in 3cba5a7

**This closes no clause of e13 and measures nothing about the project.** e13's acceptance
wants 1000 heartbeat periods on a real bus, bus-off observed and recovery seen. No CAN
controller was involved and no HAT is seated. What this records is that the grammar fix made
this morning is accepted by the engine, which is the one thing the project's own tests cannot
tell us.

## Why a green test suite was not enough

`cargo test` passed before this was run:

    rules:  9 passed, including the_name_table_is_the_grammar_order
    oracle: 4 passed, including the_stub_run_matches_the_oracle, 4000 rows

**And that proves less than it looks.** The manifest's `default = ["stub"]`, so those tests
exercise the volume's own grammar reader, which is documented as accepting "a body continuing
over later lines". That reader is exactly the one that accepted the **broken** grammar for a
fortnight while llama.cpp refused it. A green stub suite is precisely the state e13 was in on
Thursday 29 October 2026 while holding a guaranteed first-run failure.

So the suite answers "did the fix move the oracle", which matters, and cannot answer "can the
engine parse this", which is what the fix was for.

## The check that can answer it

The grammar was handed to the engine directly, built into the request body by `json.dumps`
from the file itself so that no shell quoting could alter it:

    python3 -c "import json;print(json.dumps({'prompt':'x','n_predict':24,
                'grammar':open('grammars/e13_presence.gbnf').read()}))" > /tmp/e13.json
    curl -sS -X POST http://127.0.0.1:8080/completion \
         -H 'Content-Type: application/json' --data-binary @/tmp/e13.json

    {"index":0,"content":"{   \"node\": \"joint-d\", \"decision\": \"PRESENT", ... }

**No HTTP 400 and no parse error.** The server's console shows the task launching and
completing normally, 24 tokens predicted, `truncated = 0`.

And the content is visibly constrained rather than merely well formed: an opening brace,
whitespace that `ws ::= [ \t]*` admits, the `"node"` key, one of the four node literals, the
comma, the `"decision"` key, and one of the three decision literals. It stops mid-literal
because `n_predict` was 24, which is the budget and not a fault.

**It answered `joint-d`, the fourth alternative.** That is worth one line: the stub always
takes the first expansion, so a grammar that only ever yielded `joint-a` would be consistent
with a broken alternation. Sampling reaching the fourth literal shows the alternation is live.

## The contrast with Thursday, which is the whole point

    Thursday 29 October 2026   the predecessor grammar, same engine, same route
                               parse: error parsing grammar: expecting ::= at ws "}"
                               E srv send_error: failed to parse grammar, HTTP 400

    today                      the rewritten grammar, same engine, same route
                               a constrained completion, no error

Same engine at the same pin, same endpoint, two grammars, two outcomes. That is a controlled
comparison rather than an assertion that the fix works.

## What this establishes, and the method it leaves behind

    established   the engine parses e13's rewritten grammar and constrains generation by it
    established   the stub suite still matches the oracle over 4000 rows
    established   the alternation is live, not collapsed to its first literal
    NOT shown     anything about e13's acceptance, which needs a bus
    NOT shown     that the generated object is correct for any input; the prompt was "x"

**The method is the part to reuse.** Any grammar in this volume can be put to the engine in
two commands without a board, a bus or a sensor, because the engine is on skyhorizon and the
check needs no project binary at all. The volume has 38 grammar files and a gate that pairs
them without parsing either copy. Until that gate learns to parse, this is the cheapest thing
that distinguishes a grammar the stub likes from a grammar the engine accepts, and the two
are not the same set.
