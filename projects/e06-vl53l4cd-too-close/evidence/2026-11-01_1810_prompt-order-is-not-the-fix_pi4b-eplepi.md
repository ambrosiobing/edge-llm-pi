# e06: the model echoes whatever is last, so prompt order is not the fix, Sunday 1 November 2026, 18:10

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, hostname eplepi, revision d03114
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes
    engine     llama.cpp v0.6.0, the pinned tag, real engine build at build/
    source     replay, 20 events, --threads 4, --n-predict 64, e06's own grammar
    register   0x0 before and after every run
    tree       4b8f032
    variable   ONE: the sentence order of prompt.txt. Both files are 448 bytes and the
               same sentences; "In beam" sits at character 373 of 447 in the original
               and 184 of 447 in the variant, verified by diffing the sorted sentences

**This closes no clause and refutes a remedy I proposed the same afternoon.** The record of
Friday 30 October 2026 found 40 of 40 answers putting the prompt's instruction into `beam`,
and the survey of 17:40 found 14 of 15 free-text slots in this volume admitting their own
instruction. I proposed reordering each prompt so the free-text instruction is not last,
called it "a prompt edit in fourteen files", and said the gate could then enter CI.
**Measured, that is wrong.**

## THE CONTROLLED COMPARISON

    arm A   original prompt, beam instruction LAST
    arm B   reordered prompt, beam instruction third of seven, decision rules LAST

Same board, same model, same binary, same grammar, same replay file, same thread count, same
token budget, same twenty events. Only the sentence order differs.

    arm      echoes the beam instruction    mean generate
    A        20 of 20                       3642.3 ms
    B         3 of 20                       7158.3 ms

The echo count fell by a factor of six, and **nothing improved.** Arm B's text:

    3 of 20    "short sentence saying what is probably in front of the sensor."
    2 of 20    "TOO CLOSE APPROACHING CLEAR CLEAR CLEAR CLEAR CLEAR CLEAR CLEAR..."
    ~12 of 20  "range 902, range 902, range 896, range 900, range 901, range 901, ..."
    1 of 20    "APPROACHING 902, 902, 896, 900, 901, 901, 898, 898  , 898,"

**The two rows listing TOO CLOSE, APPROACHING and CLEAR are the decision literals from the
rules section, which the reordering moved to the end.** The dozen reading back `range 902,
range 902, ...` are the event's own input.

### So the mechanism is recency, and the remedy is not reordering

**The model echoes whatever is last.** Move the instruction and it echoes the next thing.
Zero of twenty answers in either arm say what is in front of the sensor.

That reframes the defect. It is not a prompt-layout problem to be tidied. **SmolLM2-360M is
not following the instruction on this task at all; it is continuing the text, and the grammar
pours that continuation into whichever slot is free.** A grammar that pins only a shape
cannot refuse a continuation, which is what the survey of 17:40 counted across fifteen slots.

## A FREE CONTROL: THE SAMPLER IS DETERMINISTIC

Arm A was run twice, from 32.1 C and from 41.8 C, and produced **byte-identical** distinct
value tables: ten, seven, one, one, one, in the same proportions, with mean generate 3642.3
and 3646.4 ms.

So twenty events are twenty distinct inputs rather than twenty draws, **repeating an arm adds
no information**, and the starting temperature does not reach the output. That was not asked
for and it is worth more than another repeat would have been.

## FOURTEEN OF TWENTY ARM B ANSWERS NEVER CLOSED, AND THE SUMMARY CALLS THEM ANSWERS

    arm A  n_generated   9x28, 1x29, 7x30, 1x33, 1x43, 1x48    none near the budget
    arm B  n_generated   3x29, 1x48, 1x53, 1x60, 14x64         14 AT the budget of 64

A reply that stops at exactly the budget stopped because it ran out, not because its object
closed. Arm B's own summary line reads:

    e06: 20 events, 20 model answers, 0 model errors, 0 actuation errors, mean prompt
    2021.6 ms, mean generate 7158.3 ms, ... source replay

**Twenty answers and zero errors, with fourteen truncated objects among them.** e02 already
counts this and publishes it: its record carries "objects that never closed, published 949,
counted 949" out of a thousand. **e06's summary line has no such column**, so a run can be
three quarters truncated and report cleanly.

### FIXED AND VERIFIED ON THURSDAY 6 NOVEMBER 2026

The shared main now counts answers whose `n_generated` reached `n_predict` and prints the
total in the summary line. Not a JSON closure test: that main serves every project and e19's
answer is an SMS body with no braces, so a closure test would have to know each grammar's
shape, while reaching the budget is shape-agnostic and is exactly what separates a finished
answer from a truncated one.

**The prediction was written down before the run and the run matched it.** These arm B
conditions were repeated on board A at 4fd0527, from 49.6 C with the register `0x0` at both
ends:

    e06: 20 events, 20 model answers, 0 model errors, 14 stopped at the budget, ...
    n_generated histogram:  3x29, 1x48, 1x53, 1x60, 14x64

**Fourteen from the new counter and fourteen from an awk over a column it does not touch.**
The change compiled with no `-Wformat` warning in a full stub tree on skyhorizon where all
twenty tests passed, which is the breadth it needed because every project compiles that file.

So a run of this project can no longer report cleanly while three quarters of its answers
were cut off. Ten records in this volume quote the old summary line and stay as written,
being dated records of what it said then. The three arm B rows that did echo are the three
at 29 tokens, which are the ones that finished.

This is a defect in the reporting rather than in the loop, and it is the reason arm B looked
at first like a sixfold improvement: most of its answers were cut off before they could be
scored.

## THE LATENCY CONSEQUENCE, WHICH DECIDES AGAINST THE REORDERING ON ITS OWN

Generation doubled under the only change made, 3642.3 to 7158.3 ms, because the model wrote
until the budget stopped it.

    e06 steady-state latency, arm A ordering     5.809 s   MEASURED Friday 30 October 2026
    the same with arm B's generation             about 9.2 s
    ceiling, 600/v > 0.4 + L                     96.6 mm/s falls to about 62 mm/s
    sensor floor at two standard deviations      63.8 mm/s

**Board A's two-standard-deviation window closes.** The reordering that was going to be a
cheap fix for fourteen slots would, on this board, cost e06 the usable speed window that the
record of Friday 30 October 2026 established. So even if the echo had been cured, the price
would have been the clause the latency serves.

## What is established and what is not

    established   the model echoes whatever is LAST in the prompt: the beam instruction
                  when it is last, the decision literals and the input ranges when they are
    established   reordering the prompt is NOT a remedy; it changes what is echoed
    established   0 of 20 answers in either arm describe what is in front of the sensor
    established   the sampler is deterministic: arm A twice, byte-identical, from 32.1 C
                  and 41.8 C
    established   14 of 20 arm B replies stopped at the 64-token budget
    established   e06's summary line counts a truncated object as an answer and reports
                  0 model errors, where e02 publishes an "objects that never closed" count
    established   arm B's ordering would roughly double e06's latency and empty its
                  two-sd speed window on board A
    REFUTED       my own proposal of 17:40, that a prompt edit in fourteen files would fix
                  the free-text slots and let the gate enter CI
    NOT shown     whether a larger model follows the instruction here; every figure above
                  belongs to SmolLM2-360M at 4 threads and nothing else was tried
    NOT shown     whether a narrower grammar or a downstream checker would help, which is
                  now the open question rather than the prompt's layout
