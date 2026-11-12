# e19: a two-entry lookup table reproduces all four notes, which closes the paraphrase gap,
# Thursday 12 November 2026, 14:24

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model
    subject    projects/e19-cellular-alert/evidence/e19_run.csv, 50 rows of which 4 carry
               a note, and every other ledger in the tree for comparison
    method     mask the numbers in each free-text answer, then ask what share of rows a
               table from the RULE's verdict to one fixed shape would reproduce
    tool       tools/answer_shapes.py, extended here
    tree       24569e3 plus this change

**THE GAP THIS CLOSES WAS NAMED AT 11:12 TODAY AND CALLED UNSOLVED AT THE END OF THE
AFTERNOON.** Project 19's note is a paraphrase of its instruction, not a copy, so
`tools/prompt_echo.py` finds nothing in it and is right to: the longest run it shares with any
prompt sentence is three words out of thirty-four. No span comparison can see it. This record
measures it a different way.

## THE FOUR NOTES, AND THE TABLE THAT PRODUCES THEM

    rows   share   shape
       3   75.0%   ALERT TAP peak # g at t+#:#:#. Check the device settings for the
                   maximum acceleration limit.
       1   25.0%   ALERT FREEFALL peak # g at t+#:#:#. Check the message type, peak
                   value, and time.

    a table from the rule's verdict to one fixed shape reproduces 4 of 4 rows,
    100.0 percent, over 2 verdict classes

The three TAP events share one sentence and the single FREEFALL event has another. **So a
two-entry table, keyed on the verdict the rule already computed, reproduces every note in the
run.** The model's contribution over that table is nothing, and this is a measurement rather
than a reading: the record of Sunday 1 November 2026 called one note "the instruction", which
was a fair description of its meaning and not a textual fact, and that is exactly the kind of
claim this figure replaces.

## WHY THE RULE'S VERDICT IS THE RIGHT KEY

The rule column is what exists without a model. A constant baseline answers "what would one
fixed literal score", and this answers the same question one step up: what would one fixed
sentence per verdict score. It is the free-text counterpart of the baseline
`tools/agreement.py` has printed beside every rate since Sunday 1 November 2026, and it is
keyed on the rule rather than on the model's own decision so that a model which gets the
verdict wrong cannot make its prose look less predictable than it is.

## THE SAME FIGURE ACROSS THE VOLUME

    ledger                     rows  shapes   table   classes
    e19_run.csv                   4       2  100.0%         2
    e20_run.csv                  50       1  100.0%         1
    e03_run.csv                 200       4   93.5%         3
    e04_replay.csv               44       7   81.8%         3
    e06_descent_sampled.csv      47       2   76.6%         3
    e06_qwen05b.csv              20       2   65.0%         1
    e06_armA.csv                 20       5   50.0%         1

**e06 arm A at 50 per cent is the figure's own blind spot and names why there are three.** All
twenty of those answers are the same eleven-word instruction echoed verbatim; they split into
five shapes only because different rubbish follows it, ten with nothing, seven with ", e." and
three with range numbers. The table figure therefore understates that run badly, and
`prompt_echo.py` reports it as 20 of 20. Each of the three instruments is blind where another
sees:

    prompt_echo.py        the instruction echoed verbatim. e06 arm A, 20 of 20
    the shape count       a template the prompt did not supply. e03, 187 of 200
    the lookup baseline   a sentence predictable from the verdict. e19 and e20, 100 per cent

## A TEST ASSERTION WAS WRONG ABOUT THIS PROJECT AND IS CORRECTED

`field_values` treated any cell that is not a JSON object as a broken answer, and a test
required that. **Project 19's answer is an SMS body with no braces**, so on its ledger the tool
reported "with a value 0, empty 46, model cell unparsable 4" and printed nothing at all, over
a run of four perfectly good notes. The rule is now narrower and keeps the case it was written
for: a cell that STARTS with a brace or bracket and does not parse is an answer the budget cut
off, which is e02's 949 of 1000 and e04's 136 of 180, and a cell with no brackets is the free
text itself. Both cases now have their own test.

## WHAT THIS DOES NOT ESTABLISH

    established   a 2-entry table keyed on the rule's verdict reproduces all 4 e19 notes
    established   the same for all 50 e20 rows, over 1 class
    established   93.5 per cent for e03, 81.8 for e04, and 50.0 for e06 arm A
    established   the figure understates a run whose single echo carries varying rubbish,
                  which is the case prompt_echo.py was built for
    corrected     field_values read a brace-free cell as broken, which hid this finding
    NOT shown     anything about a model under a changed prompt or grammar
    NOT a verdict four rows is four rows. e19's run produced 4 notes in 50 events, and a
                  100 per cent table over 4 rows is weak evidence by itself; it is strong
                  only alongside the 3-of-4 identical sentence already published
    NOT IN CI     it needs a ledger, and a ledger needs a run
