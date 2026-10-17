# e02 wiring: nothing on the header

Project e02 is the decision loop on a replayed CSV. No part is wired, no HAT is seated, and
nothing is read from the 40-pin header. The board is the Raspberry Pi 4 Model B with the card
`eplepi` (Raspberry Pi OS, Debian 13 trixie, user `bing`), a USB keyboard and either the
Renkforce USB/TTL serial console at 115200 or SSH. This is the one project in the volume
whose wiring figure shows an empty header, on purpose: the loop must run end to end before
any sensor is attached, and from e03 on the same binary runs with `--live` and a device.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | none |

## The one-HAT rule for this Pi

The Pi 4B header is free during e02. If the MCC 118 (card A) or the WS-28164 (card B) is
seated from a previous sitting, it may stay seated: e02 touches neither SPI0 nor any GPIO
line. It does not matter which card is in the board for e02 as long as the model file is on
it; the chapter names `eplepi`.

## Pre-flight, on the Pi 4B over SSH or the serial console

Memory first. The model file must fit under 60 percent of `MemAvailable` or the run is
SD-card paging, not a decision latency.

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    vcgencmd measure_temp
    ls -l ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf

`get_throttled` must read `0x0` before and after the 1000-decision run.

## The run

    cd ~/edge-llm-pi
    build/projects/e02-decision-loop/edgellm-e02 --project e02 \
      --replay projects/e02-decision-loop/replay/synthetic_1000.csv \
      --grammar projects/e02-decision-loop/grammar.gbnf \
      --prompt projects/e02-decision-loop/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --state /tmp/e02.state \
      --log projects/e02-decision-loop/evidence/e02_run.csv
    vcgencmd get_throttled

## Acceptance

    build/tests/oracle_check projects/e02-decision-loop/evidence/e02_run.csv \
      tests/oracle/e02.expected.csv
    python tools/agreement.py projects/e02-decision-loop/evidence/e02_run.csv

The oracle check compares the rule column byte for byte and will report every row where
the real model's column differs from the stub's; what passes the chapter is the agreement
tool's count of rows whose model column is not one of NORMAL, WATCH, ALERT, which must be
zero. The second acceptance number is the ratio of the `--cold` mean `prompt_ms` to the
state-restore mean `prompt_ms` from `e02_variant_a` on the same board and thread count,
which must be at least 2.0; throttle bits `0x0` before and after both.

**The second number needs a binary the project's build command does not make.**
`project.toml`'s `[build] command` builds `edgellm-e02`, `rules_test` and `oracle_check`,
which is what the host and CI need, and the two comparison loops are deliberately left out
of the stub build because they require the real engine. On the board, build them too before
running the acceptance, or the ratio has nothing to measure:

    cd ~/edge-llm-pi
    cmake --build build --target e02_variant_a e02_variant_b -j3

    cd ~/edge-llm-pi/projects/e02-decision-loop
    head -n 101 replay/synthetic_1000.csv > /tmp/e02_first_100.csv
    ../../build/projects/e02-decision-loop/e02_variant_a \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --replay /tmp/e02_first_100.csv --threads 2 \
      --state /tmp/e02_a.state --log evidence/e02_variant_a_restore.csv
    ../../build/projects/e02-decision-loop/e02_variant_a \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --replay /tmp/e02_first_100.csv --threads 2 --cold \
      --log evidence/e02_variant_a_cold.csv

Run from the project folder, because those two resolve `grammar.gbnf` and `prompt.txt`
relative to it. Three things in those two lines were missing when the sheet was
first written, and each was found by running it on Saturday 17 October 2026 rather than by
reading it.

**`--threads` must be passed, because the default is not the figure.** `e02_variant_a`
defaults to four threads. The acceptance above asks for the ratio on the same board and
thread count as the 1000-decision run, and that run was made at two threads, so a bare
command would divide a four-thread mean by a four-thread mean and report a ratio belonging
to no run in the evidence. Worse, two full passes at four threads is the continuous
four-core load that took this board down three times; this binary has no settling pause and
cannot be given one, because a gap between decisions is part of what it measures.

**Each pass needs its own `--log`,** or the second overwrites the first: both default to
`e02_a.csv` in the working directory. The means are printed on exit either way, but a figure
whose rows cannot be read again afterwards is not evidence, and the two files are also what
a later sitting would need to see whether the saving is uniform across rows or concentrated
in the first few.

**A hundred rows is the measurement, not a thousand.** The ratio is a per-decision prompt
cost, and its mean is stable long before row one hundred; two thousand-row passes is about
two and a half hours of unbroken load for the same number. The truncated file goes in `/tmp`
rather than `replay/`, so no second replay file enters the repository for a figure that does
not need one.

**The comparison binary aborted on its first decision, and had never been run before.**
`e02_variant_a` called `llama_sampler_accept` on the token `llama_sampler_sample` had just
returned. Sample already accepts, so the grammar advanced twice: the first legal piece was
consumed, then offered to a grammar that had moved past it, no viable stack remained, and
llama.cpp threw `Unexpected empty grammar stack after accepting piece: {" (4913)`. Both the
restore pass and the `--cold` pass died there, before one row was logged, so neither the
ratio nor the restore-versus-cold control could be measured at all.

`common/c/src/edgellm.c` carries the warning in its own header comment, that calling accept
a second time would advance the grammar twice, and does not make the call. That is why the
1000-decision run through the C core completed and this binary could not do one row. The
reason nobody noticed is structural and is the same gap as the missing build target above:
`project.toml`'s build command leaves `e02_variant_a` and `e02_variant_b` out, so until
Saturday 17 October 2026 neither binary had ever been built or run by CI, by the host, or on
a board. The chapter asked for one of its two acceptance numbers from code that had never
executed. The fix is one deleted line, and the comment above it now names the measurement.

Still unfixed and deliberately left: `generate()` decodes the sampled token on the final
iteration too, where the C core guards that with `i + 1 < n_predict`. It is a wasted decode
per decision, it inflates `generate_ms`, and it falls equally on both passes, so it does not
touch the ratio the acceptance asks for. Changing it would change a figure, so it waits for
a sitting where the before and after can both be measured.

`-j3` rather than `-j4` throughout, on this board: it has gone under-voltage on sustained
four-core load three times now, on three different workloads. A `-j4` compile on Friday
16 October 2026 took the board down with no log; the back-to-back benchmark stages of
Saturday 17 October 2026 raised bits 16 and 18 of `get_throttled` three times running; and
the first attempt at this chapter's own 1000-decision run, at four threads, ended at row 880
when the board rebooted, which was read off `uptime` rather than off any error. That is why
the run above is specified at two threads and the comparison loops are given `--threads 2`
to match. A settling pause protects a staged benchmark and protects neither a compile nor a
decision loop, so on this supply the only control left is the thread count, and it is
recorded as a condition of every figure e02 produces.

The missing build target was found by reading e02 ahead of its sitting rather than during
one, on Saturday 17 October 2026, which is the cheaper of the two places to find it. The
three defects in the comparison commands were found the expensive way, during the sitting.
The build command itself is unchanged: it is right for CI, and the bench needs one line
more.

## config.txt

No overlay and no line is added for e02.
