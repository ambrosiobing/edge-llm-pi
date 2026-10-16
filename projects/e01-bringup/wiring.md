# e01 wiring: four boards, no HAT, two console routes

Project e01 is the instrument. Nothing is wired to a sensor and no HAT is seated on any board
during a timing run; a HAT that was left seated from another project stays electrically
idle, but the honest run is on a bare header, so unseat it and say so in the evidence file.

## Boards and cards

| Board | RAM | Card | Console route |
|---|---|---|---|
| Raspberry Pi 4 Model B | not recorded, e01 reads it with `free -m` | `eplepi` (Raspberry Pi OS, Debian 13 trixie, kernel 6.18.50+rpt-rpi-v8, user `bing`) | Renkforce USB/TTL serial at 115200 on header pins 6, 8, 10, or SSH |
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

    python tools/bench_runner.py --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --board pi4b-eplepi --evidence projects/e01-bringup/evidence \
      --llama-bench build/llama.cpp/bin/llama-bench
    build/projects/e01-bringup/e01_bench_harness \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --prompt projects/e01-bringup/prompt.txt -p 64 -n 32 -t 4 -r 5

The runner executes `llama-bench -p 64 -n 32 -t 1,2,3,4 -r 5 -o md` with the throttle gate
before and after and writes the table into a dated file under `evidence/`; the harness
table, its VmRSS line and its system line are pasted into the same file by hand, and the
division is written beside them. Pass: the harness tg within 10 percent of llama-bench tg
at 4 threads; throttle bits `0x0` before and after; a second day's run within 15 percent
of the first.

## config.txt

No overlay. `enable_uart=1` on the Pi 4B for the serial route; nothing else.
