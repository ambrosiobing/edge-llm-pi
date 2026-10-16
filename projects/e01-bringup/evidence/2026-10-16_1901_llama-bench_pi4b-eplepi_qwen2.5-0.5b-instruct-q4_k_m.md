# llama-bench on pi4b-eplepi, Friday 16 October 2026, 19:01

- model: qwen2.5-0.5b-instruct-q4_k_m.gguf
- kernel: Linux 6.18.50+rpt-rpi-v8 aarch64
- command: build/bin/llama-bench -m /home/bing/models/qwen2.5-0.5b-instruct-q4_k_m.gguf -p 64 -n 32 -t 1,2,3,4 -r 5 -o md
- throttled before: throttled=0x0
- throttled after: throttled=0x0
- instrument: upstream llama-bench; the volume's harness is priced against this table and published only within 10 percent of it

| model                          |       size |     params | backend    | threads |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | ------: | --------------: | -------------------: |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       1 |            pp64 |          6.87 ? 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       1 |            tg32 |          4.85 ? 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            pp64 |         13.62 ? 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       2 |            tg32 |          8.39 ? 0.00 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       3 |            pp64 |         20.04 ? 0.01 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       3 |            tg32 |          9.47 ? 0.01 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            pp64 |         25.58 ? 1.57 |
| qwen2 1B Q4_K - Medium         | 462.96 MiB |   630.17 M | CPU        |       4 |            tg32 |          8.70 ? 1.09 |

build: d81235049 (11429)

Everything above this line is what `tools/bench_runner.py` wrote and is reproduced unchanged.
The question marks are not in llama-bench's output: it prints the spread as U+00B1, and the
runner opened the file as ASCII with `errors="replace"`. Read `?` as "plus or minus". The
runner was changed the same evening to transliterate instead of substitute, so the second
day's file will read `+/-`; this one is left as it was written, because an evidence file is
a record and not a draft.

## A deviation from the chapter's procedure, recorded rather than smoothed over

The chapter builds the reference twice on purpose. Steps 3 to 5 clone llama.cpp on its own
into `~/llama.cpp` at tag v0.6.0 and build `llama-bench` there, because this project's chosen
variant is the upstream tools built as they ship, and step 7 uses the repository's submodule
build for the harness. **This run used the repository's submodule build for both.** There is
no `~/llama.cpp` on the board; the reference binary was `build/bin/llama-bench` from the same
configure that produced the harness.

The two are expected to agree, because the repository changes only which llama.cpp targets
are built and switches off curl and openssl, while the compiler flags ggml settles on are the
native ones either way, and the configure log shows the same
`-mcpu=cortex-a72+crc+nodotprod+noi8mm+nosve+nosme` for this build. Expected is not measured.
Nothing in this file should be read as the upstream-as-shipped figure until somebody builds
the standalone clone and runs the same command against it.

That comparison is worth doing once rather than assumed forever: if the two builds agree
inside the noise, the chapter can prescribe one build and save an hour of compiling and about
450 MB on every board; if they differ, the volume has learned something about its own
reference and this table needs the other build's numbers beside it.

## The board

Raspberry Pi 4 Model B Rev 1.4, card `eplepi`, user `bing`, Raspberry Pi OS Debian 13 trixie.
MemTotal 8,007,396 kB, MemAvailable 7,789,840 kB after boot, four cores. 34.5 C before the
run and 46.7 C after, `throttled=0x0` at both readings and again after the harness runs.
`pgrep` confirmed no compiler was running during the measurement.

The engine is llama.cpp at tag v0.6.0, commit `d81235049384534c167caea52b85a694f6103d14`,
built on this board. Every ARM feature probe failed at configure time and ggml settled on
`-mcpu=cortex-a72+crc+nodotprod+noi8mm+nosve+nosme`: no dot product, no i8mm, no SVE, no SME,
no half-precision vector arithmetic. That is correct for a Cortex-A72 and is the reason a
Raspberry Pi 5 is faster at this work than a clock comparison predicts.

**The header was bare: no HAT seated for this run**, which is what `wiring.md` asks for.
Confirmed by Joseph on Friday 16 October 2026, shortly after the run, from his own
observation of the bench rather than from a note written at the time. The distinction is
worth keeping in a record even when the answer is the expected one. Write it down during the
second day's run instead of afterwards.

## The harness, same board, same sitting, same model file

    build/projects/e01-bringup/e01_bench_harness \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --prompt projects/e01-bringup/prompt.txt -p 64 -n 32 -t 3 -r 5

| model | size | params | backend | threads | test | t/s |
| ----- | ---- | ------ | ------- | ------- | ---- | --- |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | pp 64 | 19.99 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | tg 32 | 9.26 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | pp 64 | 24.70 +/- 1.74 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | tg 32 | 9.18 +/- 0.13 |

    harness: qwen2.5-0.5b-instruct-q4_k_m.gguf, 5 repeats, prompt file projects/e01-bringup/prompt.txt
    VmRSS kB 542648
    system: CPU : NEON = 1 | ARM_FMA = 1 | LLAMAFILE = 1 | OPENMP = 1 | REPACK = 1 |

Resident set 542,648 kB, about 530 MiB, against a 463 MiB model file at `n_ctx` 512. The
model file is 6.16 bits per weight rather than a Q4_K_M's usual 4.8, because Qwen's own GGUF
keeps 133 tensors at q5_0 and 13 at q8_0.

## The division

| threads | quantity | llama-bench | harness | harness minus reference |
|---|---|---|---|---|
| 3 | pp 64 | 20.04 | 19.99 | -0.25 percent |
| 3 | tg 32 | 9.47 | 9.26 | -2.22 percent |
| 4 | pp 64 | 25.58 | 24.70 | -3.44 percent |
| 4 | tg 32 | 8.70 | 9.18 | +5.52 percent |

**Verdict: pass, on the three-thread comparison.** The harness measures what upstream
llama-bench measures, to 2.22 percent on generation and 0.25 percent on prompt processing,
and both instruments repeat to a hundredth of a token per second there.

The four-thread pair also falls inside the 10 percent band, but it should not be the gate and
is no longer written as one. llama-bench's own four-thread generation figure varies by 1.09
tokens per second across its five repeats, 12.5 percent of its mean, so a band of 10 percent
drawn around that reference can be crossed or not crossed by chance. `wiring.md` was amended
the same evening to gate on three threads and report four beside it. The lesson is general: a
gate is only as good as the stability of the number it is drawn around.

## What this run shows about the board

Prompt processing scales 3.72 times over four cores, close to linear. Generation never
reaches twice: 4.85, 8.39, 9.47, 8.70, peaking at three threads and falling back at four.
Both instruments show that bend independently, so it belongs to the board and not to either
tool. Reading a prompt is compute bound and the fourth core helps; writing tokens is bound by
the memory bus and the fourth core does not.

The pi-llm-bench reference the chapter cites bends after **two** threads on a Pi 4B with 4 GB
running TinyLlama-1.1B. Same shape, different knee. The suspect is model size, since a larger
model moves more weight per token and should saturate the bus sooner, and the 8 GB reading
now admits TinyLlama-1.1B on this very board. That run is the control and has not been done:
until it is, the bandwidth explanation is borrowed from someone else's page.

## Still owed

The acceptance also requires a second day's run within 15 percent of this one. Not yet run.
