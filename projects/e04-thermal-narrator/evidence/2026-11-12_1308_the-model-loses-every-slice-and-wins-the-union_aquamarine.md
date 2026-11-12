# e04: the model loses on both slices and wins on their union,
# Thursday 12 November 2026, 13:08

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model
    subject    projects/e04-thermal-narrator/evidence/e04_replay.csv, 180 rows, the Pi 4B
               run of Monday 26 October 2026 at 22:55
    method     split the rows by whether the answer's JSON object closed, and compute the
               agreement and the best constant on each part and on the whole
    tree       7348873 plus this change

**THE REVERSAL BELOW IS WITHDRAWN AS AN ARTEFACT OF THE TOKEN CEILING, at 16:16 the same
day**, by
`2026-11-12_1616_the-budget-was-the-cause-and-nine-rows-loop_pi4b-eplepi_smollm2-360m-instruct-q4_k_m.md`.
A rerun on board A with the budget at 128 instead of 64 closed 127 of the 136 objects, so the
split that produced the reversal is 171 to 9 rather than 44 to 136, and the model then beats
its best constant on the 171 and on the whole file: 43.3 against 36.3, and 42.2 against 36.7.
The effect was real on these data and its cause was the configuration: the ceiling cut the
rows the rule called WARM or THROTTLED more often than the rows it called COOL, which split
the file into two halves with different class balances. **So a defect in the configuration
produced a statistical artefact that read as a finding about the model.** Everything below
stands as a description of these 180 rows at 64 tokens; what is withdrawn is reading it as a
property of the model.

**NO RUN WAS TAKEN FOR THIS RECORD.** The ledger has been in the repository since Monday
26 October 2026. What this establishes is that its published agreement figure is not robust
to the choice of rows, and that the chapter has carried the run as pending for seventeen
days.

## THE CHAPTER SAID THE RUN WAS PENDING

\src{sections/e04.tex} reads "On the bench the model column is not measured: the Pi 4B run
has not been taken", and four table rows read "bench: not measured, run pending on the Pi 4B".
The run is dated Monday 26 October 2026 at 22:55, on the Pi 4B Rev 1.4 with
SmolLM2-360M-Instruct-Q4_K_M and `llama-server b11429-d81235049`. **Its own record says twice
that a chapter row "read not measured until this run"**, so the correction was known to be
owed on the day and the chapter never heard about it.

## THREE FIGURES, AND THE MODEL LOSES ON EVERY SLICE IT WINS ON THE UNION

The first acceptance clause requires zero rows with no parseable object. **136 of 180, 75.6
per cent, never closed**, because 159 of 180 reached the 64-token ceiling. So every row has a
readable `decision` literal and three quarters of them have a truncated `summary`, and how
those rows are treated decides the figure:

    rows                              n     model agrees    best constant on those rows
    the object closed                44     31, 70.5%       always COOL, 79.5%
    the object was truncated        136     45, 33.1%       always WARM, 44.1%
    all rows, literal taken anyway  180     76, 42.2%       always WARM, 36.7%

**The model is beaten by a constant on both parts and beats one on the whole.** That is not a
paradox and it is not rounding: the two parts have different class balances. Measured:
the well-formed 44 are COOL 35, WARM 6, THROTTLED 3, so 79.5 per cent COOL; the truncated 136
are WARM 60, THROTTLED 48, COOL 28, so 44.1 per cent WARM and only 20.6 per cent COOL. The
long answers are the ones the rule called COOL and the truncated ones are where it called
WARM or THROTTLED, which is itself worth noting: the rows that overran the budget are not a
random quarter. So a model that tracks neither balance can finish ahead of the constant that
fits the union while losing to the two that fit the parts.

The record of Monday 26 October 2026 reports 76 and 42.2 per cent, which is the one of the
three on which the model is ahead. **That was not a choice to flatter it**, the whole-file
figure being the obvious one to quote, and it is exactly why a rate needs its baseline and its
row set named together. `tools/agreement.py` on the same file reports 17.2 per cent, counting
only the 44 well-formed rows as scored and the 136 as failures of the first clause, which it
has done deliberately since Saturday 17 October 2026 after e02 read zero while 949 rows of
1000 never closed.

**Neither figure is publishable as a result, because the first clause fails.** The chapter's
own rule is that this count must be zero or the grammar or `n_predict` failed.

## WHAT THE RUN DID ESTABLISH, WHICH IS WHAT THE CHAPTER WAS BUILT FOR

    mean round trip        7683.4 ms
    server prompt          2124.8 ms
    server generate        5555.1 ms
    HTTP cost                 3.5 ms, by subtraction, 0.046 per cent of the round trip

That clause PASSES and is the question this chapter exists to answer: a libcurl client
talking to `llama-server` over the loopback is free at this interval. It is a subtraction
within one route, so it survives the confound that spoils the route comparison, which is that
this chapter fixes four threads and a 64-token budget while e02's loop fixes two and 12, so
honouring "no other change" moves two variables at once.

## AND THE FREE TEXT IS A TEMPLATE, AS IT WAS IN PROJECT 3

Of the 44 answers whose object closed, all 44 are distinct and they hold **7 shapes**, 36 of
them, 81.8 per cent, being `SoC temperature # C, throttle bits #, ARM clock # MHz, previous
temper...`. Same finding as project 3 on Thursday 12 November 2026 and the same remedy, which
is to print the shape count beside the rate.

## WHAT THIS DOES NOT ESTABLISH

    established   the chapter carried a pending run for seventeen days
    established   136 of 180 objects never closed, 159 of 180 hit the 64-token ceiling
    established   agreement is 70.5, 33.1 and 42.2 per cent on the three row sets
    established   the best constant beats the model on both parts and loses on the union
    established   the HTTP cost is 3.5 ms of 7683.4 ms, which is the chapter's question
    established   the 44 well-formed summaries hold 7 shapes, 36 of them one shape
    withdrawn     the reversal, as an artefact of the ceiling; see the record of 16:16
    answered      what the model would do with a budget that lets the object close: the
                  run was taken the same day at 128 tokens, closing 127 of 136 objects,
                  changing not one decision, and leaving nine rows looping at the ceiling
    NOT shown     the route comparison, which cannot be made as specified
    NOT a defect  the record's 42.2 per cent, which is the whole-file figure honestly
                  computed; what was missing is the row set and the baseline beside it
