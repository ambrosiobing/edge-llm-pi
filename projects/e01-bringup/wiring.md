# e01 wiring: four boards, no HAT, two console routes

Project e01 is the instrument. Nothing is wired to a sensor and no HAT is seated on any board
during a timing run; a HAT that was left seated from another project stays electrically
idle, but the honest run is on a bare header, so unseat it and say so in the evidence file.

## Boards and cards

| Board | RAM | Card | Console route |
|---|---|---|---|
| Raspberry Pi 4 Model B Rev 1.4 | 8 GB; read on Friday 16 October 2026 as MemTotal 8,007,396 kB and MemAvailable 7,789,840 kB after boot, four cores | `eplepi` (Raspberry Pi OS, Debian 13 trixie, kernel 6.18.50+rpt-rpi-v8, user `bing`) | Renkforce USB/TTL serial at 115200 on header pins 6, 8, 10, or SSH |
| Raspberry Pi 3 Model B Plus | 1 GB | `eplepi` when moved, or the second card | SSH only; the serial console on this board is dead (GPIO14 is never muxed because `disable-bt` leaves `uart0_pins` empty) |
| Raspberry Pi 3 Model B Rev 1.2 | 1 GB | the second card | SSH, or the Renkforce cable on pins 6, 8, 10 (to confirm on this board) |
| NanoPi NEO Air | 512 MB, Allwinner H3, armv7 | 8 GB eMMC | SSH over Wi-Fi; no 40-pin header, no `vcgencmd` |

Hostnames are card identities, not board identities. Every evidence file names the board and
the card in its first line.

## The Renkforce serial route on the Pi 4B

| Header pin | BCM | Cable lead | Signal |
|---|---|---|---|
| 6 | GND | GND | common ground, connect first |
| 8 | GPIO14 | RX of the cable | Pi TXD |
| 10 | GPIO15 | TX of the cable | Pi RXD |

Lead colours are not recorded for this cable; read the label on the plug. Pin 2 and pin 4
(5 V) stay unconnected: the Pi is powered from its own USB-C supply. `config.txt` needs
`enable_uart=1`. 115200, 8N1, no flow control.

## Pre-flight, every board, before any timing run

    free -m
    grep -E 'MemTotal|MemAvailable' /proc/meminfo
    vcgencmd get_throttled
    vcgencmd measure_temp
    nproc
    cat /proc/cpuinfo | grep -m1 -E 'model name|Hardware'

`get_throttled` must read `0x0` before and after the run; anything else discards the run.
The model file must be under 60 percent of `MemAvailable` or the figure is SD-card paging.
On the NanoPi there is no `vcgencmd`; read `/sys/class/thermal/thermal_zone0/temp` and
`/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq` before and after instead.

## Post-flight, every board, right after the last repeat

    vcgencmd get_throttled
    vcgencmd measure_temp

## The acceptance command

    python3 tools/bench_runner.py \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --board pi4b-eplepi --evidence projects/e01-bringup/evidence \
      --llama-bench build/bin/llama-bench \
      --harness build/projects/e01-bringup/e01_bench_harness \
      --prompt projects/e01-bringup/prompt.txt \
      --hat "bare header, no HAT"

One command, and it exits non-zero if the run does not pass. It reads the throttle register,
runs the reference at 1, 2, 3 and 4 threads, runs the harness at 3 and 4, reads the register
again, writes the dated evidence file with both tables and the division between them, and
prints the verdict. `--hat` is not decoration: `wiring.md` asks for a bare header or an
explicit note, and on the first run nobody wrote it down, so the runner now asks at the only
moment anyone is looking at the board.

`python3`, not `python`, and the reason is not the one first written here. Bare `python`
**does** work on this card: `python-is-python3` 3.13.3-1 is installed and symlinks it, checked
on Friday 16 October 2026. The original command's `python` would have run. `python3` stays
because it is the spelling that cannot be wrong anywhere, while `python` depends on a package
that a Lite image or another card need not carry, and the other three boards in the table have
not been checked. The thirty or so bare `python` commands in the other projects' wiring files
are therefore left alone: they work on this card, they are unverified on the rest, and
rewriting eighteen files on a guess is worse than knowing which guess you are making.
The llama-cpp tools land in `build/bin/`, because llama.cpp's own CMakeLists sets
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` to `${CMAKE_BINARY_DIR}/bin` and `CMAKE_BINARY_DIR` is the
top-level build directory even when llama.cpp is added as a subdirectory. The path
`build/llama.cpp/tools/llama-bench` is the build tree's mirror of the source folder of that
name, a directory, and handing it to the runner gives a `PermissionError` on exec.

The runner executes `llama-bench -p 64 -n 32 -t 1,2,3,4 -r 5 -o md` with the throttle gate
before and after and writes the table into a dated file under `evidence/`, then the harness
at each of `--harness-threads` with its own table, VmRSS line and system line, then the
division and the verdict. None of that is transcribed by hand any more. It was on the first
run, on Friday 16 October 2026, which meant eight figures copied twice and the arithmetic
that decides pass or fail done in someone's head; the parser and the gate now have their own
tests under `tools/tests`, with the first real run as the fixture and a deliberately wrong
harness figure as the case that must fail. Pass: the harness tg within 10 percent of
llama-bench tg
**at 3 threads**; throttle bits `0x0` before and after; a second day's run within 15 percent
of the first. The 4-thread pair is recorded beside it and is not the gate.

The gate moved from 4 threads to 3 on Friday 16 October 2026, on the evidence of the first
run rather than on taste. At 4 threads llama-bench's own tg spread on this board was
8.70 plus or minus 1.09 tokens per second, 12.5 percent of its mean over five repeats, so a
band of 10 percent drawn around that reference can be crossed or not crossed by chance and
decides nothing. At 3 threads both instruments repeat to a hundredth of a token per second,
9.47 plus or minus 0.01 for llama-bench and 9.26 plus or minus 0.01 for the harness, and the
comparison means what it says. A gate is only as good as the stability of the number it is
drawn around; the right reference point is the quietest one, not the fastest.

## What the first run measured, Friday 16 October 2026

Raspberry Pi 4 Model B Rev 1.4, card `eplepi`, Qwen2.5-0.5B-Instruct Q4_K_M, llama.cpp at
tag v0.6.0, commit `d81235049`, built on the board with the default `-mcpu=cortex-a72+crc`.
Throttle `0x0` before and after, 34.5 C at idle.

| threads | pp 64, llama-bench | pp 64, harness | tg 32, llama-bench | tg 32, harness |
|---|---|---|---|---|
| 1 | 6.87 +/- 0.00 | not run | 4.85 +/- 0.00 | not run |
| 2 | 13.62 +/- 0.00 | not run | 8.39 +/- 0.00 | not run |
| 3 | 20.04 +/- 0.01 | 19.99 +/- 0.01 | 9.47 +/- 0.01 | 9.26 +/- 0.01 |
| 4 | 25.58 +/- 1.57 | 24.70 +/- 1.74 | 8.70 +/- 1.09 | 9.18 +/- 0.13 |

Harness resident set 542,648 kB, about 530 MiB, for a 463 MiB model file. Prompt processing
scales 3.72 times over four cores; generation never reaches twice, peaks at three threads and
falls back at four. Both instruments show that bend independently, so it belongs to the board
and not to either tool. The second day's run is still owed.

## config.txt

No overlay. `enable_uart=1` on the Pi 4B for the serial route; nothing else.
