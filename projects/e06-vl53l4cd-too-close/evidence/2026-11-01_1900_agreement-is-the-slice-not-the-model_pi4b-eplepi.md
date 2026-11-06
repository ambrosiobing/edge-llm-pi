# e06: the agreement figure measures the slice, not the model, Sunday 1 November 2026, 19:00

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, hostname eplepi, revision d03114
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, the chapter's model
    engine     llama.cpp v0.6.0, the pinned tag, real engine build at build/
    replay     /home/bing/e06_descent_50.csv, 50 events, rows 110 to 159 of the project's
               own approaches.csv, chosen to span the descent from 491 mm to 242 mm
    settings   --threads 4, --n-predict 64, e06's own grammar and prompt, unmodified
    register   0x80000 BEFORE and 0x80000 after; 41.8 C before, 78.8 C after
    tree       45aff6e

**THIS IS A DIAGNOSTIC AND NOT AN ACCEPTANCE RUN, AND IT COULD NOT HAVE BEEN ONE.** e06's
acceptance requires `get_throttled` reading `0x0` before and after and discards any run with
a bit set. Bit 19 was **already set before this run started**, inherited from an earlier run
the same evening and clearable only by rebooting. So the run is discarded by the clause's own
wording, and the figures below stand as diagnosis rather than as a claim against clause one.

## WHY A NON-DEGENERATE SLICE WAS NEEDED, AND WHAT THE OLD ONES WERE WORTH

Three runs earlier the same evening used `--max-events 20`, which takes the first twenty rows
of `approaches.csv`. **Every one of those twenty events is CLEAR**: the first range below 300
mm is at data row 148, and the opening rows sit near 900 mm with the sensor's own noise.

So those three runs scored a one-class task, and on a one-class task a constant wins:

    all-CLEAR slice, 20 events     a constant CLEAR predictor     20 of 20   100 percent
                                   every model configuration       0 of 20     0 percent

That is three runs whose headline figure of 0.0 percent was a property of the slice. This
slice spans the descent instead, and the rule exercised all three of its names:

    rule         events
    APPROACHING     27
    CLEAR           12
    TOO_CLOSE       11

## THE MEASUREMENT

    e06: 50 events, 50 model answers, 0 model errors, 0 actuation errors,
    mean prompt 2288.9 ms, mean generate 3828.6 ms, rss 353060 kB

    agreement 27 of 50, 54.0 percent

    rule \ model      APPROACHING    truncated
    APPROACHING            27            0
    CLEAR                   9            3
    TOO_CLOSE              11            0

**The model answered `APPROACHING` on all fifty rows.** The three rows the tool could not
parse were truncated mid-sentence and their visible text reads `"decision": "APPROACHING"`
as well, so the decision field varied not once across fifty events spanning 491 mm to 242 mm.

### And the figure is exactly what a constant scores

    a constant APPROACHING     27 of 50     54.0 percent
    THE MODEL                  27 of 50     54.0 percent
    a constant CLEAR           12 of 50     24.0 percent
    a constant TOO_CLOSE       11 of 50     22.0 percent

**The model does not beat the best constant. It equals it, because it is one.** Its 54.0
percent is the frequency of `APPROACHING` in this slice and nothing else.

## THE COMPARISON THAT MAKES THE POINT UNARGUABLE

    slice                      model output      agreement
    20 events, all CLEAR       APPROACHING x20    0 of 20     0.0 percent
    50 events, the descent     APPROACHING x50   27 of 50    54.0 percent

Same model, same prompt, same grammar, same binary, same board, **same constant output**. The
figure moved by 54 points because the slice's class balance moved. A slice that was ninety
percent APPROACHING would have reported ninety percent agreement for a model that answers one
word.

**So e06's first acceptance clause, as the chapter specifies it, measures the replay's class
distribution rather than the model.** The chapter asks for an agreement table and publishing
one is honest only alongside the constant baselines above, because without them 54.0 percent
reads as partial competence and it is not.

This is the second project today in which a published acceptance figure turns out to be
saturated by a constant. e20 was the first, measured on Thursday 29 October 2026 at 30.0
percent against a constant's 100. The defect is not e20's and not e06's; it is what happens
when an agreement rate is published without the rate a constant achieves on the same rows.

## WHAT THE PROSE FIELD DID, FOR COMPLETENESS

Unchanged from the record of 18:10: the `beam` values still open with "short sentence saying
what is probably in front of the sensor" and then append input ranges, for example
"...of the sensor, 472, 481, 487, 481, 487, ...". The echo is not a separate problem from the
constant decision; both are the same model continuing text rather than answering.

## A TOOL LIMIT CONFIRMED, AND ITS CONSEQUENCE

`agreement.py`'s confusion table degraded into three extra columns whose headers are JSON
fragments, because its `model_decision` falls back to the raw text when `json.loads` fails
and each distinct truncated reply becomes its own category. With 3 truncated rows the table
is still readable; the run of 18:10 had 14 and its table was not.

**And its vocabulary check misreported the earlier runs.** The tool takes the set of valid
decisions from the rule column of the log it is given, which its own comment documents
together with the limit: "a log whose rule never used one of its names, where a model
answering that name would be reported as malformed ... a log short enough for the risk to be
real is short enough to read by eye". The three 20-event runs are exactly that case, and
their "not a decision of the rule: 20" meant only that the rule never said `APPROACHING` or
`TOO_CLOSE` in those twenty rows. The grammar worked in every arm. **I pasted the tool's
number instead of reading the log by eye, which is what the comment asks for.** On this
slice the figure is 3 and means what it says.

## What is established and what is not

    established   the first 20 rows of approaches.csv are all CLEAR, so every 20-event
                  run of this project scores a one-class task
    established   on a 50-event slice spanning the descent the rule emits all three names,
                  27 APPROACHING, 12 CLEAR, 11 TOO_CLOSE
    established   SmolLM2-360M answered APPROACHING on 50 of 50 events
    established   its agreement is 27 of 50, 54.0 percent, which is EXACTLY the score of a
                  constant APPROACHING predictor on the same rows
    established   the same constant output scored 0.0 percent on the all-CLEAR slice, so
                  the figure tracks the slice and not the model
    established   bit 19 was set before the run, so clause one could not have been met by
                  it under any outcome
    NOT shown     what a larger model does on THIS slice; qwen2.5-0.5b has only been run
                  on the all-CLEAR one, where it answered TOO_CLOSE 20 of 20
    NOT a claim   against clause one, which needs a clean register and therefore a reboot
                  and, since a non-degenerate slice is about 50 events against this board's
                  32 before the soft limit, cooling as well

## REPEATED ON THURSDAY 6 NOVEMBER 2026 FROM A CLEAN REGISTER, AND IT STILL TRIPPED

The same fifty events, the same model, binary, grammar and prompt, on a board freshly
booted and idle for five days.

    start 35.5 C, register 0x0        the coldest start any run this week has had
    mean prompt 2263.3 ms, mean generate 3792.0 ms
    rule counts 27 APPROACHING, 12 CLEAR, 11 TOO_CLOSE
    agreement 27 of 50, 54.0 percent  IDENTICAL to the run above
    always APPROACHING 54.0 percent   printed by agreement.py itself
    register 0x80000 AFTER, 78.4 C

**The agreement figure is confirmed by repetition**, five days apart, from registers in
opposite states. Determinism holds, so the constant equality is a property of the model and
not of one sitting.

**And the attempt to get a clean register failed, which corrects a standing bench figure.**
This run was predicted to fit: about 303 s of load against "a thermal limit after 367 s"
carried by e06's pre-flight record of Thursday 29 October 2026 and requoted in e20's record
of Thursday 29 October 2026. It did not fit. That 367 s line names no load, no thread count
and no model, and whatever produced it was not this one.

    measured Thursday 6 November 2026   trips inside about 300 s from 35.5 C
    measured Sunday 1 November 2026     onset at event 33 of 40, about 190 s, from 41.8 C
    the standing claim                  367 s, source unstated

**So clause one cannot be satisfied on this board even for a fifty-event replay, from any
starting temperature.** Cooling is required by measurement rather than by extrapolation, and
that is a firmer statement than the one above it. The 367 s figure should not be requoted
until something re-measures it and says what was running.
