# e02 clause two accepted, the cooled pair, Monday 19 October 2026

Written by hand like the files beside it. Every figure was recounted from the logs' own columns
on the authoring laptop; where the board's summary line is quoted beside the recount they
agreed. This is the third attempt at this pair and the first the chapter accepts.

| | |
|---|---|
| Date | Monday 19 October 2026, 14:30:57 to about 14:48:40 local |
| Board | Raspberry Pi 4 Model B, 8 GB, card `eplepi`, revision `d03114`, **no HAT fitted** |
| Supply | ISY IAC-5065, 65 W USB-C charger, 5 V rail labelled 5.0 V at 3.0 A |
| Clock | 1.800 GHz from `arm_boost=1` |
| Model | `qwen2.5-0.5b-instruct-q4_k_m.gguf`, 491400032 bytes, `n_ctx` 512 |
| Threads | 2, both halves |
| Engine | `llama.cpp v0.6.0`, built from the submodule into `build-engine/` |
| Starting temperature | **37.0 degrees C**, the coldest start of any run in this project |
| **Clause two** | **PASSES** |

## The acceptance, line by line

The clause asks for a ratio of at least 2.0 between the cold and restored mean `prompt_ms`,
measured on the same board at the same thread count, with `get_throttled` reading `0x0` before
and after. It was not amended at any point; the decision of Monday 19 October 2026 was that
cooling is a condition of the run rather than a change to the criterion.

| | restore | cold |
|---|---|---|
| rows | 100 | 100 |
| mean `prompt_ms` | 1318.6 | 5867.3 |
| mean `generate_ms` | 1508.7 | 1494.9 |
| NUL bytes in the file | 0 | 0 |

**Ratio 4.4495**, against an acceptance of at least 2.0. State restore saves **4548.6 ms** on
every decision.

**And it holds row by row, which the ratio of two means cannot show.** Per-row ratios run from
4.246 to 4.473 with a median of 4.461, and **zero of 100 rows** fall below 2.0.

| quarter | restore | cold | ratio |
|---|---|---|---|
| rows 0 to 24 | 1316.8 | 5879.7 | 4.465 |
| rows 25 to 49 | 1317.4 | 5878.0 | 4.462 |
| rows 50 to 74 | 1323.0 | 5863.3 | 4.432 |
| rows 75 to 99 | 1317.4 | 5848.2 | 4.439 |

Row 0 is not an outlier in either half, at 0.3 percent below its own pass for the restore and
0.2 percent above for the cold.

## The register, which is what the two earlier attempts failed on

`get_throttled` read `0x0` before the pair and `0x0` after it, and **zero of 1500 trace samples
were dirty**. The sticky bits of that register do not clear until the board boots, so a single
engaged moment anywhere in the seventeen minutes of the pair would have left every later sample
non-zero, and the trace runs on for twenty-five minutes in total. All 1500 read `0x0`.

**The sample count was corrected on Monday 26 October 2026, and this is why.** This file first
said 1328, because the trace was copied off the board while the sampler was still running and
the copy was committed as if it were the whole run. The board still held it: 1501 lines against
the committed 1329, identical for every byte of the shorter one, so the committed file was a
prefix and not a different measurement. The missing tail is 172 rows from 14:52:18 to 14:55:12,
every one of them `0x0`. The figures below are recounted from the full trace. Nothing in the
verdict moves. What moved is a denominator that was a count of a clipped file, and one derived
count inside this record.

**A second, independent confirmation from the clock column.** 270 of the 1500 samples sit below
1.7 GHz, and **none of them falls inside either pass**: four are during the first model load, at
seconds 3 and 9 to 11, and the remainder begin at second 1048, after the cold pass ended, where
the on-demand governor idles the core. All 99 that the recovered tail adds are in that idle
stretch, which is why the count moved and the claim did not; `tools/verify_e02_figures.py`
checks the inside-the-pass count as a row of its own and it stayed at zero through the
correction. So the clock held 1.800 GHz for every decision of both
halves. A minimum taken over a whole trace cannot tell frequency scaling from a reduction, which
is why the span matters and not just the minimum.

The second model load, between the halves, shows no idle samples at all, because the model file
was already in the page cache from the first.

## What the cold start bought

| Run | Start | Peak | Register |
|---|---|---|---|
| this pair | 37.0 degrees C | **72.5** | clean throughout |
| clause one, 80 minutes, Monday 19 October 2026 | 53.5 degrees C | 79.8 | clean throughout |
| the cold pass of Sunday 18 October 2026, MCC 118 fitted | 56.9 degrees C | 80 to 83 | bit 19 set |

Fifteen degrees of extra headroom at the start translated into seven degrees of lower peak,
which was the difference between a clean register and a dirty one. Two things changed between
the bottom row and the other two, the HAT and the starting temperature, so neither is isolated
and this is not an attribution.

## Drift that did not happen, and why that supports the earlier one

| | first quarter to last |
|---|---|
| restore half | plus 0.05 percent |
| cold half | minus 0.54 percent |
| the cold pass of Sunday 18 October 2026 | plus 1.4 percent |

Sunday's drift was attributed to the clock moving between 1.800, 1.775 and 1.726 GHz while bit 3
toggled. This pair had no clock movement and no drift. Drift where the clock dropped, none where
it held: two cases pointing the same way, which is stronger than the one case that produced the
attribution.

The internal check agrees. Generation does not depend on whether the prompt was decoded or
restored, so the two halves should match: 1508.7 against 1494.9, a difference of **0.91
percent**, against 2.35 percent on Sunday's warmer board.

## The discarded pair was accurate, and discarding it was still correct

| | accepted, today | discarded, Sunday 18 October 2026 | difference |
|---|---|---|---|
| restore `prompt_ms` | 1318.6 | 1325.6 | minus 0.53 percent |
| cold `prompt_ms` | 5867.3 | 5927.5 | minus 1.02 percent |
| restore `generate_ms` | 1508.7 | 1524.8 | minus 1.06 percent |
| cold `generate_ms` | 1494.9 | 1560.6 | minus 4.21 percent |
| **the ratio** | **4.4495** | 4.4715 | minus 0.49 percent |

This is the part worth keeping. On Sunday the measurements taken during that run would have
supported amending the criterion to let it through, and the decision was to leave the criterion
alone and treat cooling as a condition of the run. Three hours of work later the unamended
criterion is satisfied, and the figure the protocol withheld turns out to have been right to
half a percent.

A protocol that holds out a correct figure is a protocol doing its job. There is no way to
establish that except to take the clean measurement afterwards and compare, which is why this
table exists. Had the criterion been amended instead, the same number would stand today resting
on an argument rather than on a clean register.

## One caveat about the trace's time base

The sampler is nominally one sample a second and is actually 1.020 seconds, because three
`vcgencmd` calls run before each sleep. Over 1500 samples that is 30 seconds of accumulated
slip, so the sample index is not a clock and the `unix_s` column is what to use for timing. It
affects no figure in this file.

The full trace measures that interval better than the clipped one argued it. First sample
1792412983 at 14:29:43, last 1792414512 at 14:55:12, so 1529 seconds span 1499 intervals and
the mean interval is 1.0200 seconds. That is the published 1.020 to four figures, arrived at
from the span rather than from counting `vcgencmd` calls, which is the one figure the
correction improved rather than merely restated.

## What is still open in this chapter

Clause one and clause two both pass. The agreement figure of 8.7 percent over the thousand is
the chapter's result and it is a poor one, which the front matter allows for. What is not
measured here:

- The pair is a hundred rows, which the chapter chose deliberately because the ratio is a
  per-decision quantity and every row costs about the same. It is not a thousand.
- One sitting on one supply. A second day's pair would make the ratio a two-day figure, which
  is what the measurement protocol asks of e01 and not of this clause.
- The soft temperature limit's threshold is bracketed between 79.8 and roughly 80 degrees C
  from two runs, and is not a measured value.
