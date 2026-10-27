# e04, the thermal and throttle narrator, replay run, Monday 26 October 2026, 22:55

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72
    card       eplepi, Raspberry Pi OS, Debian 13 trixie
    HAT        none read; e04 touches no bus and no GPIO line
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes,
               sha256 2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2
    engine     llama-server b11429-d81235049, llama.cpp v0.6.0, four threads, -c 512
    variant    catalogue D, a libcurl client of llama-server on the loopback interface
    source     replay/benchmark_180.csv, 180 events
    logs       evidence/e04_replay.csv 181 lines 52893 bytes,
               evidence/e04_replay_rtt.csv 181 lines 8146 bytes

The replay half of e04. The measurement the chapter exists for succeeds; two clauses fail,
one of them in a way the client's own summary concealed.

## What was measured

    e04: 180 events, 180 model answers, 0 model errors, 0 vcgencmd errors,
         mean round trip 7683.4 ms, mean server prompt 2124.8 ms,
         mean server generate 5555.1 ms, mean HTTP cost 3.5 ms,
         agree 76, disagree 104 (of which 16 on a row where the rule changed),
         no literal 0, source replay

    rows at the 64-token ceiling          159 of 180, 88.3 percent
    rows whose object never closed        136 of 180, 75.6 percent

    rule    COOL 63   WARM 66   THROTTLED 51
    model   COOL 111  WARM 69   THROTTLED  0

## The HTTP cost, which is what the chapter was built to find

**3.5 ms of a 7683.4 ms round trip, 0.046 percent.** The client's own clock against the
server's reported timings, as a subtraction: 7683.4 minus 2124.8 minus 5555.1. Catalogue
variant D, a libcurl client talking to `llama-server` over the loopback, is free at this
interval. That row read "not measured" in the chapter's comparison table until this run.

**The like-for-like half was measured on Tuesday 27 October 2026 at 16:26**, recorded in
`projects/e02-decision-loop/evidence/2026-10-27_1626_inprocess-360m-comparison_pi4b-eplepi_smollm2-360m-instruct-q4_k_m.md`.
That row had read "not measured" since this chapter was written.

    route                     threads  budget  prompt   generate  tokens  per token  total
    D, llama-server, HTTP           4      64  2124.8    5555.1   64.0      86.8    7683.4
    C, in process, e02's loop       2      12  2771.2    1110.1    9.09    122.1    3881.3

**And carrying it out showed the comparison cannot be made cleanly as specified.** This
chapter fixes four threads and a 64-token budget; e02 fixes two and 12. The table asks for
"e02's loop on SmolLM2-360M with no other change", and honouring that moves two variables at
once, so the two totals are not comparable. That is a defect in the comparison's own
specification rather than in either run, and it is recorded rather than papered over.

What survives the confound is the number this chapter exists for, because it is a
subtraction within a single route: **the wire costs 3.5 ms** against a decision costing
between 3.9 and 7.7 seconds by either path. Variant D is free at this interval.

And per token the generate figures compare after normalising: **86.8 ms at four threads
against 122.1 at two, so four threads are 1.41 times faster per token.** Cores buy real
speed on this board. An earlier reading of that run divided by the budget of 12 instead of
the `n_generated` column, which averaged 9.09, and reported 6.6 percent with the opposite
conclusion. Divide by what was generated, never by what was permitted.

## The model never reaches its third class

Zero THROTTLED across 180 events where the rule called THROTTLED on **51**. The arithmetic
closes exactly: all 76 agreements fall among the 129 rows the rule called COOL or WARM, and
51 of the 104 disagreements are simply the class the model never emits.

    agreement on the two classes it does use      76 of 129, 58.9 percent
    recall on THROTTLED                            0 of 51
    overall                                       76 of 180, 42.2 percent
    a constant COOL                               63 of 180, 35.0 percent
    a constant WARM, the best single answer        66 of 180, 36.7 percent

So the model beats the best constant answer by **ten rows out of a hundred and eighty**, and
misses every throttling event. Rows 52 to 60 replay 80.2 C with the throttle word `0xa000a`,
which has bit 3 set, the soft temperature limit; the rule calls that THROTTLED and the model
answers WARM or COOL. It was given the thresholds in its prompt and does not apply them.

The published 35.0 percent for the mock is confirmed by this run from the other side: a
constant COOL scores 63 of 180, which is 35.0 percent exactly.

## The safety rule, now with a number behind it

The volume's safety rule has always said the model is never in the path to an LED, a buzzer,
a modem send or any protective action; the rule acts and the model explains. This is the
first measurement that prices it. **Had e04's model been in that path, 51 of 51 throttling
events would have gone unreported.**

## A clause that passed as written and should not have

The client printed `no literal 0` and that was first read as satisfying "zero rows with no
parseable object". It does not. **136 of 180 objects never closed**, 75.6 percent, because
`decision` is emitted first and parses before the free-text summary degenerates and the
budget cuts the string mid-flight. 159 of 180 rows reached the 64-token ceiling.

`no literal` and `object closed` are two different questions and the client answers only the
first. That is a defect in the instrument and not only in the run, and it is recorded here as
one: the summary line is not sufficient evidence for that clause, and the counts above come
from the log's own `n_generated` column and a pattern over the model column.

A larger budget would not fix it. It would lengthen the repetition. Compare row 10 against
row 20:

    row 10: { "decision": "COOL", "summary": "SoC temperature 45.4 C, throttle bits 0x0,
              ARM clock 1500 MHz, previous temperatures 45.8, 45.6, 45.7." }
    row 20: { "decision": "COOL", "summary": "COOL, WARM, COOL, WARM, COOL, COOL, COOL,
              COOL, COOL, COOL, COOL, COOL, COOL, COOL, COOL, COOL, COOL

On roughly half the events the model fills the summary by repeating its own enum literals
until the ceiling. 94 of 180 rows carry that pattern.

## What passes

**No hallucinated numbers in the well-formed rows.** Row 59 reads
`"SoC temperature 80.3 C, throttle bits 0xa000a, ARM clock 1000 MHz, previous temperatures
79.8, 80.2, 80.1."` and all four values match the replayed row. That is one of e04's
acceptance clauses and it holds.

## The clauses

    HTTP round trip cost measured               PASSES   3.5 ms of 7683.4 ms, and the
                                                in-process arm measured Tuesday 27 October
                                                2026, though not comparable as specified
    agreement at least 95 percent               FAILS    42.2 percent, 46.3 percent
                                                         excluding rule-change rows
    zero rows with no parseable object          FAILS    136 of 180 never closed
    no hallucinated numbers                     PASSES   in the well-formed rows
    self-heating of narrating                   NOT MEASURED, needs the --rule-only
                                                idle trace over the same benchmark
    95 percent during an e01 benchmark          NOT MEASURED, this is the replay run

**The acceptance is not changed.** e02 settled that on Monday 19 October 2026: a goalpost
moved after the measurement that failed to reach it is not a protocol change however well
argued. e04 reads measured at 42.2 percent and not accepted, which is a different statement
from unmeasured.

## Power

`get_throttled` read `0x0` before the run and **`0xf0000`** after: all four sticky bits,
under-voltage, frequency capping, throttling and the soft temperature limit. Twenty-four
minutes of a 360M model at four threads took this board from 42 C into its limits, and the
server log shows it, with eval time drifting from 83 ms per token early to 92 ms late.

The HTTP cost of 3.5 ms survives that, being a difference between two clocks on the same
rows. The absolute latency figures do not and are labelled accordingly. e03 then did the same
thing on Tuesday 27 October 2026, going `0x0` to `0xd0000`.

**The boundary was then found, the same afternoon.** e02's in-process arm ran two threads for
about six and a half minutes and finished at `0x0`:

    run   threads  events  duration        register after
    e02         2     100  about 6.5 min   0x0
    e03         4     200  about 16 min    0xd0000
    e04         4     180  about 24 min    0xf0000

All three began on a clean boot. So the brownout is a function of thread count and duration,
not something inherent to running a model on this board, and the remaining projects have a
usable rule rather than a warning. It also means this run's latency figures are upper bounds
while e02's arm is clean, which is the asymmetry the comparison above has to carry.

## Deviations from what the chapter prints

1. **Run as `bing` from a root login rather than through the systemd unit**, so the unit's
   `SupplementaryGroups=video` is not what was exercised. The client reads `/dev/vcio`
   through `vcgencmd` and reported 0 errors doing so.
2. **The HAT state is not recorded for this run**, and e04's own file requires it, because
   the temperature trace is comparable between runs only at the same HAT state. The next
   run must state it.
3. **The engine was built on a tree that had just been repaired.** The build before it left
   two zero-byte object files from an interrupted session on Friday 16 October 2026, and a
   cached `EDGELLM_STUB_MODEL=ON` had silently removed the entire engine from the configure.
   Both were found and fixed before this build; the run itself followed a clean reboot at
   `0x0`.

## How the logs reached this repository

`scp` from the board to the authoring laptop on Tuesday 27 October 2026, **after the run had
exited**, then verified against the board's own counts before anything was written from them:
181 lines and 52893 bytes for the log, 181 lines and 8146 bytes for the round-trip file, no
carriage returns, matching on both sides. That check exists because e02's register trace was
once committed as a 1329-line prefix of its own 1501-line run, copied while the sampler was
still writing.
