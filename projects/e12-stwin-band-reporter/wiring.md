# e12 wiring: USB-C only, nothing on the header

Project e12 reads the STEVAL-STWINBX1 STWIN.box over its USB-C connector and shows the
report on the official DSI 7 inch touchscreen. No HAT is seated and nothing is read from
the 40-pin header; the panel sits on the DISPLAY connector, not on the header, which is
what lets the two coexist. The board is the Raspberry Pi 4 Model B with the card `eplepi`
(Raspberry Pi OS, Debian 13 trixie, user `bing`); either card is fine as long as the model
file is on it.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | none; the probe is USB-C to USB-A and the panel is the DSI ribbon |

## Connections

| Connection | Connector | Note |
|---|---|---|
| STWIN.box to Pi 4B | USB-C on the probe to a USB-A port on the Pi | data and 5 V bus power on one short cable, strain-relieved at both ends; a port on the Pi itself, never a hub |
| DSI panel to Pi 4B | the DSI ribbon to the DISPLAY connector | seated with the Pi powered off; not the CAMERA connector |
| 40-pin header | none | left empty for the whole project |

## The one-HAT rule for this Pi

The header is free. If the MCC 118 (card A) or the WS-28164 (card B) is seated from a
previous sitting it may stay seated: e12 touches neither SPI0 nor any GPIO line. The DSI
panel and the Raspberry Pi keyboard coexist with any HAT.

## Firmware, the first check

CDC-ACM exists only after the firmware declares it. USB-C is the connector; the Zephyr
sensors sample for `steval_stwinbx1` enumerates with CDC-ACM as its default console
(Zephyr board page, verified Wednesday 14 October 2026), and that is the mode kit lab P04
selects. FP-SNS-DATALOG2 streams a different, vendor protocol and is not what this project
parses (to verify against the pack's documentation; st.com refused the fetch on
Wednesday 14 October 2026). The frame form the parser expects is one text line per frame,

    S,<seq>,<t_us>,<ax0>,<ay0>,<az0>,<ax1>,<ay1>,<az1>,<ax2>,<ay2>,<az2>,<ax3>,<ay3>,<az3>

four samples in milli-g with a sequence number and the first sample's timestamp in
microseconds. It is defined by this project in `src/stwin_frames.hpp` and is to verify
against the firmware actually flashed; the parser is the one file to change.

## Pre-flight, on the Pi 4B over SSH or at the panel

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    ls -l /dev/serial/by-id/ /dev/ttyACM*
    timeout 2 cat /dev/ttyACM0 | head -n 5

The last line must show frames in the form above, with the sequence number rising by one
per line. `get_throttled` must read `0x0` before and after the run. The panel's console
font is set once for a report readable from a metre:

    sudo sed -i 's/^FONTSIZE=.*/FONTSIZE=16x32/' /etc/default/console-setup
    sudo setupcon

## The run

Mount the probe, let it settle for a few seconds, then start the run with the hand away
from the mounting: the first 50 windows (about 34 s) are the baseline and must be taken
on the same mounting the excited windows are. From window 51 on, hold a phone with its
vibration motor on against the probe, or tap the mounting, for the next 50 windows.

    cd ~/edge-llm-pi
    build/projects/e12-stwin-band-reporter/edgellm-e12 --project e12 \
      --live serial /dev/ttyACM0:921600 \
      --grammar projects/e12-stwin-band-reporter/grammar.gbnf \
      --prompt projects/e12-stwin-band-reporter/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --state /tmp/e12.state --n-predict 96 --baseline-windows 50 --max-events 100 \
      --log projects/e12-stwin-band-reporter/evidence/e12_run.csv
    vcgencmd get_throttled

The binary must have been configured with `-DEDGELLM_IO_SERIAL=ON`; without it `--live`
says so and exits. The baud in the spec is formal: a CDC-ACM port ignores it.

## Acceptance

    python tools/agreement.py projects/e12-stwin-band-reporter/evidence/e12_run.csv
    awk -F, 'NR>1 {split($3, v, ";"); print v[8]}' \
      projects/e12-stwin-band-reporter/evidence/e12_run.csv | sort -n | sed -n '1p;$p'

The first prints the agreement table over the 100 windows and the count of rows without
a literal, which must be zero. The second prints the lowest and highest measured sample
rate over the run from the raw column (value 8 of the event), which must both sit within
1 percent of 6000 Hz. The program's closing line on stderr must report zero dropped
frames and zero windows with gaps.

## config.txt

No overlay and no line is added for e12. The DSI panel is detected by the firmware.
