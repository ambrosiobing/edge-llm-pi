# e15 wiring: the ESP32 on the PPK2, the LED on GPIO4, the Pi over one serial cable

Project e15 runs on a Raspberry Pi 4 Model B (or the Pi 3 Model B, same wiring, card
`eplepi`, Raspberry Pi OS, user `bing`) with nothing on the 40-pin header. Everything
else is on the sidecar side of two USB cables: the nRF PPK2 and the Renkforce USB/TTL
cable, both into the Pi's USB-A ports. The SBC-NodeMCU-ESP32 is powered ONLY by the
PPK2 in source-meter mode at 3.3 V; its own USB connector stays empty during every
capture and every run, because the Pi's 5 V would otherwise feed the board a second
way and the PPK2 would no longer be the only supply (kit lab P10, the two supplies
trap).

## Pin table, Pi side

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | nothing on the header; both instruments are USB |

## Pin table, ESP32 side

| ESP32 pin | Signal | Goes to | Wire colour |
|---|---|---|---|
| 3V3 | supply, 3.3 V | PPK2 VOUT (source-meter mode, 3300 mV) | not recorded; label it |
| GND | ground | PPK2 GND, the cable's GND, the LED module's GND | not recorded; connect first |
| GPIO17 | UART2 TX | Renkforce cable RX | not recorded; read the plug label |
| GPIO16 | UART2 RX | Renkforce cable TX | not recorded; read the plug label |
| GPIO4 | LED, driven high to light | LK-LED10 red module, signal pin (anode) | not recorded |
| micro-USB | nothing during a run | flashing only, in skyhorizon's WSL | the cable stays in the drawer |

The UART2 pin numbers are set explicitly by `uart_set_pin()` in the firmware, so they
hold whatever the board's default mapping is; the silkscreen labels of GPIO16 and
GPIO17 on the Joy-it board are read before wiring (to verify against the Joy-it manual).
The red module is the one of the four LK-LED10 modules that e03 does not use (blue,
green and yellow sit on the Pi 3B's BCM 17, 27, 22 there); its figure at 3.3 V is
6.34 mA (PPK2, Saturday 3 October 2026). GPIO0, 2, 5, 12 and 15 are strapping pins and
carry nothing here.

## The one-HAT rule for this Pi

The header is free. If the MCC 118 (card A) or the WS-28164 (card B) is seated from a
previous sitting it may stay: e15 touches no header pin. Either card serves as long as
the model file is on it.

## The PPK2

Source-meter mode, 3300 mV, set by `tools/measure_patterns.py` before the DUT is
enabled (and by hand in nRF Connect Power Profiler for a look at the trace). The PPK2
sources up to about 1 A, which covers an ESP32 with Wi-Fi and Bluetooth off (the
firmware never starts either). Ampere-meter mode is the wrong mode here: it would put
the instrument in series with a supply the ESP32 does not have. Nordic's guide names a
second USB cable for the PPK2 above a few hundred milliamps of sourced current (to
verify); the ESP32 in this firmware stays well below that.

## Pre-flight, on the Pi over SSH

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    ls -l /dev/serial/by-id/
    ls -l ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf
    python -c "import llama_cpp, serial, ppk2_api; print(llama_cpp.__version__)"

`/dev/serial/by-id/` lists the Renkforce cable and the PPK2 under their USB identities;
the cable is a `ttyUSB` device and the PPK2 a `ttyACM` device (to verify on the first
plug-in), and only the by-id paths are used in commands, never the bare numbers, which
change with plug-in order. `get_throttled` must read `0x0` before and after the run.

## The measurement, first

    cd ~/edge-llm-pi
    python projects/e15-esp32-sidecar/tools/measure_patterns.py \
      --serial /dev/serial/by-id/<the Renkforce cable> \
      --out projects/e15-esp32-sidecar/evidence

Writes `evidence/pattern_costs.csv` (every capture) and `evidence/pattern_costs_summary.csv`
(mean and spread per pattern) and prints PASS when every pattern's spread over the
three runs is within 5 percent. Then the replay is regenerated from the measured means:

    python projects/e15-esp32-sidecar/tools/make_replay.py

## The run

    cd ~/edge-llm-pi
    edgellm-e15 --project e15 \
      --n-predict 16 \
      --replay projects/e15-esp32-sidecar/replay/budgets_200.csv \
      --grammar projects/e15-esp32-sidecar/grammar.gbnf \
      --prompt projects/e15-esp32-sidecar/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --log projects/e15-esp32-sidecar/evidence/e15_run.csv \
      --serial /dev/serial/by-id/<the Renkforce cable>
    vcgencmd get_throttled

**`--n-predict 16`, and why it is on the bench command and not only in the unit.** The core's
default budget is 8. That number is what truncated 949 objects out of 1000 in e02 on Saturday
17 October 2026, and 64dfa5c raised this project's budget to 16 in its systemd unit for exactly
that reason. The unit is the Yocto deployment; the block above is what a person pastes on the
bench, and until Monday 19 October 2026 it passed no budget at all and therefore ran at 8. The
same gap was found and closed in e02's sheet the same day, which makes three projects where the
remedy reached the unit and missed the command.

It would have been caught rather than published, because `tools/agreement.py` returns non-zero
on any row whose object did not close and lists them. What it would have cost is the sitting:
this run is not a thing to repeat casually with the hardware mounted. Sixteen is generous on
purpose, since an unused token is free, the loop stops at the end-of-generation token and the
root ends at the closing brace. The `n_generated` column of the first real log says how many
were actually spent, and that column settles it rather than this paragraph.

## Acceptance

    python -m pytest projects/e15-esp32-sidecar
    python tools/agreement.py projects/e15-esp32-sidecar/evidence/e15_run.csv

The first is the oracle (stub) and the rule cases; the second scores the bench log.
What passes the chapter: every pattern's spread within 5 percent in the summary file,
and zero rows in the bench log whose model column is not one of the four literals.

## config.txt

No overlay and no line is added for e15.
