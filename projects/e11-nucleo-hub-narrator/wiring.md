# e11 wiring: USB only, nothing on the header

Project e11 reads the sensor hub of the Linux volume's project 12: the NUCLEO-H7A3ZI-Q
carrying the X-NUCLEO-IKS5A1, streaming framed CBOR over USB CDC. The Raspberry Pi 4
Model B with the card `eplepi` (Raspberry Pi OS, Debian 13 trixie, user `bing`) sees one
USB serial device and nothing else: no part is wired to the 40-pin header, no HAT is
seated, and the five sensors of the shield sit on the microcontroller's I2C bus where
`i2cdetect -y 1` on the Pi cannot see them. An empty grid on the Pi is the architecture
working.

## Which USB port of the Nucleo

The Nucleo has two micro-USB connectors. Project 12 uses the ST-LINK one (CN1 in that
chapter's table): the firmware's USART3 on PD8 and PD9 reaches the ST-LINK V3E through the
board's default solder bridges, and the ST-LINK enumerates a USB CDC ACM interface beside
its debug interface. That is the port this project uses too, at 921600 baud, because it is
the port the hub firmware already talks on and it carries the SWD bridge for reflashing on
the same cable. The user USB connector (CN13, USB OTG FS on the microcontroller itself)
is not used; a firmware that spoke CDC on CN13 would need its own USB device stack and is
a different hub. The connector numbers are from project 12 and the board's silkscreen; the
MB1363 user manual is the reference (to verify).

The cable must be a data cable. A charge-only cable powers the Nucleo, lights its LEDs and
never enumerates; `dmesg | grep cdc_acm` is the check before anything else.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | none: the only connection is the USB cable, Nucleo CN1 to a Pi USB-A port |

| Nucleo side | Signal | Note |
|---|---|---|
| Arduino Uno V3 headers | I2C1 SDA on D14 (PB9), SCL on D15 (PB8), 3V3, GND | the IKS5A1 stacks here; pull-ups on the shield |
| CN1 micro-USB | ST-LINK V3E: CDC ACM virtual COM port plus SWD | to the Pi 4B USB-A, data cable |
| CN13 micro-USB | USB OTG FS of the H7A3 | not used |
| Zio connector | optional trace USART | the Renkforce cable, red lead left open; not used here |

## One shield at a time

The X-NUCLEO-IKS4A1 and the X-NUCLEO-IKS5A1 are the same shape and carry different
silicon. Remove the IKS4A1 and put it in its bag before the IKS5A1 comes out of its bag;
never stack one on the other. The IKS5A1's parts, all on the Nucleo's I2C bus: ISM6HG256X
at 0x6A (a low-g channel at 104 Hz and a high-g channel reporting a peak-hold), ISM330IS
at 0x6B, IIS2DULPX at 0x19, IIS2MDC at 0x1E, ILPS22QS at 0x5C. The hub firmware's own
bus scan, printed at boot on the trace USART or carried in the HELLO frame's sensor list,
is the check; the Pi has no view of that bus.

## The one-HAT rule for this Pi

The Pi 4B header is free during e11. If the MCC 118 (card A) or the WS-28164 (card B) is
seated from a previous sitting it may stay seated: e11 touches no SPI chip select and no
GPIO line. The chapter names the card `eplepi`; any card with the model file serves.

## The Nucleo is shared: reflash first

Three volumes use this one NUCLEO-H7A3ZI-Q. Whatever was flashed last is what runs, and a
stale flash streams something that decodes, or does not, with no error that says so. The
hub firmware is reflashed immediately before every capture, from the Pi over the same
cable, and the frame layer prints the firmware version from the HELLO frame at start,
which carries the firmware's build stamp. A capture whose HELLO line is missing from the
stderr record, or whose stamp is not that day's build, is a reproduction of an older
flash and is not evidence.

    cd ~/edge-llm-pi
    openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
      -c "program ~/sensorhub/sensorhub.elf verify reset exit"

The ELF is the hub firmware built for project 12 with the two added map keys (protocol
1.1, see `src/cbor_frames.h`); it lives in that project's tree, not in this repository.

## Pre-flight, on the Pi 4B over SSH

The device name first, by id and never by number: the ESP32 sidecar of e15, when it is
plugged in, changes the order in which `ttyACM0` and `ttyACM1` are assigned at boot and
after every replug, and a run on the wrong port decodes nothing and says only "bytes
skipped". The ST-LINK V3 virtual COM port is interface 2 of the ST-LINK's USB device, so
its by-id name ends in `-if02`; the serial in the middle is the board's own and is read,
not typed.

    ls -l /dev/serial/by-id/
    dmesg | grep -i cdc_acm
    lsusb | grep -i st-link

Then the memory, the throttle bits and the model file, as every Pi 4B chapter does:

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    vcgencmd measure_temp
    ls -l ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf

`get_throttled` must read `0x0` before and after the run. If `ModemManager` is installed
it probes a new ACM device with AT commands and can corrupt the first frames; the udev
rule under `meta-edgellm/recipes-app/edgellm-e11/files/` carries
`ENV{ID_MM_DEVICE_IGNORE}="1"` for the image, and on Raspberry Pi OS the same line goes
into `/etc/udev/rules.d/80-edgellm-e11.rules`.

## The run

    cd ~/edge-llm-pi
    build/projects/e11-nucleo-hub-narrator/edgellm-e11 --project e11 \
      --live cdc /dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_<serial>-if02:921600 \
      --grammar projects/e11-nucleo-hub-narrator/grammar.gbnf \
      --prompt projects/e11-nucleo-hub-narrator/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --n-predict 64 \
      --state /tmp/e11.state \
      --log projects/e11-nucleo-hub-narrator/evidence/e11_run.csv \
      2> projects/e11-nucleo-hub-narrator/evidence/e11_run.stderr
    vcgencmd get_throttled

Then 100 hand impacts on a block of foam under the stacked boards, about one every six
seconds, so that the model has answered the previous impact before the next one arrives
(the frame layer's pipe buffers about thirteen seconds of frames; past that, frames are
lost and the gap counter says so). Stop with Ctrl-C; the loop finishes the current row.

## Acceptance

    tail -3 projects/e11-nucleo-hub-narrator/evidence/e11_run.stderr
    python tools/agreement.py projects/e11-nucleo-hub-narrator/evidence/e11_run.csv --md

Three numbers come out of the one run. The agreement table over the model rows, which
are one per impact plus one every ten seconds of quiet. The line
`e11: N frames, G sequence gaps (M frames missing), R Hz from the timestamps against the
hub's stated 104 Hz`: G must be 0, and a gap makes the exit status 3, which fails the run
and says nothing about the rule. R must sit within 1 percent of 104 Hz; the hub's own
timestamps are the instrument, so this is the hub's claim about itself checked against
its own clock, and the delivery jitter on the Pi side is a different quantity, not
measured here. The oracle test in CI runs the same binary over `replay/hub_60s.csv` and
expects the two deliberate gaps in that file to be counted and reported, with the exit
status 0 because the source is a replay.

## config.txt

No overlay and no line is added for e11.
