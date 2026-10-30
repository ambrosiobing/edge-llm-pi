# e20: fifty presentations against the remote 1.5B, Thursday 29 October 2026, 21:15

    client     Raspberry Pi 4 Model B Rev 1.4, card eplepi, 10.42.0.70, Python 3.13.5,
               nothing on the header and no HAT seated
    server     skyhorizon, WSL side, llama-server bound 0.0.0.0:8080, -c 1024 -t 8
    engine     llama.cpp submodule at d81235049384534c167caea52b85a694f6103d14, the pin;
               the build stamps itself 0.6.0-dev, which is the pinned commit
    model      qwen2.5-1.5b-instruct-q4_k_m.gguf, 1117320736 bytes,
               sha256 6a1a2eb6d15622bf3c96857206351ba97e1af16c30d7a74ee38970e434e9407e
    link       bench LAN 10.42.0.0/24 through a Windows portproxy, WSL being NAT'd
    log        evidence/e20_run.csv, 51 lines 6812 bytes; evidence/e20_proposals.csv,
               151 lines 11222 bytes; both byte counts confirmed on the board and after copy

**Clause two was already met this afternoon. Clause one fails, and fails hard.**

    acceptance   the model's first identifier is the rule's on at least 40 of 50
    measured     15 of 50, 30.0 percent
    exit         1, the armed condition reporting its own shortfall

## THE HEADLINE, WHICH IS WORSE THAN THE HEADLINE FIGURE

    reading                        result
    position 1, what is published  15 of 50   30.0 percent
    position 2                      5 of 50   10.0 percent
    position 3                      2 of 50    4.0 percent
    ORDERED TRIPLE                  0 of 50    0.0 percent
    SET OF THREE, ANY ORDER         0 of 50    0.0 percent

**Not once in fifty attempts did it name the right three projects in any order.** The rule's
answer is `e01, e05, e06` on all fifty, by design: the rule cannot see the presentation.

**AND THE CI STUB BEATS IT BY SEVENTY POINTS.** The stub answers `e01` three times on every
presentation, reads nothing and applies no sort, and scores 100.0 percent on the published
figure against this model's 30.0. That is the prediction made in this morning's record,
confirmed by measurement rather than argument: the published figure cannot separate ranking
from not ranking, because the trivial answer is also the correct first answer. The ordered
triple separates them immediately, 0.0 against the stub's 0.0, and both are honest.

## UNDER THE GRAMMAR THE FAILURE IS NOT FORMAT AND NOT HALLUCINATION

    0 answered no identifier, over 50 events
    150 reasons, every one between 20 and 106 characters, the grammar allowing 20 to 200

Every answer was a well-formed array of three objects under the grammar. More than that, **the
model reads the table correctly and quotes it faithfully.** Its reasons carry the right
numbers for the row it picked:

    e04 rank 1   "180 evidence 17.2 percent quieter 9 louder 4 no literal 136"
    e05 rank 2   "0 evidence 0 agreement not measured (a CI stub run of 2000 rows at 0.0 ..."
    e03 rank 1   "200 evidence, 100 agreement, 0 disagreement"

Those are the rows' own figures, correct in every digit, some lifted near verbatim from the
rule's own note text. **The third one is the whole chapter in a line.** e03 has the most bench
evidence of any row and the best agreement, 200 events at 100.0 percent, so the rule ranks it
**last** of twenty. The model ranked it **first** and quoted the two numbers that disqualify
it as though they supported it.

So the model can read a row, can copy it without inventing anything, and can satisfy a
constrained grammar. What it cannot do is apply a three-key comparison it was handed in
writing. The system prompt states the rule in full, including "where not measured ranks first".

## THE MECHANISM, MEASURED RATHER THAN ASSERTED

**It is not position bias.** The obvious hypothesis is that it names whatever it saw first,
since `present(rows, seed)` reshuffles the twenty rows per event. Tested against the actual
shown order:

    picked from the top five shown    12 of 50, 24 percent
    uniform would be                  25 percent

Flat. The picks spread across all twenty shown slots. It is not reading off the top.

**It is a bias toward low-numbered identifiers**, which is the order the grammar lists them in
and the order the volume numbers its projects:

    rank    mean identifier ordinal    e01..e04's share    uniform
    1       3.56                       86 percent          20 percent
    2       7.24                       42 percent          20 percent
    3       6.28                       52 percent          20 percent

    all 150 picks, mean ordinal 5.69, against 10.5 for uniform over e01..e20

**And that bias explains the whole agreement profile.** The targets are `e01`, `e05`, `e06`:

    e01   ordinal 1, heavily favoured     -> 30.0 percent at position 1
    e05   ordinal 5                       -> 10.0 percent at position 2
    e06   ordinal 6                       ->  4.0 percent at position 3

`e06` appears **twice in 150 picks**. The agreement at each position tracks how favoured that
position's correct identifier happens to be, not any ability to order rows. The 30.0 percent
is e01 being a popular guess, not e01 being correctly ranked first.

## THE RUN WAS BLOCKED FIRST BY A GRAMMAR THE STUB ACCEPTS AND THE ENGINE REFUSES

Recorded in full in commit `5ccd8e1`, summarised here because it is why this run is dated
today and not a fortnight ago. The first attempt returned HTTP 400. llama.cpp's GBNF parser
ends a rule at a newline unless the line ends with a pipe, and e20's `item` rule continued
onto a second line after a rule reference. The volume's own GBNF reader in `remote.py` is
documented as accepting "a body continuing over later lines", so the stub parsed it, the
oracle test passed, and CI runs the stub. **A grammar the engine cannot parse passed every
gate for a fortnight.** `grammar_pairs.py` checks that the two copies match, never that either
parses. A sweep of all 38 grammar files found the same shape in e13's, which has never been
run and would have failed identically.

**The client misreported it twice over.** It said "could not reach llama-server" and advised
checking that the server listens on `0.0.0.0` and that the Pi can reach the host. The server
had been reached; it answered 400. `urllib.error.HTTPError` is a subclass of `URLError`, so
the refusal fell into the unreachable branch, and the response body, which carried the
server's reason, was discarded unread. The diagnosis came from reading the server's own
console instead. That defect is not fixed yet.

**Fixed on Friday 30 October 2026 in `a37bf79`**, after this reading was taken and so
recorded as a pointer rather than by editing the sentence above. `HTTPError` is now caught
before the `URLError` branch, its body is read into the message, and it raises
`RemoteRefused`, a subclass of `RemoteUnreachable` so the chapter's fallback is unchanged.
Five cases cover it, and putting the clauses back in the old order fails three of them with
the exact message quoted above.

## THE BOARD DID NOT WARM, WHICH IS WHY THIS PROJECT WAS RUNNABLE AT ALL

    before   throttled=0x0   temp=43.3'C
    after    throttled=0x0   temp=43.3'C
    elapsed  10m52s, 50 events, 13.0 s per event

Not a tenth of a degree across eleven minutes, and the register clean at both ends. e20 is the
one project in the volume whose client runs no model, so it is the only one that could be run
on a bench with no cooling, which is what four other clauses are waiting for.

A note on the timing: a two-event probe measured 5.2 s per event and the full run measured
13.0. The probe was unrepresentative and should not have been trusted as a rate. Every
presentation reshuffles the rows, so `cache_prompt` only ever matches the shared system
prefix, never the table.

## A TOOL READING THAT LOOKS LIKE 35 DEFECTS AND IS AN ARTIFACT

`agreement.py` reports `not a decision of the rule 35`, and every chapter's first acceptance
clause wants that number to be zero. **Here it is structural and means nothing of the kind.**
The tool infers its vocabulary from the decisions the rule emitted in that log, and e20's rule
emits `e01` on all fifty rows by design, because it is invariant to the presentation. So every
disagreement, however well formed, falls outside the vocabulary. The client's own count is the
honest one: **0 answered no identifier**.

This is the same blind spot e06's pre-flight recorded, firing again, but with a difference
worth noting: for e06 it was a short-log accident that a longer run would cure. **For e20 it
can never be cured**, because the rule's invariance is the chapter's design. Two tools
disagree and the one to believe is the loop's own counter.

## A CONTROL THAT REMOVES THE GRAMMAR, WHICH EXONERATES IT

**The confound this control exists to test.** Under the grammar every identifier is spelled
`"e` then two digits, and at the first digit the automaton admits only `0`, `1` or `2`. A
plain token-level preference for `0` would therefore produce the low-ordinal skew measured
above **without the model holding any opinion about ranking at all**. The measurement cannot
separate a ranking failure from a decoding artifact of the grammar's own shape, so the grammar
had to be taken away.

Run through `llama-server`'s own web UI, which applies the model's chat template server side
and **sends no grammar**, with the UI's default temperature of 0.8 rather than the run's 0.3,
on the same table and the same presentation, seed 2026:

    source                                      pick 1  pick 2  pick 3
    the scored run, grammar, temp 0.3           e04     e05     e11
    chat box 1, no grammar, temp 0.8            e04     truncated
    chat box 2, no grammar, temp 0.8            e04     e05     truncated
    chat box 3, NO GRAMMAR, TEMP 0.3, -c 4096   e04     e11     e17
    the rule                                    e01     e05     e06

**The third sample is a single-variable control.** Temperature was set to 0.3 in the UI, and
`top_k` 40 and `top_p` 0.95 are the UI defaults, which are also the server defaults the client
inherited because its payload names only `temperature`. So the only thing differing from the
scored run is the grammar.

**Remove the grammar and it still answers `e04` first.** Three samples, two temperatures, and
the first pick never moves. If the grammar's alternation order were steering the answer,
removing it should have moved it. So the bias is the model's, the grammar is not manufacturing
it, and this record's account of the first pick stands.

What the grammar *does* move is picks two and three: `e05, e11` under it against `e11, e17`
without it. One sample each, so that is a direction and not a measurement.

**And the grammar was doing real work on the shape.** Unconstrained, the first sample emitted
`"name": "e04"` and the second dropped the identifier key altogether, carrying only `"reason"`
with the project named inside the prose. The system prompt asks for three objects each with a
reason and never names the key; `"project"` came from the grammar every time. The ranking went
wrong under the grammar, but the parseability that let it be scored at all came from there.

### The complete triple, taken at `-c 4096` so nothing truncated

    the rule                      e01, e05, e06
    unconstrained, complete       e04, e11, e17

Zero of three, and no set overlap either. **`e04` first in all three unconstrained samples.**

**AND THE REASONS NOW MISREPORT THE ROWS, WHICH THE CONSTRAINED RUN'S DID NOT.**

    row as shown   e11 evidence 0 agreement not measured (a CI stub run of 6236 rows at
                   5.7 percent, which is not evidence)
    model wrote    "6236 evidence, 5.7% agreement, 5.7% louder, 48.7% no literal"

    row as shown   e17 evidence 0 agreement not measured (a CI stub run of 1440 rows at
                   68.8 percent, which is not evidence)
    model wrote    "1440 evidence, 68.8% agreement, 68.8% no literal"

    row as shown   e04 evidence 180 agreement 17.2 percent quieter 9 louder 4 no literal 136
    model wrote    "180 evidence, 17.2% agreement, 9 louder, 4 no literal"

Three failures, and the first governs the whole ranking. **It reads a stub run's row count as
the evidence count.** The row says `evidence 0`, the row's own parenthesis says `which is not
evidence`, and the system prompt says "A CI stub run is not evidence". Told three times in the
same prompt, it wrote "6236 evidence". Second, it **mislabels columns**: e04's row reads
`quieter 9 louder 4 no literal 136` and it wrote "9 louder, 4 no literal", shifted by one and
dropping 136. Third, it **invented 48.7 percent**, a figure that appears nowhere in e11's row.

**That is a change from the constrained run and it cuts against this record's own earlier
paragraph.** Under the grammar the reasons were faithful, including `"0 evidence"` for a stub
row and e04's figures complete and correctly labelled. Unconstrained, on the same table, they
are not. So the grammar was suppressing invention as well as supplying the `project` key. Why
is not established: the `reason` character class is `[A-Za-z0-9 ,.()]`, which admits no `%`
and no `:`, so near verbatim copying of the row text may simply be the cheapest path under the
constraint, while unconstrained paraphrase is where fields get dropped and numbers invented.
**One unconstrained sample against fifty constrained ones does not settle that**, and it is
recorded as an observation to test rather than a finding.

**One more caveat against this record's aggregate claim.** The unconstrained answer picked
`e11` and `e17`, which are high-ordinal, where the low-ordinal bias measured over the fifty
constrained events would not expect them. The aggregate claim rests on 150 picks and one
sample does not overturn it, but the constrained and unconstrained regimes may differ in more
than the key name, and the honest position is that only the grammar-exoneration is settled.

**The limits of this control, which are real.** Three samples on one presentation, the first
two truncated by the context, and the framing not identical: the web UI applies the chat
template server side and prepends its own system message, where the client sends raw ChatML
carrying e20 own system prompt, and the paste folded that prompt into the user turn. It settles the narrow question
it was built for, whether removing the grammar moves the first pick, and it settles nothing
about the aggregate distribution over fifty presentations.

**Both samples ran out of context at exactly 1024 tokens**, 981 and 961 prompt tokens against
generation of 43 and 63, because the web UI carries conversation history and its own system
message where the client sends a stateless completion each time. That is not a defect in
either: the client's worst case is 754 prompt plus a 192 budget, 946 against 1024, so the
chapter's `-c 1024` is sound for the scored path with about 78 tokens to spare. It is simply
too tight for an interactive chat on the same server.

## THE PROMPT FORMAT IS NOT THE CAUSE EITHER, CHECKED AGAINST THE MODEL'S OWN TEMPLATE

`/completion` is a raw route: it applies no chat template, so the client has to emit the
ChatML itself, and a missing assistant generation prompt would leave the model completing a
user turn rather than answering one. Checked against the Qwen2.5 template the server reports:

    Qwen2.5's template   <|im_start|>system\n{s}<|im_end|>\n
                         <|im_start|>user\n{u}<|im_end|>\n
                         <|im_start|>assistant\n       (when add_generation_prompt)

    what e20 emits       '<|im_start|>system\nYou are given a scorecard of twenty proje...'
                         '...the order the rule gives.<|im_end|>\n<|im_start|>assistant\n'

**Byte for byte what the model's own template would produce**, generation prompt included.
`common/python/edgellm/edgellm/template.py` gets it right, and its docstring says the strings
match `common/c/src/edgellm_main.c` so the C and Python loops agree.

So three candidate causes have now been ruled out by evidence rather than by argument: the
context overflow, the shown-position bias, and the prompt format. The low-ordinal bias is what
is left standing.

## WHAT THE SERVER'S OWN LOG ADDS, INCLUDING A HYPOTHESIS OF MINE IT KILLS

The server console was read after the run. Four things come out of it.

**The context was never the problem, and I had wrongly suspected it.** When the first attempt
returned 400 I named two candidates, a rejected field and the prompt overflowing `-c 1024`,
and estimated the prompt at about 724 tokens. The server measures it:

    prompt eval    642 to 754 tokens per task
    n_ctx_slot     1024
    truncated      0, on every one of the fifty tasks

So the estimate was close and the hypothesis was wrong: `-c 1024` was adequate throughout,
nothing was ever truncated, and the chapter's own server parameter needs no change. The cause
was the grammar alone.

**Generation never hit its budget.** 65 to 126 tokens generated against `n_predict` 192, so
the grammar closed the array of its own accord every time. Nothing was cut off, which is the
other half of why `0 answered no identifier`.

**The server slowed by roughly three times during the run**, which is why the two-event probe
was so misleading:

    early tasks    8.4 s total, generation near 23 tokens per second
    late tasks     12 to 19 s total, generation 12 to 15 tokens per second
    two tasks      232 and 261 ms per token, 4.3 and 3.8 tokens per second

Prompt evaluation drifted the same way, 160 tokens per second at the start and 71 to 95 later.
That is the serving laptop under sustained eight-thread load, not the Pi, which never moved
off 43.3 C. **The 5.2 s per event the probe measured was the cold-machine rate and the 13.0 s
average is the real one.** A rate measured over two events at the start of a run is not a rate.

**And the 20 second default timeout was genuinely too short, not merely unlucky.** The log
shows the client giving up on one task while the server was still generating at 1.48 tokens
per second, and the server then cancelling it:

    n_gen = 105, tg = 9.12 t/s, tg_3s = 1.48 t/s
    W srv stop: cancel task, id_task = 350

At that rate the remaining tokens could not have arrived inside the budget. The `--timeout
120` used for the real run was not padding.

## Deviations

1. **A Windows portproxy and a firewall rule** stand between the bench LAN and the server,
   because WSL is NAT'd at `172.17.188.105` and the chapter assumes the Pi can reach the WSL
   port directly. The rule is scoped to `10.42.0.0/24`. The chapter's own note anticipated
   this as the commonest failure.
2. **`python3`, not the chapter's `python`.** Board A has 3.13.5 under `python3`.
3. **The step-5 `pip install -e` was skipped.** `load_shared()` falls back to the in-tree copy
   by design, and Pi OS trixie would refuse the install outside a venv anyway.
4. **The snapshot was regenerated on the board** before the run, since board A's checkout is
   four commits behind. It produced byte-identical output to the authoring laptop's.
5. **No deviation on the engine or the model.** Both match the volume's pin exactly.

## What is established and what is not

    established   clause one FAILS: 15 of 50, 30.0 percent, against a bar of 40 of 50
    established   the ordered triple and the set are both 0 of 50
    established   the CI stub scores 100.0 percent where this model scores 30.0
    established   every answer was well formed; 0 of 50 carried no identifier
    established   the model quotes its chosen row's figures correctly
    established   no shown-position bias: 24 percent from the top five against 25 uniform
    established   a low-ordinal bias: rank 1 mean 3.56 against 10.5 uniform
    established   the client is thermally flat: 0x0 and 43.3 C at both ends
    established   the prompt is 642 to 754 tokens and nothing was truncated at -c 1024,
                  so the context hypothesis I raised at the 400 is disproved
    established   generation ran 65 to 126 tokens against a budget of 192
    established   the server slowed about threefold across the run, 23 to 12 tokens per
                  second, so the two-event probe's 5.2 s was not a rate
    established   removing the grammar does not move the first pick: two unconstrained
                  samples at temperature 0.8 answered e04 first, as the run did
    established   the grammar supplied the "project" key; unconstrained the model used
                  "name" once and dropped the key entirely once
    NOT shown     whether a larger model ranks correctly, no other model was run
    NOT shown     whether the failure survives a restated prompt, the prompt was not varied
    NOT shown     the unconstrained distribution over fifty presentations; the control is
                  two samples on one presentation and both were truncated by the context
    NOT shown     anything about e13, whose grammar carries the same defect unfixed

## Gates

    line_check             0 problems
    no_trace_scan          0 hits
    no_authoring_files     0 refused
    grammar_pairs          19 grammars paired, 0 problems
    pytest                 46 passed, e20's own suite
