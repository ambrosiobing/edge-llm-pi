# e02 clause one passes, the thousand decisions, Monday 19 October 2026

Written by hand, like the files beside it, because e02 has no runner as e01 does. Every figure
here was recounted from the log's own columns by `tools/verify_e02_figures.py` and a scratch
recount, not transcribed from the summary line the board printed. Where the board's figure and
the recount are both quoted they agreed.

| | |
|---|---|
| Date | Monday 19 October 2026, about 11:18 to 12:41 local, 83 minutes |
| Board | Raspberry Pi 4 Model B, 8 GB, card `eplepi`, revision `d03114`, **no HAT fitted** |
| Supply | ISY IAC-5065, 65 W USB-C charger, 5 V rail labelled 5.0 V at 3.0 A |
| Clock | 1.800 GHz from `arm_boost=1`, full for every sample under load |
| Model | `qwen2.5-0.5b-instruct-q4_k_m.gguf`, 491400032 bytes, `n_ctx` 512 |
| Threads | 2 |
| Budget | `--n-predict 12` |
| Engine | `llama.cpp v0.6.0`, ggml commit `d81235049`, built from the submodule into `build-engine/` |
| **Clause one** | **PASSES** |
| Clause two | unchanged: measured at 4.47 and not accepted |

## The clause, and why it passes

The acceptance asks for 1000 decisions with zero free text. The agreement tool's gate is the
count of rows whose model column is not one of NORMAL, WATCH or ALERT, which must be zero.

| What | Saturday 17 October 2026, budget 8 | Monday 19 October 2026, budget 12 |
|---|---|---|
| rows | 1000 | 1000 |
| objects that never closed | **949** | **0** |
| empty model column | 0 | 0 |
| NUL bytes in the file | 0 | 0 |
| `n_generated` values present | 8 only | 8 and 9 |

Nine is the ALERT path's true cost and 8 was one short. Twelve was never approached, so the
one-token diagnosis is confirmed by the column rather than argued from it. A budget of 9 would
have sufficed; 12 is slack and an unused token is free, because the loop stops at the
end-of-generation token and this root ends at its closing brace.

## The result the chapter exists to report

Agreement is **87 of 1000, 8.7 percent**.

| rule says | model NORMAL | model WATCH | model ALERT | recall |
|---|---|---|---|---|
| ALERT, 67 | 1 | 0 | 66 | 98.5 percent |
| NORMAL, 823 | 21 | 0 | 802 | 2.6 percent |
| WATCH, 110 | 29 | 0 | 81 | **0.0 percent** |

**Against the only baseline that matters.** A program that ignores its input and answers the
same literal every time scores the frequency of that literal in the replay:

| strategy | agreement |
|---|---|
| always NORMAL, which is what the CI stub does | 823 of 1000, 82.3 percent |
| always WATCH | 110 of 1000, 11.0 percent |
| **the model** | **87 of 1000, 8.7 percent** |
| always ALERT | 67 of 1000, 6.7 percent |

So the model is beaten by a constant NORMAL by a factor of **9.5**, and beats a constant ALERT
by two percentage points, which is roughly what answering ALERT 949 times out of 1000 should
score. The volume's through-line question is what a small local model adds over a two-line
threshold. For e02 the answer is nothing, and this is the number.

It finds 66 of 67 real alarms, which is the one thing it does well, and calls 802 of 823 quiet
events alarms as well. That is a detector with no threshold rather than one with a badly set
threshold, and the safety rule in the chapter's spine is the reason it costs nothing here: the
model never actuates, the rule does.

## A prediction, stated on Sunday and confirmed exactly

On Sunday 18 October 2026, with only the broken run in hand, the repaired figure was predicted
as 87 of 1000 by reading the literal out of each truncated string and arguing that greedy
sampling under the same grammar and the same restored state cannot change which literal it
picks when it is merely allowed more tokens afterwards. That was recorded in the pair's evidence
file as a prediction with its mechanism.

The run confirms it at the single row:

| | budget 8 | budget 12 |
|---|---|---|
| model ALERT | 949 | 949 |
| model NORMAL | 51 | 51 |
| rows choosing the same literal | | **1000 of 1000** |

Not one row of a thousand changed its answer. The budget controlled whether an object closed
and never what it said. In the chapter's agreement table this is visible directly: the
budget-12 block's ALERT column is the budget-8 block's "Did not close" column, row for row.

## Timing, and a drift that did not happen

| | |
|---|---|
| mean `prompt_ms` | 2993.3 |
| mean `generate_ms` | 2008.1 |
| per decision | 5001.5 ms |
| 1000 decisions | 83.4 minutes, against the 85 the chapter computed from the hundred-row run |

**And the within-run drift is 0.2 percent, where the cold pass of Sunday 18 October 2026 drifted
1.4 percent.** By quarter the mean `prompt_ms` reads 2987.2, 2996.1, 2995.6, 2994.4: flat.

That difference is explained by the trace rather than guessed at, and the explanation runs the
right way round. The cold pass drifted while bit 3 toggled and the clock moved between 1.800,
1.775 and 1.726 GHz. This run had **zero** samples below 1.7 GHz and a register that never went
dirty, so the clock never moved and neither did the timing. Drift appeared where the clock
dropped and did not appear where it held, which is two cases pointing the same way.

## The thermal trace, 6000 samples at one a second

Written by `tools/throttle_log.sh`, capped at 6000 samples so a detached sampler could not
outlive the run. It covers the run and about twenty minutes of cooldown.

| | |
|---|---|
| samples | 6000, no NUL bytes |
| temperature, whole trace | 47.7 to 79.8 degrees C |
| under load, 60 s to 5000 s | 4832 samples, 67.2 to 79.8 degrees C, median **76.9** |
| samples below 1.7 GHz under load | **0** |
| clock range under load | 1800404352 to 1800457088 Hz |
| **dirty-register samples** | **0 of 6000** |

**That zero is the strongest reading in this file after the clause itself.** The sticky bits of
`get_throttled` do not clear until boot, so a single engaged moment anywhere in the run would
have left every later sample non-zero. All 6000 read `0x0`. The soft temperature limit never
engaged across eighty minutes of continuous load at a median of 76.9 degrees C.

**Which gives the soft limit a measured bracket for the first time.** The threshold could not be
verified from the Raspberry Pi configuration documentation at all and was marked to verify.

| Evidence | Peak | Register |
|---|---|---|
| this run, 6000 samples, no HAT | 79.8 degrees C | clean throughout |
| the cold pass, Sunday 18 October 2026, spot readings, MCC 118 fitted | 80 to 83 degrees C | bit 19 set |

So it sits above 79.8 and at or below roughly 80. Two configurations changed at once between
those rows, the HAT and the load, so neither is isolated and the bracket is what the pair of
runs supports rather than an attribution.

## What this says about clause two, which is still not accepted

A board that holds eighty minutes of this load with a clean register and no clock reduction may
well hold an eighteen-minute pair. That moves the cooled pair from waiting on cooling that does
not exist to worth attempting as the board stands.

It does not settle it. The cold pass is far heavier per decision, about 5830 ms of prompt
against the 2993 here, and a heavier load reaches a higher plateau. The bracket above says the
margin is a couple of degrees.

## Two defects of mine that this run exposed

**The build directory was flipped to the stub and the real binary lost.** The verification build
of Sunday 18 October 2026 ran `cmake -S . -B build -DEDGELLM_STUB_MODEL=ON` in the directory the
Saturday figures came from, which replaced the real-engine binary with a stub one at the path
the wiring sheet names. A stub run does not fail; it answers the grammar's first expansion on
every row and writes a plausible thousand-row log whose agreement figure means nothing. It was
caught before the run by reading `build/CMakeCache.txt` rather than by trusting the directory.
The engine now lives in `build-engine/` and the stub stays in `build/`, so neither can clobber
the other, and the engine column of this log reads `llama.cpp v0.6.0`.

**The launch command was silently re-runnable and was pasted twice.** Two engine instances ran
for about two minutes against one log and one state file, which is four threads of continuous
inference, the load this board has gone down under three times. The sampler refused to overwrite
its trace and exited, which is the guard working; the binary has no such guard. The log was
discarded and the run restarted from nothing. The launch line in the wiring sheet now refuses
when an engine is already running or the log exists, and `elog_open` should get the same refusal
in C.
