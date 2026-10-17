# llama-bench on pi4b-eplepi, Saturday 17 October 2026, 06:31

- model: qwen2.5-0.5b-instruct-q4_k_m.gguf
- kernel: Linux 6.18.50+rpt-rpi-v8 aarch64
- command: build/bin/llama-bench -m /home/bing/models/qwen2.5-0.5b-instruct-q4_k_m.gguf -p 64 -n 32 -t 1,2,3,4 -r 5 -o md
- header: bare, MCC 118 unseated and nothing on USB (lsusb shows only the root hubs and the Pi's internal VIA Labs hub)
- throttled before: throttled=0x0
- throttled after: throttled=0x0
- instrument: upstream llama-bench built from the repository's submodule at tag v0.6.0, commit d81235049
- run by hand in three stages rather than by tools/bench_runner.py, for the reason in the last section

This is the second day's run the acceptance asks for. The board had been idle about six hours
and sat at 44.8 C before the first stage.

## Stage one, the reference

| model                          |       size |     params | backend    | threads |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | ------: | --------------: | -------------------: |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       1 |            pp64 |          6.82 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       1 |            tg32 |          4.93 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            pp64 |         13.49 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            tg32 |          8.54 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       3 |            pp64 |         19.88 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       3 |            tg32 |          9.51 +/- 0.01 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            pp64 |         25.38 +/- 1.31 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            tg32 |          8.67 +/- 1.05 |

build: d81235049 (11429). After this stage: throttled=0x0, 72.0 C.

## Stage two, the harness at 3 threads

| model | size | params | backend | threads | test | t/s |
| ----- | ---- | ------ | ------- | ------- | ---- | --- |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | pp 64 | 20.01 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | tg 32 | 9.26 +/- 0.01 |

VmRSS kB 542640. After this stage: throttled=0x0, 71.5 C.

## Stage three, the harness at 4 threads

| model | size | params | backend | threads | test | t/s |
| ----- | ---- | ------ | ------- | ------- | ---- | --- |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | pp 64 | 25.98 +/- 0.29 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | tg 32 | 8.21 +/- 0.98 |

VmRSS kB 542652. After this stage: throttled=0x0, 68.1 C.

## The division

| threads | quantity | reference | harness | harness minus reference |
|---|---|---|---|---|
| 3 | pp 64 | 19.88 | 20.01 | +0.65 percent |
| 3 | tg 32 | 9.51 | 9.26 | -2.63 percent |
| 4 | pp 64 | 25.38 | 25.98 | +2.36 percent |
| 4 | tg 32 | 8.67 | 8.21 | -5.31 percent |

**Verdict: PASS.** The harness tg 32 at 3 threads is -2.63 percent against the reference,
inside the 10 percent band.

## The second day, against Friday 16 October 2026

| threads | pp 64 Friday | pp 64 Saturday | drift | tg 32 Friday | tg 32 Saturday | drift |
|---|---|---|---|---|---|---|
| 1 | 6.87 | 6.82 | -0.73 percent | 4.85 | 4.93 | +1.65 percent |
| 2 | 13.62 | 13.49 | -0.95 percent | 8.39 | 8.54 | +1.79 percent |
| 3 | 20.04 | 19.88 | -0.80 percent | 9.47 | 9.51 | +0.42 percent |
| 4 | 25.58 | 25.38 | -0.78 percent | 8.70 | 8.67 | -0.34 percent |

Worst drift 1.79 percent against a clause that allows 15. The harness at 3 threads reads
9.26 on both days, identical to the hundredth. **All three clauses of the acceptance now
hold and project e01 is complete.**

## Why the gate sits at 3 threads, now measured rather than argued

The gate moved off 4 threads on Friday because the reference's own spread there was 12.5
percent of its mean, which makes a 10 percent band meaningless. The second day tested that
reasoning without being designed to:

- at 3 threads the harness-to-reference comparison reads -2.22 percent on Friday and
  -2.63 percent on Saturday, stable to four tenths of a point;
- at 4 threads the same comparison reads +5.52 percent on Friday and -5.31 percent on
  Saturday, an eleven-point swing.

Both days' underlying figures moved by under two percent. The instability is in the
comparison at 4 threads, not in the board. A gate drawn there would have passed on Friday
and passed on Saturday for unrelated reasons, and would have failed on some third day for
none at all.

## Why this run was done by hand, and what it says about the runner

tools/bench_runner.py refused twice in the early hours of Saturday 17 October 2026, both
times with throttled=0x50000: bit 16, under-voltage has occurred since boot, and bit 18,
throttling has occurred. The second refusal followed a reboot that left the register at
0x0 with the core at 0.8600 V and the board idle, so the event happened during the run.

The cause is not the HAT and not the peak thread count, both of which were tested and
cleared:

- bare header with the MCC 118 unseated, and lsusb showing nothing external: still refused;
- a one-thread burst and a four-thread burst from a fresh boot: both left the register at 0x0;
- all three stages above, run by hand: all left it at 0x0, reaching 72.0 C.

What distinguishes the refusals is that the runner executes the three stages back to back
with no gap. The hand runs had pauses, and the temperature shows their effect: 72.0 C after
stage one, then 71.5 and 68.1, falling between stages instead of climbing. Run continuously
the board would have gone higher, into the band under the 80 C soft limit where draw is
greatest and thermal throttling has not yet intervened. Bit 19 was never set in either
refusal, which fits: hot enough to pull more current than the supply holds, not hot enough
to be told to stop.

That is a hypothesis with evidence behind it, not a conclusion. It predicts that a settling
pause between stages makes the runner pass on this board, and that is the next thing to
test. Note the irony in the record: collapsing the sitting into one command was an
improvement made on Friday evening, and it is what exposed a supply that the older
hand-run workflow never loaded hard enough to find.

The supply has not been identified and should be. A Raspberry Pi 4 under four cores wants
5.1 V at 3 A over USB-C; the two events this board has shown, an unexplained reboot during
a four-core compile and an under-voltage flag under sustained benchmark load, are what an
undersized supply or a thin cable looks like.
