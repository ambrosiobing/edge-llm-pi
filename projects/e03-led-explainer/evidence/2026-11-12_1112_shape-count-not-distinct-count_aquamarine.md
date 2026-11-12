# e03: 191 distinct answers are four shapes, and 187 of 200 are one of them,
# Thursday 12 November 2026, 11:12

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model
    subject    every ledger under projects/*/evidence/*.csv that a past run wrote, read
               for the free-text field each project's grammar names
    method     mask every number in an answer and count the sentence forms that remain
    tool       tools/answer_shapes.py, added here, with eight tests
    tree       ae5956f plus this change

**NO RUN WAS TAKEN.** Every input is a file already in the repository, the oldest from
Tuesday 27 October 2026. What this establishes is a figure that was computable for a
fortnight and a correction to a generalisation published on Monday 9 November 2026.

## A COUNT OF DISTINCT ANSWERS HIDES THE DEFECT IT LOOKS LIKE IT WOULD FIND

Project 3's 200-row run on board A with the real engine:

    191 distinct values    which reads as a model writing something new nearly every row
      4 shapes             once the numbers are masked
    187 of 200             are the single shape "level #, slope # per second", 93.5 per cent

The answers are the event's own measurements re-spelled. The model supplied the sentence
form once. **The distinct count cannot see this, because the event hands it a fresh number
on every row and the number is the only thing that differs.** The same reading of the other
ledgers, all with the real engine:

    ledger                     values  distinct  shapes   commonest shape
    e03_run.csv                   200       191       4   187, level #, slope # per second
    e04_replay.csv                 44        44      10   22, SoC temperature # C, ...
    e06_armA.csv                   20         5       5   10, short sentence saying what ...
    e06_descent_sampled.csv        47         2       2   36, short sentence saying what ...
    e06_qwen05b.csv                20         2       2   13, The two newest ranges are ...

e04 is the same trap in milder form: 44 answers, every one distinct, ten shapes, the
commonest covering 22 of 44.

## THREE DEFECTS THAT I HAD BEEN TREATING AS ONE

All three were already in these ledgers and they need separate remedies:

    the instruction, verbatim   e06, 20 of 20 rows answered the prompt's own last clause
    the event, re-spelled       e03, 187 of 200 rows, one shape, numbers from the event
    an invented constant        e19, 3 of 4 notes were one identical sentence present in
                                neither the prompt nor the event

A checker that refuses the project's own instruction catches the first and neither of the
others, which matters because that checker is the remedy `tools/free_text_slots.py` names in
its own docstring. The shape count catches the first and the second, and reports the third as
a count of one.

## THE ECHO DETECTOR WAS BUILT, VALIDATED AND IS NOT SHIPPED

A prototype asked whether an answer carries a contiguous word run of a prompt sentence. It
reproduces the measured counts exactly once the coverage is measured the right way round:

    e06 arm A, instruction last    20 of 20 echo the instruction      matches the record
    e06 arm B, instruction third    0 of 20 echo the instruction      matches the record
    e06 arm B                       3 of 20 echo some other sentence  the rules section

**The first version measured coverage against the answer and found 18 of 20.** The two it
missed carried the whole eleven-word instruction and then appended range numbers, which
lengthened the answer and pushed the ratio under the threshold. An echo with rubbish after it
is still an echo, so coverage belongs against the prompt sentence. Measured that way the
honest case stays out: a real answer such as "a wall is in front of the sensor" shares a
five-word run with a fourteen-word instruction and covers 36 per cent of it.

**It is not shipped because of e19, where it finds nothing and is right to.** That note reads
"Check the message type, peak value, and time." and its longest run shared with any prompt
sentence is three words out of thirty-four. The model paraphrased rather than copied. The
record of Sunday 1 November 2026 calls that note "the instruction", which is a fair reading
of its meaning and not a textual fact, and **no string comparison can find it.** A verbatim
echo check would therefore close e06's case and leave e19's open, which is worth doing and
worth saying plainly rather than shipping as a general remedy.

## WHAT I ALMOST REPORTED, AND THE COLUMN THAT STOPPED IT

The first pass over the ledgers flagged `e06_repeat_200mm.csv` and `e06_repeat_304mm.csv`:
100 rows each, one distinct answer, a single full stop. Read as a model result that is a
degenerate run behind a published repeatability figure. **The `engine` column says `stub`,
with `generate_ms` and `n_generated` both zero.** Those runs measure the sensor's
repeatability, which needs no model, and the full stop is the stub's documented expansion of
the grammar. The tool now prints that note itself whenever a ledger's engine is the stub.

## THE GENERALISATION OF MONDAY 9 NOVEMBER 2026 IS WITHDRAWN AS WRITTEN

That record reads: "a 0.5B model on this volume's tasks has answered with one constant
literal in two projects out of two measured". **A third project was measured and contradicts
it literally**: e03 answered with 191 distinct strings. The spirit survives, since those 191
are one sentence form, but the claim as written was a generalisation from two cases while a
third sat in the tree, and the figure that reconciles them is the shape count rather than the
distinct count. Corrected in that record and in the journal.

## WHAT THIS DOES NOT ESTABLISH

    established   e03's 200-row run holds 4 shapes and 187 rows of one of them
    established   e04's 44 answers hold 10 shapes, the commonest covering 22
    established   a verbatim echo detector reproduces e06's measured 20 of 20 and 0 of 20
    established   e19's note is a paraphrase, so no string comparison can flag it
    established   the three defect kinds are separate and need separate remedies
    withdrawn     the claim that two of two measured projects answered one constant literal
    NOT shown     anything about any model's behaviour under a narrower grammar
    NOT SHIPPED   the echo detector, for the reason above; only the shape count is here
    NOT IN CI     answer_shapes.py needs a ledger, and a ledger exists only after a run
