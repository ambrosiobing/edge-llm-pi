# e15 sidecar firmware

One ESP-IDF project, `main/sidecar.c`: one LK-LED10 module on GPIO4 through LEDC, four
patterns A to D plus O (off) selected by one letter on UART2 (GPIO17 TX, GPIO16 RX,
115200 8N1), every command echoed back as `pattern X`. No Wi-Fi, no Bluetooth.

## ESP-IDF version

The API names in `sidecar.c` were read from the ESP-IDF v6.1 stable reference pages
(LEDC, UART, GPIO) on Wednesday 14 October 2026. The version installed on skyhorizon's
WSL is not recorded yet: write it here from `idf.py --version` at the first build
(to verify). The target is `esp32`; a build for another chip is not attempted.

## Build, flash and monitor, in skyhorizon's WSL

The ESP-IDF environment is activated the way the installer left it (an activation
script under `~/.espressif` for v6.x installs, `export.sh` for older ones; to verify on
that machine). Then, from the repository clone there:

    cd ~/src/edge-llm-pi/projects/e15-esp32-sidecar/firmware
    idf.py set-target esp32
    idf.py build

Flashing goes through the board's own USB connector, which is the one time it may be
plugged in; the ESP32's USB is removed again before the PPK2 run (the two supplies
trap in `../wiring.md`). The USB port is read from `ls /dev/ttyUSB* /dev/ttyACM*`
after `usbipd attach` of the board into WSL:

    idf.py -p /dev/ttyUSB0 flash
    idf.py -p /dev/ttyUSB0 monitor

The monitor shows the build stamp line `e15 sidecar, built <date> <time>, ...` at
boot; a stamp older than the last edit is a stale flash, whatever the file names say.

## Patterns

| Letter | Pattern | On-time | How |
|---|---|---|---|
| A | steady | 100 percent | duty 1023 of 1023 |
| B | slow blink | 50 percent | 1 Hz, 500 ms on |
| C | fast blink | 20 percent | 5 Hz, 40 ms on |
| D | breathing | 50 percent mean | 0.5 Hz triangle of the duty, 0 to 1023 |
| O | off | 0 | duty 0, the PPK2 baseline |

The on-time column is what `edgellm_e15/rules.py` carries as `ON_PCT`; the two must
agree or the rule speaks of a pattern the firmware does not run.
