# e04: the budget closed 127 of 136 objects, nine rows loop instead, and the free text
# degenerates, Thursday 12 November 2026, 16:16

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72
    card       eplepi, Raspberry Pi OS, Debian 13 trixie, user bing
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf
    engine     llama-server b11429-d81235049, four threads, -c 512
    variant    catalogue D, a libcurl client over the loopback
    source     replay/benchmark_180.csv, 180 events, the same file as before
    change     ONE variable: --n-predict 128 instead of the default 64
    logs       evidence/e04_replay_np128.csv, evidence/e04_replay_rtt_np128.csv
    duration   about 44 minutes
    tree       0fd8560

This answers the question the record of 13:08 today left open, which asked what the model
would do with a budget that lets the object close. The old logs were not overwritten, so both
runs sit side by side.

## THE SUMMARY LINE

    e04: 180 events, 180 model answers, 0 model errors, 0 vcgencmd errors,
         mean round trip 14013.9 ms, mean server prompt 2582.6 ms,
         mean server generate 11415.5 ms, mean HTTP cost 15.8 ms,
         agree 76, disagree 104 (of which 16 on a row where the rule changed),
         no literal 0, objects unclosed 9

## THE FIRST CLAUSE IS NEARLY MET, AND THE REMAINING NINE ARE A DIFFERENT DEFECT

    objects never closed   136 of 180 at 64 tokens   ->   9 of 180 at 128

**The budget was the cause for 127 of the 136**, which was the predicted outcome. The nine
that remain all stop at exactly 128 tokens, so they are at the ceiling again, and what they
write there says why no larger budget would help:

    row  49   79.7, COOL, 79.7, COOL, 79.7, COOL, 79.7, COOL
    row  54   80.2, 80.2, 80.2, 80.2, 80.2, 80.2, 80.2, 80
    row  76   0000000000000000000000000000000000000000000000
    row  91   79.6, 79.6, 79.6, 79.6, 79.6, 79.6, 79.6, 79.6
    row 154   79.7, 79.7, 79.7, 79.7, 79.7, 79.7, 79.7, 79.

All nine were also truncated at 64. **They are repetition loops, not long answers**, so
raising the budget again would buy more repetition. The clause is not reachable by budget
alone, and that is a model defect rather than a configuration one.

The typical requirement is now measured rather than estimated: 97 tokens on 64 rows and 95 on
41, which is why 64 fell short by about a third. The prediction from the old log's 2.12
characters per token was 94.

## THE MODEL IS DETERMINISTIC, WHICH EXPLAINS WHY THE AGREEMENT DID NOT MOVE

    agreement at 64 tokens    76 of 180
    agreement at 128 tokens   76 of 180, and the SAME 76 rows, not one differing
    confusion table           identical cell for cell
    answers                   on all 180 rows one answer is a prefix of the other;
                              44 are byte identical, being the ones already finished at 64

So doubling the budget changed no decision anywhere. The model writes the same tokens and
then continues, and the decision is the first field, so the truncation never touched it. This
also means the two runs are not independent measurements of agreement; they are one
measurement seen at two lengths.

## AND THE SIMPSON REVERSAL OF 13:08 TODAY IS WITHDRAWN AS AN ARTEFACT OF THE CEILING

That record found the model losing to a constant on both parts of the file and beating one on
their union, with the parts being the 44 well-formed and the 136 truncated rows. With the
ceiling raised the split is 171 to 9:

    rows                     n     model agrees    best constant on those rows
    the object closed      171     74, 43.3%       always COOL, 36.3%
    the object was open      9      2, 22.2%       always WARM, 55.6%
    all rows               180     76, 42.2%       always WARM, 36.7%

**The reversal is gone.** The model beats its best constant on the 171 and on the whole file.
The effect was real on those data and its cause was the budget: the ceiling split the rows
into two halves with very different class balances, 79.5 per cent COOL among the well-formed
and 20.6 among the truncated, because the rows that overran were mostly the rows the rule
called WARM or THROTTLED. **A defect in the configuration produced a statistical artefact that
looked like a finding about the model.** That is worth keeping as the lesson of the pair.

What the clause asks for is 95 per cent. 42.2 against a constant's 36.7 fails it by a wide
margin, and the model still never says THROTTLED once against the rule's 51.

## THE FREE TEXT DEGENERATES, AND ONLY 21 PER CENT HAS THE INTENDED FORM

Of the 171 answers whose object closed, 74 distinct values hold **33 shapes**:

    rows  share  shape
      54  31.6%  COOL, WARM, COOL, WARM, COOL, COOL, COOL, COOL, COOL, ...
      36  21.1%  SoC temperature # C, throttle bits #, ARM clock # MHz, previous
                 temperatures #, #, #
      30  17.5%  COOL, WARM, COOL, COOL, COOL, COOL, COOL, COOL, COOL, ...
      11   6.4%  COOL, WARM, COLD, COLD, COLD, COLD, COLD, COLD, COLD, ...

**Only the 36 rows of the second shape are the report the chapter asks for.** Over ninety rows
list decision words instead, and eleven of those write `COLD`, **a literal that appears in
neither the grammar, nor the prompt, nor rules_e04.c.** The grammar's three literals are
`COOL`, `WARM` and `THROTTLED`; `COLD` sits in the free-text field, whose class admits any
letters, so it is an invention and not a grammar violation.

**Checked rather than assumed: `COLD` was equally visible at 64 tokens, in exactly the same 17
rows.** The ceiling did not hide it. What the ceiling hid was the shape statistic, because
that counts only closed objects and the degenerate answers were among the 136 that never
closed. So the record of 11:12 today, reporting 7 shapes with the intended report as the
commonest at 36 of 44, was reading the well-formed minority.

    tools/prompt_echo.py        0 of 171 echo any prompt sentence verbatim
    the lookup-table baseline   76 of 171, 44.4 per cent, over 3 verdict classes

## THE HTTP COST IS NOT A CONSTANT OF THE ROUTE

    mean HTTP cost    3.5 ms on Monday 26 October 2026   ->   15.8 ms here

Four and a half times higher, on the same route, the same client and the same server. It is
still 0.11 per cent of a 14013.9 ms round trip, so the chapter's conclusion that variant D is
free at this interval survives. But **the figure scales with load and is not a property of the
wire**, and this run was throttled, which is the likely cause. The chapter should carry both
numbers rather than the smaller one alone.

## THE THERMAL TRACE, WHICH THIS RUN PRODUCED AS A BY-PRODUCT

    before   throttled=0x0      54.0 C
    during   throttled=0xe0008  84.2 C   bit 3 soft limit ACTIVE, plus sticky 17, 18, 19
    after    throttled=0xe0000  68.6 C   bit 3 gone, sticky bits remain
    later    throttled=0xe0000  62.8 C

No under-voltage bit at any point, so the supply held. Generation fell from 8.49 tokens per
second early to 6.27 late, and time per token rose from 117 to 159 ms. **So every timing in
this run is a throttled timing**, which is why the round trip is not published as a latency
and why the HTTP figure above is reported with its cause.

## WHAT THIS DOES NOT ESTABLISH

    established   the budget closed 127 of 136 objects; 9 remain and loop at the ceiling
    established   the typical answer needs 95 to 97 tokens, so 64 fell short by a third
    established   the model is deterministic here: same 76 agreements, same confusion
                  table, every answer a prefix of the other
    established   the Simpson reversal of 13:08 was an artefact of the ceiling
    established   42.2 per cent against a constant's 36.7, so the 95 per cent clause fails
    established   THROTTLED is never said, against the rule's 51
    established   only 36 of 171 answers have the intended report shape
    established   17 rows invent the literal COLD, in both runs alike
    established   the HTTP cost is 15.8 ms here against 3.5 ms in October
    NOT a latency  every timing here, the board having throttled from bit 3 active at 84.2 C
    NOT shown      what the model does unthrottled at this budget, which needs cooling
    NOT shown      why nine rows loop, which is a model question and not a budget one
