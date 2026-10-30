# e06: the decision latency is a steady state, not a mean, Friday 30 October 2026, 13:52

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, hostname eplepi, booted 13:32:14
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes
    engine     llama.cpp v0.6.0, the pinned tag, real engine build at build/
    source     replay from projects/e06-vl53l4cd-too-close/replay/approaches.csv, no sensor
    settings   --max-events 40, --threads 4, --n-predict 64, e06's own grammar and prompt
    register   0x0 before, 0x80000 after; 41.8 C before, 78.8 C after
    log        /home/bing/e06_boardA_plateau.csv on the board, 40 rows

**This closes no clause of e06.** It replaces a projection that the board recommendation of
Friday 30 October 2026, 13:07 rested on, and it finds two things that were not being looked
for: the latency figure that record quotes is the wrong kind of average, and every model
answer in this run is the prompt's own instruction read back.

## THE PROMPT TIME IS A RAMP THEN A PLATEAU, SO A MEAN OVER A RUN IS NOT A LATENCY

The per-event table is the whole finding. The prompt time climbs for nine events, holds flat
for twenty-three, then steps upward for the last eight.

    events  1 to  9    prompt   818.1 to 2288.4    ramp, the context filling
    events 10 to 32    prompt  2395.5 to 2400.5    FLAT to 0.2 percent over 23 events
    events 33 to 40    prompt  2405.9 to 2461.6    rising 2.7 percent, the throttle

    STEADY STATE, events 10 to 32    prompt 2396.9 ms    generate 3412.4 ms    L 5.809 s

The loader reports `inp_tokens [i32, ne = { 512, ... }]`. The prompt grows as history
accumulates until it fills that context and then every prompt is the same size and costs the
same. **A hand approach run is in the saturated state within the first minute and stays
there, so the plateau is the only figure the speed window may use.**

Three averages of the same board, all arithmetically correct, none of them the latency:

    20 events, mean of all        L 5.697 s    ramp is 9 of 20, so it pulls hard
    40 events, mean of all        L 5.762 s    ramp is 9 of 40, so it pulls less
    40 events, plateau only       L 5.809 s    THE FIGURE
    the projection it replaces    L 5.88  s    off by 1.2 percent

**The projection was better than either measurement that replaced it.** It was computed from
the e03 thread ratios on Tuesday 27 October 2026 and labelled as an estimate throughout, and
it lands 1.2 percent from the plateau while the twenty-event mean lands 1.9 percent the other
way. The lesson is not that projections are reliable. It is that **a number's label told the
truth and its precision did not**, and the two measurements were wrong for a reason no label
carried: they averaged a transient with a steady state.

## THE THROTTLE ONSET IS VISIBLE, WHICH IS WHY A FAILED RUN IS STILL READABLE

This run ends `0x80000`, bit 19, soft temperature limit has occurred. e06's acceptance says a
run with any bit set is discarded, so **the run is discarded and this is not a gate pass.**

The table nevertheless says exactly when the clock changed. At a saturated prompt the token
count is constant, so the prompt time is a clock measurement: flat at 2396 ms through event
32, then 2405.9, 2409.2, 2411.8, 2416.4, 2424.9, 2455.7, 2453.8, 2461.6. **The onset is event
33 and the plateau used above ends at event 32.** The figure is defensible on the evidence
even though the run fails the rule, and those are different statements.

    events 1 to 32, inclusive    about 190 s of inference from a 41.8 C start

The standing figure for this board is a thermal limit after 367 s of load, taken from a colder
start. **The opening temperature is a first-order variable**, not a detail: 41.8 C bought
roughly half of 367 s. An earlier run the same afternoon opened at 59.4 C and tripped inside
114 s, and one opening at 32.1 C ran 113 s and stayed at `0x0`.

### What the thermal budget buys, as a number rather than a word

    board A delivers about 32 decisions before the soft limit
    e06's first clause asks for 200 approaches

**That is 16 percent of the run.** This is the figure that sizes the cooling part. "A fan is
needed" has been in the record since Thursday 29 October 2026; the useful form is that the
board completes under a sixth of the work and the shortfall is a factor between three and
nine depending on the approach speed chosen.

## GENERATION IS LINEAR IN TOKENS, AND THAT CLOSES AN OPEN QUESTION

    ntok 28    generate about 3285 ms
    ntok 30    generate about 3545 ms
    mean over the plateau: 28.96 tokens, 3412.4 ms, so 117.8 ms per token

The record of 13:07 left two events on board B unexplained, generating 9876 and 11261 ms
against a plateau near 6800, and declined to guess. **They were longer sentences.** Board A's
events 3 and 4 do the same thing with `ntok 43` and `48`, costing 5268.8 and 5981.9 ms, which
is 122 to 125 ms per token. No mechanism is needed beyond the token count, and in particular
it was not the throttle.

`ntok` never exceeded 48 against a budget of 64. **So `--n-predict` is decorative for e06**,
and the discrepancy flagged earlier today, that the chapter prescribes 40 in both places while
board B's figure was taken at 64, **is not a defect.** Two runs on this board at the two
budgets agreed to 1.3 percent on total latency and to 0.2 percent on the prompt. The grammar
completes its object first, so the budget is never reached and the figures were always
comparable. Measured, not inferred.

## THE WINDOW, RECOMPUTED FOR BOTH BOARDS FROM PLATEAU FIGURES

The floor belongs to the sensor and is unchanged: the spread of 3.18874 mm at 200 mm over 100
samples, measured on board B on Friday 30 October 2026, gives `v > 31.9 mm/s` at one standard
deviation and `v > 63.8 mm/s` at two. Only the ceiling moves between boards.

    board      L plateau    ceiling      one sd window        two sd window
    board A     5.809 s     96.6 mm/s    31.9 to 96.6         63.8 to 96.6, ratio 1.51
    board B    13.217 s     44.1 mm/s    31.9 to 44.1         EMPTY

Board B's figures are the plateau of its own 40-event run of the same afternoon, events 12 to
20, where its prompt held at 6301 ms to 1.3 ms. **Board B's ceiling falls from the 47.5 mm/s
the 13:07 record carries to 44.1 mm/s**, and its one-sd window narrows from a ratio of 1.49 to
1.38. The conclusion that board B has no window at two standard deviations is unchanged and
is now measured at both ends rather than at one.

**So e06 still belongs on board A at four threads, and the recommendation no longer rests on a
projection.** The two-sd ratio of 1.51 is the honest figure and it is tight: three speeds
inside it would sit near 70, 82 and 95 mm/s, stepping 7.0, 8.2 and 9.5 mm per sample against a
two-sd noise of 6.38 mm, so the slowest of the three has a margin of 1.10. That is a finding
about e06's design rather than a problem to be worked around.

## WHERE THE 12.24 SECOND FIGURE CAME FROM, AND WHY IT CANNOT BE CERTIFIED

The ceiling of 47.5 mm/s in the 13:07 record is built on `L = 12.24 s`, which is run 5 of
Thursday 29 October 2026: board B, four threads, live, 1000 max-events, **8 answers**, prompt
5066.8 and generate 7172.5.

**Eight answers is entirely inside the ramp.** On board B the plateau begins at event 12, so
run 5 stopped four events before the steady state existed. Its figure is a measurement of the
warm-up and it understates the board.

**And no register reading for run 5 exists in any record.** Neither e06 evidence file carries
a `get_throttled` value for it. In its favour, eight answers is about 112 s of load against
board B's 282 s trip point, so it was probably clean. Probably is not a record, and the two
runs today that ended `0x80000` and `0x80008` are what make the word unacceptable. The figure
is superseded rather than corrected, and it should not be requoted.

## EVERY ANSWER IN THIS RUN IS THE QUESTION READ BACK

This was not what the run was for and it outranks what the run was for.

    prompt.txt   "In beam, one short sentence saying what is probably in front of the sensor."
    model        "beam": "short sentence saying what is probably in front of the sensor."

    instruction echoed in rows: 40 of 40

Two rows show it degrading rather than improving: one appended the raw range, giving
`...in front of the sensor, 902.`, and another trailed off as `...in front of the sensor, e.`

**The grammar permits this by construction and cannot be made to notice it:**

    beam ::= "\"" [A-Za-z0-9 ,%-]{8,120} ".\""

Eight to 120 characters from that class, ending in a full stop. The prompt's own sentence is a
legal instance. **The only check on the field is a shape, and the question has the shape of an
answer.**

The decision field is no better on this input. The ranges are 902, 897 and 901 mm, the rule
says `CLEAR` on every row, and the model said `APPROACHING` on all forty. That is consistent
with what is already recorded and was read as a prompt problem: run 3 of Thursday 29 October
2026 scored 17 of 50 with `APPROACHING x50`, and run 5 scored 0 of 8 with `APPROACHING x8`.
**It is now 0 of 40 at four threads on board A.**

So the latency measured above is honest as a latency and it is the time taken to produce a
wrong answer. The window arithmetic is unaffected, because it asks how fast a decision arrives
rather than whether it is right. **e06's first clause, the agreement table, is in trouble for a
reason that has nothing to do with cooling and that a fan will not fix.**

## A QUESTION THIS RAISES ABOUT THE CODE, NOT YET ANSWERED

If the prompt grows until it fills 512 tokens and then stops growing, something is dropping
history to keep it inside the window. For a project whose decision depends on the five newest
ranges being lower than the one before, **what gets dropped is a correctness question rather
than a performance one.** e06's prompt assembly has not been read and nothing is claimed about
it here. It is a better reason to read that code than the latency was.

A second question of the same kind: board B's prompt saturated at event 12 and board A's at
event 10, from the same replay file. Two runs of the same input should fill the same context
at the same rate. The difference is small and may be an artefact of where each run's first
answer landed, and it is not explained.

## Four mistakes of mine this afternoon, each with its correction

1. **I indexed the log by column position after indexing it by offset from the end.** The
   model field contains commas and quoting does not protect it under `-F,`, so the field count
   varies per row. `$(NF-3)` is correct and `$6` is not. The first extraction worked and I
   rewrote it while describing the rewrite as more robust.
2. **I wrote measurement logs to `/tmp` on a board whose register is cleared by rebooting.**
   The reboot that clears bit 19 also clears `/tmp`. Two logs were lost that way, one of them
   after I had already seen the mechanism take the first. Measurement logs belong in `~`.
3. **I concluded this board was thermally clear for e06 from a single run from cold.** The
   next run tripped. One run is not a board property, and the claim was withdrawn in the same
   hour it was made.
4. **I read a board B paste as board A's second run.** Both boards are `bing@` with the same
   repository path. The hostname guard inside the command is what caught it, and only because
   the prompt line was pasted along with the output.

## What is established and what is not

    established   board A's plateau latency for e06 at four threads is 5.809 s, MEASURED
    established   board A's ceiling is 96.6 mm/s and its two-sd window is 63.8 to 96.6
    established   board B's plateau latency is 13.217 s and its ceiling 44.1 mm/s
    established   board B has no window at two standard deviations, measured at both ends
    established   generation is linear in tokens at 117.8 ms per token on board A
    established   --n-predict is decorative for e06; the grammar terminates first
    established   board A completes about 32 of the 200 approaches before the soft limit
    established   40 of 40 answers echo the prompt instruction into the beam field
    established   0 of 40 decisions agreed with the rule on this input
    superseded    L = 12.24 s for board B, a mean over 8 ramp events with no register reading
    NOT shown     e06's first clause, the 200 approaches, still blocked on cooling
    NOT shown     e06's third clause, which needs a run that passes the register check
    NOT read      e06's prompt assembly, and what it drops when the context fills
