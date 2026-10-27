# e03 wiring: three LED modules on the bare header of the Pi 3 Model B

Project e03 lights one of three Joy-it LK-LED10 modules, green, yellow and red, from the
rule of `src/rules_e03.c`, and asks the model for the sentence that explains the colour.
The board is the Raspberry Pi 3 Model B Rev 1.2 (1 GB) with the card `eplepi` (Raspberry
Pi OS, Debian 13 trixie, user `bing`), reached over SSH. No HAT is seated. The event
stream is the replayed CSV `replay/state_200.csv`; the LEDs are the only live part, and
they are opened by the project's own `--leds` option, not by `--live`, because the gpio
backend's events are the line read-backs and not a level stream.

The modules carry the anode on the signal pin and light when the line is driven high.
Measured with the nRF PPK2 on Saturday 3 October 2026 at 3.3 V: three together 16.5 mA;
per module green 7.39 mA, yellow 6.20 mA, red 6.34 mA. The loop lights one line at a
time, so one BCM line sources at most the green figure. The fourth module, blue, stays
in the box.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 11 | GPIO17 | green module, S | **yellow** |
| 13 | GPIO27 | yellow module, S | **yellow** |
| 15 | GPIO22 | red module, S | **white** |
| 14 | GND | green module, G | **black** |
| 20 | GND | yellow module, G | **green** |
| 25 | GND | red module, G | **orange** |
| none | | the V pin of each module: left unconnected; the measurement record names the signal pin and 3.3 V across the module, nothing on V (to confirm on the module's silkscreen) | |

Colours recorded from the bench on Tuesday 27 October 2026, with the photograph
`evidence/2026-10-27_1503_wiring_pi4b-eplepi.jpg` taken during that run. They had read
"not recorded" since Saturday 3 October 2026.

**TWO SIGNAL LEADS ARE THE SAME COLOUR, and that is not a detail.** GPIO17 and GPIO27
both carry a yellow wire, one to the green module and one to the yellow module. On
Tuesday 27 October 2026 the three modules were wired one position out, and driving each
line by hand before the run gave yellow on GPIO17, red on GPIO27 and nothing on GPIO22.
Two identical yellow leads is the mechanism. If a third jumper colour is to hand, use it
for one of them; otherwise the per-line check below is the only thing standing between a
swapped lead and a published agreement figure, because a swapped lead does not fail
loudly, it returns a wrong colour agreement, and a wrong agreement looks exactly like a
wrong model.

Each module takes its own ground rather than three leads sharing pin 25, which is how
\ref{sec:e09} already wires them and avoids a three-way splice on one header pin. Pins 1
and 17 (3V3) stay unconnected: the lines are driven by the SoC, not by a rail.

## The one-HAT rule for this Pi

The Pi 3B header is bare for e03. The ADXL345 (e05) and the VL53L4CD (e06) can share
the header with the three modules, since they use pins 1, 3, 5, 6, 9, 16 and 17 and
e03 uses 11, 13, 15 and 25, so all three projects can be wired in one sitting.

Neither HAT for this board can join them. The SIM7020E of e16 covers the whole header:
the modules come off before that HAT goes on. The 3.5 inch LCD of e09 is the same answer
for a different reason, and an earlier version of this paragraph had it backwards: the
panel's socket mates with pins 1 to 26 and its carrier buries 27 to 40 without contacts,
so a seated panel leaves nothing reachable, and e09 hangs it on jumpers instead and moves
the green LED off pin 11, which is the panel's touch interrupt. e09 is therefore a
separate sitting with its own wiring, not these three with a panel added.

## Pre-flight, on the Pi 3B over SSH

The card is the hostname: `ssh bing@eplepi` after the card has moved from the Pi 4B to
this board. Memory first, then the GPIO character device, then one module by hand.

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    ls -l ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf
    gpiodetect
    gpioinfo -c gpiochip0 17 27 22
    groups

`gpiodetect` must list `gpiochip0`; the three lines must read `unused` (a line held by
another consumer refuses the request); `groups` must include `gpio`, or the request is
refused with permission denied. Then blink the green module for a few seconds and stop
with Ctrl-C (`gpioset` and `gpioinfo` from the `gpiod` 2.2.1 package on trixie; the
exact option spelling is read from `gpioset --help` on the card, to verify):

    gpioset -c gpiochip0 -t 500ms 17=1

The model file must fit under 60 percent of `MemAvailable` or the run is SD-card paging,
not a sentence latency; 0.27 GB needs 450 MB available on this 1 GB board.

## Build on the board

    sudo apt install libgpiod-dev gpiod
    cd ~/edge-llm-pi
    cmake -S . -B build -DGGML_NATIVE=ON -DEDGELLM_IO_GPIO=ON -DEDGELLM_BUILD_TESTS=OFF
    cmake --build build -j2 --target edgellm-e03

`-j2`, not `-j4`: a 1 GB board swaps at four parallel compiles of the engine tree.

## The run

    cd ~/edge-llm-pi
    vcgencmd get_throttled
    build/projects/e03-led-explainer/edgellm-e03 --project e03 \
      --replay projects/e03-led-explainer/replay/state_200.csv \
      --leds /dev/gpiochip0:17,27,22 \
      --grammar projects/e03-led-explainer/grammar.gbnf \
      --prompt projects/e03-led-explainer/prompt.txt \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
      --n-predict 64 --temperature 0.3 --threads 4 \
      --state /tmp/e03.state \
      --log projects/e03-led-explainer/evidence/e03_run.csv
    vcgencmd get_throttled

All three lines start low (the gpio backend requests every output inactive) and end low
(main drives them low before it returns). Add `--interval 10` for a demonstration where
the LED is visible for ten seconds per event; the acceptance run uses no interval.

## Acceptance

    build/tests/oracle_check projects/e03-led-explainer/evidence/e03_run.csv \
      tests/oracle/e03.expected.csv
    python projects/e03-led-explainer/tools/reason_check.py \
      projects/e03-led-explainer/evidence/e03_run.csv

The oracle check compares the rule column byte for byte and reports every row where the
real model's object differs from the stub's; what the chapter publishes is the checker's
two fractions over the 200 rows, the colour agreement and the share of reasons that
name the field the rule triggered on, plus the count of rows with no parseable object,
which must be zero. The pass fraction for the reason is fixed after a pilot of the first
20 events and the full 200 are published either way. Throttle bits `0x0` before and after.

## NanoPi NEO Air variant

Same binary, cross built armv7 in skyhorizon's WSL as in e01, model
`SmolLM2-135M-Instruct-Q4_K_M.gguf` (105 MB; 512 MB total on this board), threads from
e01's NanoPi row. The 24-pin header has no 40-pin layout and its GPIO numbering is the
Allwinner one (PAn, PGn); the three line offsets and the ground pin are read from the
board's silkscreen and the `gpioinfo` listing on the board before any module is
connected, and the spec becomes `/dev/gpiochip0:<a>,<b>,<c>`. Nothing about those
offsets is claimed here: to confirm on the silkscreen.

## config.txt

No overlay and no line is added for e03: `gpiochip0` exists from the kernel at boot.
