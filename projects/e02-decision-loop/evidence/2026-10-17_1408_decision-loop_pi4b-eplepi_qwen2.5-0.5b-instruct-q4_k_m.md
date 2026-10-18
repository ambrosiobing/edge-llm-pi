# e02 decision loop, Saturday 17 October 2026, Raspberry Pi 4 Model B, card eplepi

Every line of this file was written by hand. There is no runner for e02 as there is for e01,
so nothing here was produced by a program and each figure is transcribed from a terminal with
the command that produced it printed beside it. Where a figure is missing the file says so
rather than leaving a gap.

## The board and the engine

| | |
|---|---|
| Board | Raspberry Pi 4 Model B Rev 1.4, 8 GB, card `eplepi`, user `bing` |
| Header | bare, nothing seated |
| Engine | llama.cpp v0.6.0, commit d81235049384534c167caea52b85a694f6103d14, built on the board |
| Model | qwen2.5-0.5b-instruct-q4_k_m.gguf, 462.96 MiB, Q4_K_M |
| Context | n_ctx 512, n_batch 512, greedy under grammar |
| Threads | **2 for every run in this file** |
| Date | Saturday 17 October 2026, runs between about 11:11 and 14:23 |

**Two threads is a condition of every figure here, not a detail.** e02 at two threads is not
e02 at four, even though its acceptance is about decisions and state restore rather than
tokens per second. The thread count was lowered because the board had already restarted
unexplained twice under four-core load; it restarted a third time at four threads at row 880
of the first attempt at the thousand-decision run, and a fourth time at **two** threads during
the cold pass described below.

## Clause one: zero rows whose model column is not NORMAL, WATCH or ALERT

**FAILS. 949 rows of 1000.**

    cd ~/edge-llm-pi && build/projects/e02-decision-loop/edgellm-e02 --project e02 --threads 2 \
      --replay projects/e02-decision-loop/replay/synthetic_1000.csv \
      --grammar projects/e02-decision-loop/grammar.gbnf \
      --prompt projects/e02-decision-loop/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --state /tmp/e02.state --log projects/e02-decision-loop/evidence/e02_run.csv

1000 decisions, `throttled=0x0` before and after, `uptime` increasing throughout. The run was
relaunched detached at two threads after the four-thread attempt died at row 880; it completed
in about seventy-five minutes at 13.3 rows a minute.

`python tools/agreement.py` on that log:

    N 1000, scored 1000, empty model column 0, agreement 21 (2.1 percent)
    rule \ model      NORMAL{ "decision": "ALERT"
    ALERT                  1          66
    NORMAL                21         802
    WATCH                 29          81

**Two independent defects are visible in that table, and repairing one does not touch the
other.**

### Defect one, the generation budget, now cured

Every ALERT row read `{ "decision": "ALERT"` with no closing brace, against
`{ "decision": "NORMAL" }` complete. The grammar was never violated: each output is a legal
*prefix*, and the loop stopped at `n_predict` before the grammar reached its accept state.
This tokenizer splits ALERT into two pieces where NORMAL is one, so the ALERT path needs nine
tokens and at eight it loses its last. The grammar file had asserted that eight sufficed "on
every tokenizer in the ladder"; it had never been checked against one, and that phrase was
the part that made it sound checked.

Proved by changing only that number, same board, same day:

    cd ~/edge-llm-pi && build/projects/e02-decision-loop/edgellm-e02 --project e02 --threads 2 \
      --n-predict 12 --replay /tmp/e02_first_100.csv ... \
      --log projects/e02-decision-loop/evidence/e02_npredict12.csv

    e02: 100 events, 100 model answers, 0 model errors, 0 actuation errors,
         mean prompt 3081.2 ms, mean generate 2023.2 ms, rss 580208 kB,
         engine llama.cpp v0.6.0, source replay

**100 answers, 0 model errors.** Complete objects on every row of a hundred, not a sample.

### Every automated counter in the project read zero on this log

Material to the verdict above, and found the same day: **no check in this repository could
fail clause one.** Three counters exist and all three reported zero on the log where 949 rows
of 1000 held an object that never closed.

| counter | what it actually counts | on this log |
|---|---|---|
| `agreement.py`, "empty model column" | only an empty string; a truncated answer is a non-empty string and was scored as though it were a decision | 0 |
| the shared main loop, "model errors" | only `edgellm_decide()` returning non-zero, which is an engine failure | 0 |
| `e02_variant_a`, "rows without a literal" | a substring search, which finds ALERT inside the truncated form | 0 |

The chapter's acceptance step reads "the count of rows without a literal must be zero" and
names `agreement.py` as the thing that measures it. By that wording and that tool, **clause
one passed.** What caught the failure was a person reading `{ "decision": "ALERT"` as a column
header in the confusion table, which is not a check.

`agreement.py` now counts rows whose model column is not one of the decisions the rule
itself emitted in the log, prints them, and exits non-zero when there are any. It
discriminates the two cases that matter: a model answering NORMAL where the rule said WATCH
is the disagreement the agreement table exists to measure and is not a failure, while an
answer that is not a decision at all is. Both directions are tested. The same tool scores
sixteen of the twenty projects, so the clause was unenforceable in all of them, and the
wording in those chapters and manifests is a separate question from the tool.

### Defect two, the model itself, not cured by anything here

    python tools/agreement.py .../e02_npredict12.csv
    N 100, scored 100, agreement 4 (4.0 percent)
    rule \ model       ALERT      NORMAL
    ALERT                  2           0
    NORMAL                95           2
    WATCH                  0           1

The model answers ALERT on 97 of 100 here and on 949 of 1000 in the thousand-row run, and
**says WATCH never**. The three-way enum is binary in its mouth. The input is not at fault:
the user line carries the level, the slope and up to three previous levels, and the thresholds
are in the system prompt.

**4.0 percent and the thousand-row 2.1 percent are not comparable, and neither is the repaired
thousand-row figure.** The first hundred rows of the replay are 97 NORMAL, 1 WATCH, 2 ALERT;
the full thousand is 823, 110, 67. A slice that is almost entirely quiet gives the model
almost nothing to agree with. The hundred-row file was made for a timing ratio, where the head
of the replay is adequate because every row costs about the same, and it is not adequate for
an agreement figure. **The repaired agreement over the full thousand is unmeasured.**

## Clause two: cold mean prompt_ms over restore mean prompt_ms, at least 2.0

**NOT MEASURED. Blocked on the supply.**

The restore half is in hand, after a defect that had kept this binary from ever running:

    cd ~/edge-llm-pi/projects/e02-decision-loop && ../../build/projects/e02-decision-loop/e02_variant_a \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --replay /tmp/e02_first_100.csv \
      --threads 2 --state /tmp/e02_a.state --log evidence/e02_variant_a_restore.csv

    variant A (C++ on llama.h): rows 100, mode restore
    mean prompt_ms 1320.6, mean generate_ms 1506.3, rows without a literal 0
    VmRSS kB 579904

So the cold pass must exceed **2641.2 ms** for the ratio to reach 2.0.

Two cautions on that output. `rows without a literal 0` does **not** mean the text is complete:
variant_a has no `--n-predict` option, its budget is fixed at eight in the source, and its own
counter uses a substring search that finds ALERT inside the truncated form. Its log should not
later be fed to the agreement tool as though it were clean. And `mean generate_ms 1506.3` was
taken with a wasted decode of the final token on every row, which the C core guards against
and this binary does not; that flaw is left in place deliberately so that the cold pass is
measured by identical code.

**The cold pass has been attempted once and lost.** At about 14:23, roughly three minutes into
a run of about seven, with the board otherwise idle at two threads, the board restarted. It
left nothing: the binary buffered its log, so the file was zero bytes and every computed row
was gone, and `get_throttled` cannot be read after a restart because its sticky bits clear at
boot. `uptime` read 3 min at 14:26 where it had read 1:40 at 14:08, and that was the only
trace. A per-row flush has since been added so a repeat leaves the rows that did finish.

**Why this waits on hardware rather than on another attempt.** The ratio is two means that have
to be taken under the same conditions on the same board. A board that fails unpredictably
under the exact load being measured cannot supply either half as evidence, however many
attempts eventually produce a number. The supply had not been identified when this file was
written.

> **Correction of Sunday 18 October 2026.** It has since been identified. These runs, and
> those of Friday 16 October 2026, were on an ISY IAD-1025-1 USB-C dock and not on the ISY
> IAC-5065 charger the board carries today. The dock's exact topology on Friday 16 and Saturday 17 October 2026, meaning
> which plug entered the Pi's own USB-C socket, is not confirmed. The cold pass was completed
> on the charger on Sunday 18 October 2026 without a restart, and is recorded, together with
> the reason it is nonetheless discarded, in
> `2026-10-18_2133_cold-pass_pi4b-eplepi_qwen2.5-0.5b-instruct-q4_k_m.md`. A Pi 4 on four
> cores wants 5.1 V at 3 A over USB-C; the charger's 5 V rail is labelled 5.0 V at 3.0 A.

## What the four restarts say, and one claim withdrawn

| # | Workload | Threads | Detected by |
|---|---|---|---|
| 1 | `cmake --build -j4`, Friday 16 October 2026 | 4 | board vanished, no log |
| 2 | back-to-back benchmark stages, three refusals | 1 to 4 | `get_throttled` read `0x50000` before any reboot |
| 3 | this chapter's 1000-decision run, first attempt | 4 | `uptime` reset, log stopped at row 880 |
| 4 | this chapter's `e02_variant_a --cold` pass | 2 | `uptime` 3 min where it had read 1:40 |

A paragraph written into this project's wiring sheet that morning, claiming the thread count
was the one control left on this supply, is **withdrawn**. Two threads completed a thousand
decisions in about seventy-five minutes that morning and failed three minutes into a
seven-minute run that afternoon. A setting that survives the longer load and fails the shorter
one is controlling nothing; it was two successes read as a mechanism.

## The binary that had never been run

`e02_variant_a` aborted on its first decision, three times, before any of the above could be
attempted:

    e02_variant_a: Unexpected empty grammar stack after accepting piece: {" (4913)

It called `llama_sampler_accept` on the token `llama_sampler_sample` had already accepted, so
the grammar advanced twice. `common/c/src/edgellm.c` carries that warning in its own header
comment and does not make the call, which is why the C core managed a thousand rows and this
binary could not manage one. `project.toml`'s build command leaves `e02_variant_a` and
`e02_variant_b` out, so until this day neither binary had ever been built or run by CI, by the
host, or on a board: the chapter asked for one of its two acceptance numbers from code that
had never executed.

## Verdict

**e02 does not pass.** Clause one is measured and failed at 949 rows of 1000, and a repaired
budget does not reach it: the model agrees with the rule on 4.0 percent of a quiet hundred and
never says WATCH at all. Clause two is unmeasured and waits on the board's supply. The verdict
does not wait on it, because one failed clause is enough.

What the day did establish, and what a chapter can honestly print: a thousand grammar-
constrained decisions ran to completion on this board with one `state_write_data` and one
`state_read_data` per decision, so the mechanism e02 exists to demonstrate works; the cost is
about five seconds a decision at two threads; and the model adds nothing to the rule here,
which the volume's own spine allows as a result.
