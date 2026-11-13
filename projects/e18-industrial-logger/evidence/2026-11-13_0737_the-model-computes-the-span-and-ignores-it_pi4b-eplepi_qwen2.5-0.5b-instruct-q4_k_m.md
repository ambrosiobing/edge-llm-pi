# e18: the model computes the deciding span exactly and its verdict is independent of it,
# Friday 13 November 2026, 07:37

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, card eplepi
    model      qwen2.5-0.5b-instruct-q4_k_m.gguf, 491400032 bytes on disk,
               462.96 MiB loaded, and llama.cpp reads 630.17 M parameters
    engine     the C daemon of this project over its Unix socket, ABI 1000,
               libllama.so of the pinned submodule, -c 512, four threads
    budget     --n-predict 128, NOT the 64 the manifest carried; see below
    source     replay/registers_600.csv, 600 frames
    log        evidence/e18_replay.csv
    duration   about 2 hours 40 minutes at 16.1 s per event
    tree       5953676 plus this change

**THIS IS THE FIRST TIME e18 HAS RUN WITH A REAL MODEL ON HARDWARE**, and it closes
acceptance clause two, which had been open since the chapter was written. The toolchain was
the only thing missing: cargo 1.85.1 and rustc 1.85.1 from Debian trixie, well above this
manifest's floor of 1.75, installed on the board the same evening.

## THE SUMMARY LINE

    e18: 600 frames, 0 refused, 0 sequence gaps, 600 answers, 0 engine errors,
         600 over the 2.0 s period, mean call 16134.0 ms, backend socket, abi 1000

## THE AGREEMENT TABLE, AND THE MODEL IS FAR WORSE THAN A CONSTANT

                    model DRIFT   model NOMINAL
    rule DRIFT,  58           1              57
    rule NOMINAL, 542       191             351

    agreement                 352 of 600, 58.7 per cent
    always NOMINAL            542 of 600, 90.3 per cent
    always DRIFT               58 of 600,  9.7 per cent

**58.7 against a constant's 90.3, so the model is 31.6 points below doing nothing.** This is
not the degenerate case e06 and e20 produced, where a model answers one literal and ties its
own baseline. It is worse: this model uses both literals and is wrong with both. It says DRIFT
192 times and is right once.

The rule's two DRIFT episodes are 29 frames each, at rows 181 to 209 and 401 to 429. The model
finds **one** frame of the first, row 190, and **none** of the second. Where the rule has two
long episodes, the model produces **103 DRIFT runs**, the longest 14 frames and the shortest
one, scattered across the file, and not one of them overlaps the second episode.

## AND THE REASON IS NOT A MISSING NUMBER, WHICH IS THE FINDING

The rule is arithmetic: DRIFT when the span of register 0 across the last 30 frames exceeds
100 counts. **481 of the 600 answers state that span in words**, in the form
`span N counts over 30 frames`, and recomputing every one of them from the ledger's own `raw`
column:

    rows naming a span              481 of 600
    exactly right when recomputed   481   100.0 per cent, not one off by a count

So the model computes a 30-frame sliding span without error, four hundred and eighty-one times
in a row. **And the verdict is independent of it:**

    of the 481 rows naming a span      model DRIFT   model NOMINAL
      the span it named is > 100                 0              57
      the span it named is <= 100               73             351

**Not once in the 57 rows where it writes a span above 100 does it answer DRIFT.** Zero of 57.
And in 73 rows where it writes a span at or below 100 it answers DRIFT anyway. Three rows read
in full:

    row 405   register 0 level 426 counts, span 175 counts over 30 frames    NOMINAL
    row 411   register 0 level 423 counts, span 178 counts over 30 frames    NOMINAL
    row 421   register 0 level 427 counts, span 179 counts over 30 frames    NOMINAL

Recomputed: 175, 178, 179. The rule is a comparison of that number with 100 and the model
writes the number, then contradicts it.

**The one row it gets right does not name a span at all.** Row 190 reads "Register 0 span is
above 100 counts, which is a tenth of its range", which is a paraphrase of the rule rather
than a report of the input, and it answers DRIFT. One row of 600 is the rule restated; 398 are
a template of the input.

## THE FREE TEXT, BY THE THREE MEASURES ADDED YESTERDAY

    values 600, distinct 414, shapes 6
    commonest shape, 398 rows, 66.3 per cent:
      "register # level # counts, span # counts over # frames, temperature # ..."

So two thirds of the answers are one sentence form with the event's own measurements
substituted in, which is project 3's defect in a project that also computes a derived
quantity correctly inside that template. The distinct count of 414 would have read as variety.

## THE CLAUSE THE CHAPTER CALLS NOT MEASURED IS NOW MEASURED, AND IT FAILS BADLY

    600 of 600 events overrun the 2.0 s period
    mean call 16134.0 ms, minimum 9995.6, maximum 23374.9

The service is slower than its own period by a factor of eight, on every single frame. The
chapter's table carries "overruns past the 2 s period: not measured; replay has no real time
pressure, so this row needs the live run or a timed replay". The counter measured it anyway,
from the call duration against the nominal period, and the answer is every row.

## TWO THINGS THE SOCKET PATH LOSES, AND ONE PREDICTION THAT HELD

**`prompt_ms` and `n_generated` are zero on all 600 rows**, with `generate_ms` carrying the
whole call. The daemon knows both figures and the ABI does not carry them back, so this
project's ledger lacks two columns e04's has. That is a gap in the data path and not a model
result, and it means the sufficiency of the budget rests on the zero unclosed objects rather
than on any token count.

**The budget was raised from the manifest's 64 to 128 before the run, and the prediction
held.** e18's `summary` rule is `[A-Za-z0-9 ,.%]{9,159}` plus a full stop, character for
character the same class as e04's, and e04 measured on this board on Thursday 12 November 2026
that such an answer needs 95 to 97 tokens. This run reports **0 rows with no parseable
object**, where e04 at 64 tokens had 136 of 180. The manifest's 64 would have produced the
same defect, and project.toml is corrected in this change.

## WHAT THIS DOES NOT ESTABLISH

    established   clause two is measured: 352 of 600, 58.7 per cent, against a constant's
                  90.3, so the model is 31.6 points below answering NOMINAL always
    established   it finds 1 of 58 DRIFT frames and none of the second episode
    established   it computes the deciding span exactly in 481 of 481 rows naming one
    established   its verdict is independent of that span: 0 of 57 rows naming a span
                  above 100 answer DRIFT
    established   the one correct DRIFT row paraphrases the rule instead of reporting input
    established   600 of 600 events overrun the 2.0 s period at 16134 ms mean
    established   the free text is 6 shapes, 398 rows sharing one
    established   the budget of 128 closed every object, against e04's 136 of 180 at 64
    established   cargo 1.85.1 on the board, C daemon built against libllama.so of the
                  submodule, daemon and service both run
    NOT shown     clause one, the 1000-frame RS485 echo, which needs the WS-28164 on card B
    NOT shown     clause three, the per-call cost of the boundary, which is a separate run
    NOT a latency  nothing here, the board having thrown its three sticky throttle bits
                  overnight, 0xe0000 at 54.0 C afterwards with no under-voltage bit
    NOT MEASURABLE prompt time and tokens per call on this path, the ABI not carrying them
    NOT EXPLAINED why a model that computes the span ignores it. That is the question this
                  run leaves, and it is a model question, not a configuration one
