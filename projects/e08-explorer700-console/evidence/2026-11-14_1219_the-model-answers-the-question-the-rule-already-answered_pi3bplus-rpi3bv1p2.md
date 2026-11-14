# e08: one of seven, and five of the six failures answer the question the rule had
# already answered. Saturday 14 November 2026, 12:19 to 12:21

    board      Raspberry Pi 3 Model B Plus Rev 1.3, card rpi3BV1p2,
               Raspberry Pi OS trixie, kernel 6.18.50+rpt-rpi-v8
    hat        Waveshare Explorer700, four stock Pi OS overlays
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf through the project's own daemon
               over /tmp/e08.sock, four threads, --n-predict 64, temperature 0.3
    logs       evidence/e08_live.csv, evidence/e08_live.stderr,
               evidence/e08_thermal_2026-11-14.csv, 1819 samples two seconds
               apart from 12:17:20 to 13:18:59, every line parsed
    instrument tools/answer_check.py, the project's own acceptance checker

**THE CLAUSE IS NOT MET AND THE FIGURE IS PUBLISHED ANYWAY**, which is this volume's rule
for a condition that fails: measured at X and not accepted is a different statement from
unmeasured. Two conditions failed, and both are stated with the number rather than folded
into it.

    asked        7 questions, not the 50 the clause names
    throttled    0x80000 by the end: the soft temperature limit was reached
    scored       1 of 7 answers carry the live reading within 1 per cent

## THE SIX FAILURES ARE TWO DIFFERENT THINGS, AND THE TOKEN COUNT SEPARATES THEM

    row   question                  n_generated   outcome
    2-6   is the clock drifting          32       wrong quantity
    7     how warm is the box            59       PASS
    8     how warm is the box            64       truncated at the budget

**Five of five answers to "is the clock drifting" read, character for character:**

    true, clock IN-SYNC, pressure 97.46 kPa, temperature 27.2 C.

The question asks for the difference between the DS3231 and the Pi clock in seconds. The
logged difference is 0.0 s. The answer does not contain it. What it contains is
**`IN-SYNC`, which is the rule's own verdict and sits in the next column of the same log
row**, plus the pressure and the temperature, which nobody asked for.

**And this is not the budget.** Those five answers stop at 32 generated tokens against a
ceiling of 64: the model had half its allowance left and still did not state the number.
The one answer that *was* truncated stops at exactly 64 and breaks off mid-word at
`previous 97.4`; the one that passed used 59. So the budget explains one failure and
excludes itself from the other five.

**This is a fourth way for the model to add nothing**, beside the three this volume had
recorded. Not the instruction echoed, not the input re-spelled in a template, not a
constant tying its own baseline: here the model answers a **different question**, and the
question it answers is the one the control rule has already answered without it.

## THE BOARD REACHES ITS SOFT LIMIT IN THIRTY-FOUR SECONDS

From the thermal log, two seconds apart. The load begins where the ARM clock leaves its
600 MHz idle:

    12:19:04   41.3 C   0x0        600 MHz    idle
    12:19:14   47.8 C   0x0       1400 MHz    under load
    12:19:46   59.1 C   0x0       1400 MHz
    12:19:48   60.1 C   0x80008   1200 MHz    the cap engages
    12:19:52   59.1 C   0x80000   1400 MHz    released, the sticky bit stays

**The soft temperature limit of this board is 60.1 C, measured to the tenth**, and it caps
the ARM clock from 1400 to 1200 MHz. `vcgencmd get_config temp_soft_limit` reports 0, which
means only that nothing is set in `config.txt`, not that the limit is absent: the firmware
default had to be observed under load rather than read.

**From the first loaded sample to the cap: seventeen samples, about thirty-four
seconds.** An earlier reading of this log said forty-four, counted from an idle sample ten
seconds before the load actually began; the whole file corrects it.

Two figures from the full hour. **The peak temperature is 60.1 C and is never exceeded**,
so the cap holds the board exactly at its own threshold. And the cap is *active* in only
**11 of 1819 samples**: the board touches the limit, is capped for a moment, and then sits
just below it. The sticky bit stays set from the first touch, which is what the acceptance
condition reads.

**So the clause's condition, `0x0` before and after fifty questions, is unreachable on this
board.** Fifty questions at about 17 s are roughly fifteen minutes of load against a limit
reached in three quarters of a minute. Joseph decided on Saturday 14 November 2026 that no
heatsink will go on this board, so the condition is permanently out of reach here, and the
figure above is published with that stated rather than with the condition quietly dropped.

For comparison, board A, the Pi 4B, took twenty-three minutes to reach its own limit at
80 C on Friday 13 November 2026. The two boards differ by a factor of thirty in time and
by twenty degrees in threshold.

## THE COST OF A QUESTION ON THIS BOARD

    prompt      6.8 to 7.7 s
    generate    8.0 s for a 32-token answer, 17.0 s for a 64-token one
    per call    15.8 s over the first three, 17.5 s over these seven

The rise is consistent with the clock cap but is not attributed to it: both runs crossed
the threshold, the samples are three and seven, and there is no control.

## WHAT THIS DOES NOT ESTABLISH

    established   1 of 7 answers carry the reading the question asked for
    established   five failures state the rule's own verdict and omit the asked number,
                  at 32 of 64 tokens, so the budget does not explain them
    established   one failure is a budget truncation at exactly 64 tokens
    established   64 tokens is marginal for question 2: the passing answer used 59
    established   the soft temperature limit of this board is 60.1 C, 1400 to 1200 MHz
    established   thirty-four seconds of load before the cap, 17 samples of 2 s
    established   the peak is 60.1 C over the whole hour and the cap is active in
                  11 of 1819 samples
    established   the whole chain works: key, cursor, question, daemon, model, answer, log
    NOT ACCEPTED  clause one. It asks 50 and it asks for 0x0 before and after; this run
                  asked 7 and ended at 0x80000
    NOT MEASURED  questions 0, 3 and 4. Only questions 1 and 2 were asked
    NOT SHOWN     that the clock cap caused the slower calls; no control was run
    NOT CLOSED    clause three, the recovery after signal 9
