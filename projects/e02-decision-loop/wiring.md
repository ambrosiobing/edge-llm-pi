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
four-core load that took this board down three times. Two threads is not a safe operating
point either: see the correction at the end of this section.

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

## The supply, and a claim withdrawn the same day it was written

Four unexplained failures on this board, all under continuous load, all on Saturday
17 October 2026 except the first:

| # | Workload | Threads | How it was detected |
|---|---|---|---|
| 1 | `cmake --build -j4` on Friday 16 October 2026 | 4 | board vanished, no log at all |
| 2 | back-to-back benchmark stages, three refusals | 1 to 4 | `get_throttled` read `0x50000` before any reboot |
| 3 | this chapter's 1000-decision run, first attempt | 4 | `uptime` had reset; the log stopped at row 880 |
| 4 | this chapter's `e02_variant_a --cold` pass | **2** | `uptime` read 3 min where it had read 1:40 |

Event 4 happened at about 14:23, roughly three minutes into a run of about seven, with the
board otherwise idle. It left nothing behind: the binary buffers its log, so the file was
zero bytes, and `get_throttled` cannot be read after the fact because the sticky bits clear
at boot. Only the reset `uptime` recorded it.

**The claim written here earlier the same day, that on this supply the thread count is the
one control left, is withdrawn.** Two threads completed a thousand decisions that morning in
about seventy-five minutes, then failed three minutes into a seven-minute run that afternoon.
A setting that survives the longer load and fails the shorter one is not controlling
anything; it was a count of two successes read as a mechanism, which is the same error as
taking a tally for a reason. No thread count has been shown safe on this board, and the
chapter must not print one as though it had.

What does survive the four events: every one was continuous load, and the only measure that
has ever been shown to work is the settling pause between stages of e01's benchmark, proved
by one changed variable on a command that had been refused three times. A pause cannot be
given to a compile or to a decision loop, because for the loop the gap between decisions is
part of what is being measured.

`-j3` rather than `-j4` stays, not because three is safe but because it is less load than
four on a supply that is failing, and the same for `--threads 2`.

**Where e02's second number stands, Sunday 18 October 2026.** The pair exists. Both halves
were taken that evening in one sitting on one supply at two threads: restored prompt 1325.6 ms
against 5927.5 ms cold over the first hundred rows, a ratio of **4.47** against an acceptance
of at least 2.0. It is recorded in
`evidence/2026-10-18_2208_pair_pi4b-eplepi_qwen2.5-0.5b-instruct-q4_k_m.md`, together with the
discarded single cold pass earlier the same evening in
`evidence/2026-10-18_2133_cold-pass_pi4b-eplepi_qwen2.5-0.5b-instruct-q4_k_m.md`.

**It is not accepted, and the reason is one bit.** `get_throttled` read `0x0` before the pair
and `0x80000` after, bit 19, the soft temperature limit since boot, where the acceptance asks
for `0x0` at both ends. Bits 16, 17 and 18 stayed clear across the whole thirty minutes: no
under-voltage, no frequency capping, no throttling. So the chapter has a decision to make,
which belongs to it and not to this sheet: cool the board until a pair comes back clean, or
separate bit 19 from bits 16, 17 and 18 in the acceptance and require a clock trace with any
run that carries it. Until then the clause reads measured at 4.47 and not accepted.

**The supply, identified on Sunday 18 October 2026.** All four restarts happened with the
board fed through an ISY IAD-1025-1 USB-C dock, and the topology of those two days, meaning
which plug entered the Pi's own USB-C socket, is not confirmed. Sunday 18 October 2026 ran on
an ISY IAC-5065 65 W USB-C charger whose label gives its 5 V rail as 5.0 V at 3.0 A: the
current is adequate, the voltage is 5.0 V where a Raspberry Pi 4 is specified at 5.1 V. The
board held thirteen minutes of continuous load on it where three minutes had been enough to
lose the run on the dock, which makes the dock the leading explanation and not the established
cause. A Raspberry Pi 4 on four cores wants 5.1 V at 3 A over USB-C.

**What a cooled sitting needs.** Reboot first, because bit 19 is sticky until boot and a run
started on a dirty register cannot satisfy the acceptance however well it goes. Then both
passes back to back on the one supply, restore first and cold second, reading `get_throttled`
before the first and after the second.

**Run the sampler alongside the passes.** Spot checks cannot give the cost of bit 19, because
the limit toggles through a run and nothing was sampling how long it was engaged.
`tools/throttle_log.sh` writes the time, `get_throttled`, `measure_clock arm` and
`measure_temp` to a CSV once a second, which turns that from an argument into a column.

**It has never been run, so smoke-test it first, for five seconds.** A remedy named in a sheet
is not a remedy until it has executed once, and this one was written on a laptop that has no
`vcgencmd`:

    cd ~/edge-llm-pi/projects/e02-decision-loop
    ../../tools/throttle_log.sh /tmp/smoke.csv & sleep 5; kill %1; cat /tmp/smoke.csv

Five lines after the header, no empty fields, and a sample count on stderr means it works on
this board. Then the real trace, started before the restore pass and stopped after the cold
one, so both halves share one record:

    cd ~/edge-llm-pi/projects/e02-decision-loop
    ../../tools/throttle_log.sh evidence/e02_throttle_pair.csv &
    # ... the two passes ...
    kill %1

It refuses to overwrite an existing file, so each sitting needs its own name. And its own cost
is unmeasured: three `vcgencmd` calls a second on a board already at its thermal ceiling is not
obviously free, so read the temperature column of a trace taken on an idle board before
trusting one taken alongside a run. The instrument is the first suspect.

**The thermal numbers to design against, measured Sunday 18 October 2026 on a bare board.**
Idle sits at or below 53 degrees C and falls further. The restore pass took five minutes to
reach 74.5 degrees C with the register still clean. The cold pass started at 56.9 degrees C,
passed 75 degrees C inside two minutes, and settled between 80 and 83 degrees C for the rest
of its twelve and a half minutes, with bit 3 toggling throughout. Once the load stopped the
board fell from 80.3 to 67.2 degrees C in about forty seconds, so the cooling is adequate at
idle and not under sustained load.

The missing build target was found by reading e02 ahead of its sitting rather than during
one, on Saturday 17 October 2026, which is the cheaper of the two places to find it. The
three defects in the comparison commands were found the expensive way, during the sitting.
The build command itself is unchanged: it is right for CI, and the bench needs one line
more.

## config.txt

No overlay and no line is added for e02.
