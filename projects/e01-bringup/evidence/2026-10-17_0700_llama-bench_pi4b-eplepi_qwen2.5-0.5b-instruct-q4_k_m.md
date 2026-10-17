# llama-bench on pi4b-eplepi, Saturday 17 October 2026, 07:00

- model: qwen2.5-0.5b-instruct-q4_k_m.gguf
- kernel: Linux 6.18.50+rpt-rpi-v8 aarch64
- command: build/bin/llama-bench -m /home/bing/models/qwen2.5-0.5b-instruct-q4_k_m.gguf -p 64 -n 32 -t 1,2,3,4 -r 5 -o md
- header: bare header, no HAT
- settling pause between stages: 60 s
- instrument: upstream llama-bench; the volume's harness is priced against this table and published only within 10.0 percent of it

## The register, read at every stage

| moment | vcgencmd get_throttled | in words | temperature |
|---|---|---|---|
| before any load | throttled=0x0 | clean | temp=45.2'C |
| after the reference | throttled=0x0 | clean | temp=72.0'C |
| after settling 60 s | throttled=0x0 | clean | temp=52.1'C |
| after the harness at 3 threads | throttled=0x0 | clean | temp=68.6'C |
| after settling 60 s | throttled=0x0 | clean | temp=50.1'C |
| after the harness at 4 threads | throttled=0x0 | clean | temp=70.6'C |

Read between stages and not only at the ends, because a run refused at the end says the rail sagged without saying when.

| model                          |       size |     params | backend    | threads |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | ------: | --------------: | -------------------: |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       1 |            pp64 |          6.89 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       1 |            tg32 |          4.93 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            pp64 |         13.63 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            tg32 |          8.55 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       3 |            pp64 |         20.05 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       3 |            tg32 |          9.53 +/- 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            pp64 |         25.61 +/- 1.64 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            tg32 |          8.72 +/- 1.09 |

build: d81235049 (11429)

## The harness, same board, same sitting, same model file

    build/projects/e01-bringup/e01_bench_harness --model /home/bing/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --prompt projects/e01-bringup/prompt.txt -p 64 -n 32 -t 3 -r 5

| model | size | params | backend | threads | test | t/s |
| ----- | ---- | ------ | ------- | ------- | ---- | --- |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | pp 64 | 19.97 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | tg 32 | 9.24 +/- 0.00 |

harness: qwen2.5-0.5b-instruct-q4_k_m.gguf, 5 repeats, prompt file projects/e01-bringup/prompt.txt
VmRSS kB 542628
system: CPU : NEON = 1 | ARM_FMA = 1 | LLAMAFILE = 1 | OPENMP = 1 | REPACK = 1 |

## The harness, same board, same sitting, same model file

    build/projects/e01-bringup/e01_bench_harness --model /home/bing/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --prompt projects/e01-bringup/prompt.txt -p 64 -n 32 -t 4 -r 5

| model | size | params | backend | threads | test | t/s |
| ----- | ---- | ------ | ------- | ------- | ---- | --- |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | pp 64 | 24.93 +/- 1.80 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | tg 32 | 8.96 +/- 0.74 |

harness: qwen2.5-0.5b-instruct-q4_k_m.gguf, 5 repeats, prompt file projects/e01-bringup/prompt.txt
VmRSS kB 542648
system: CPU : NEON = 1 | ARM_FMA = 1 | LLAMAFILE = 1 | OPENMP = 1 | REPACK = 1 |

## The division

| threads | quantity | reference | harness | harness minus reference |
|---|---|---|---|---|
| 3 | pp 64 | 20.05 | 19.97 | -0.40 percent |
| 3 | tg 32 | 9.53 | 9.24 | -3.04 percent |
| 4 | pp 64 | 25.61 | 24.93 | -2.66 percent |
| 4 | tg 32 | 8.72 | 8.96 | +2.75 percent |

**Verdict: PASS: the harness tg 32 at 3 threads is -3.04 percent against the reference, band 10.0 percent.**

The gate is the tg 32 comparison at 3 threads. Other thread counts are measured and reported here and do not decide, because a band is only meaningful around a figure that repeats.

## What this run was for, added by hand after it finished

Everything above was written by `tools/bench_runner.py`. This section was added afterwards
and is the only part of the file a person wrote.

This is the control for the brownout of the early hours. The identical command, on the
identical board with the identical binaries and a bare header, was refused three times
between about 00:10 and 00:30 with `throttled=0x50000`. One variable changed: the runner now
waits `--settle` seconds between stages. It passed.

The temperature column is the mechanism, and it is the reason to read the register between
stages rather than only at the ends. Each pause sheds about twenty degrees, so every stage
begins in the low fifties and peaks near seventy, and the board never goes past 72.0 C. Run
back to back the harness stages would have started from 72 C instead of 51 C and climbed
from there, into the band below the 80 C soft limit where the draw is greatest and nothing
yet tells the processor to slow down. Bit 19 was never set in any of the refusals, which is
how it was known to be voltage rather than heat; heat is what carries the board to the
voltage.

**This does not mean the supply is adequate.** It means a pause keeps the load inside what
the supply can hold. The board has still rebooted unexplained during a four-core compile,
and that has no settling pause available to it. The supply remains unidentified and is
still worth replacing: a Raspberry Pi 4 on four cores wants 5.1 V at 3 A over USB-C.

### Three runs of the reference, and what they say about the gate

| threads | Friday 19:01 | Saturday 06:31 | Saturday 07:00 | spread |
|---|---|---|---|---|
| tg 32 at 3 | 9.47 | 9.51 | 9.53 | 0.63 percent |
| pp 64 at 3 | 20.04 | 19.88 | 20.05 | 0.85 percent |

The harness-to-reference comparison that the gate is drawn around, across those same three
runs: **-2.22, -2.63, -3.04 percent at 3 threads**, a range of 0.82 points, against
**+5.52, -5.31, +2.75 percent at 4 threads**, a range of 10.83 points. The quantity the gate
measures is thirteen times more stable at three threads than at four, on a board whose own
figures moved by under one percent. The gate was moved there on Friday from a single day's
standard deviations; three runs have now said the same thing.
