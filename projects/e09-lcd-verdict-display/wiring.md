# e09 wiring: the 3.5 inch panel on jumpers, the ADXL345 and three LEDs on pins 1 to 26

Board: Raspberry Pi 3 Model B Rev 1.2 (1 GB) with the card `eplepi` (Raspberry Pi OS,
Debian 13 trixie, user `bing`), reached over SSH. Model: SmolLM2-360M-Instruct Q4_K_M.
Parts: the Waveshare 3.5 inch RPi LCD (A), the DFRobot SEN0032 ADXL345, three Joy-it
LK-LED10 modules (green, yellow, red), fourteen female-to-male jumper wires for the
panel, six female-to-female for the sensor, six for the LEDs.

## Why the panel is not seated on the header

The panel's 26-pin socket mates with header pins 1 to 26 and its carrier runs the full
length of the board over pins 27 to 40 without contacts, so a seated panel leaves no
pin reachable at all: the kit labs' lab P19 settled that from the carrier's schematic
on Thursday 8 October 2026, and the 2x20 stacking header that would raise every pin
above the carrier is not on the bench. The panel declares pins 3, 5, 7, 12, 13, 15 and
16 not connected, so the I2C bus and the GPIO lines this project needs are electrically
free and only physically buried. The way through is the Linux volume's project 7
second route: the panel on eleven jumpers (its own pin table, one wire per signal),
with the SPI clock lowered from 32 MHz to 16 MHz because jumper wires do not carry
32 MHz cleanly. Everything then sits on pins 1 to 26, and the same wiring holds the day
a stacking header arrives and the panel is seated, except that the panel's own 3V3 and
GND jumpers disappear.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | panel 3V3 (logic, pin 1 of its socket) | not recorded |
| 2 | 5V | panel 5V (backlight, pin 2 of its socket) | not recorded |
| 6 | GND | panel GND (pin 6 of its socket) | not recorded |
| 11 | GPIO17 | panel TP_IRQ (pin 11 of its socket), the pen-down interrupt | not recorded |
| 18 | GPIO24 | panel LCD_RS, data or command (pin 18) | not recorded |
| 19 | GPIO10 MOSI | panel LCD_SI / TP_SI (pin 19) | not recorded |
| 21 | GPIO9 MISO | panel TP_SO, the touch controller's data out (pin 21) | not recorded |
| 22 | GPIO25 | panel RST (pin 22) | not recorded |
| 23 | GPIO11 SCLK | panel LCD_SCK / TP_SCK (pin 23) | not recorded |
| 24 | GPIO8 CE0 | panel LCD_CS (pin 24) | not recorded |
| 26 | GPIO7 CE1 | panel TP_CS (pin 26) | not recorded |
| 17 | 3V3 | ADXL345 VCC; its CS is joined to VCC at the sensor end | not recorded |
| 9 | GND | ADXL345 GND; its SDO is joined to GND at the sensor end | not recorded |
| 3 | GPIO2 SDA | ADXL345 SDA, the bus at 100 kHz | not recorded |
| 5 | GPIO3 SCL | ADXL345 SCL | not recorded |
| 16 | GPIO23 | ADXL345 INT1, wired as in e05, not read here | not recorded |
| 7 | GPIO4 | green LED signal pin (NONE) | not recorded |
| 13 | GPIO27 | yellow LED signal pin (TILT) | not recorded |
| 15 | GPIO22 | red LED signal pin (TAP or FREEFALL) | not recorded |
| 14, 20, 25 | GND | the three LED modules' ground pins, one each | not recorded |

Two changes from e05's proven sensor wiring, both forced by the panel and both to be
proved again with `i2cdetect` before anything else: the sensor's VCC moves from pin 1
to pin 17 because the panel takes pin 1, and the two straps (CS to 3V3, SDO to GND) are
made at the sensor's own header instead of at header pins 17 and 6, because the board
has two 3V3 pins and three consumers. The SEN0032 header is not soldered, so every one
of those contacts is friction; the address must read 0x53, alone and on every scan.

One change from e03's LED lines: the green LED moves from GPIO17 (pin 11) to GPIO4
(pin 7), because pin 11 is the panel's touch interrupt and the ads7846 driver refuses
to probe without it. GPIO27 and GPIO22 stay.

## The IKS5A1 in place of the ADXL345, decided Friday 20 November 2026

Joseph decided on Friday 20 November 2026 that this project takes the **X-NUCLEO-IKS5A1**. The
decision of Thursday 19 November 2026 named the IKS4A1 and that shield was wired and identified
at the bus on the morning of the 20th; the allocation changed the same day, and the record of
both is
`projects/e05-adxl345-events/evidence/2026-11-20_1012_the-shield-is-the-iks5a1_pi.md`.

**The part that replaces the ADXL345 is the LSM6DSO16IS at 0x6B**, accelerometer and gyroscope,
identified by `WHO_AM_I` at register `0x0F` reading **`0x22`** and not by its address, because
the IKS4A1 puts a different part at that same address and reads `0x70` there. The two shields
hold a shared part at swapped addresses, measured Monday 9 November 2026, so **the value is the
identification and the address is not**.

The shield's other IMU, at 0x6A, reads `0x73`, which is in no driver's table on this bench. It
is not used. One accelerometer is what this project needs and 0x6B is it.

**Four flying leads, not the five e07's sheet carries.** The fifth is INT1 on header 18 and
this project polls, so it is not wired.

| Header pin | BCM | Shield pin | Wire colour |
|---|---|---|---|
| **17** | 3V3 | CN6 pin 4 (3.3 V) | not recorded |
| 3 | GPIO2, SDA1 | CN5 pin 9 (D14, SDA) | not recorded |
| 5 | GPIO3, SCL1 | CN5 pin 10 (D15, SCL) | not recorded |
| **9** | GND | CN6 pin 6 (GND) | not recorded |

**3V3 from pin 17 and GND from pin 9, because the panel takes pins 1 and 6.** Those are the
two moves this sheet already made for the ADXL345, so they carry over unchanged. Pin 16 is
freed. INT1 on header 18 would be the panel's `LCD_RS` and is excluded, and it is not needed.

**The five addresses of this shield**, measured Tuesday 27 October 2026 on a Raspberry Pi 3
Model B Plus with this same lead plan, by command set and not by address:

    0x19   reg 0x0F = 0x47   LIS2DUXS12; answers i2cdetect -r only
    0x1e   reg 0x4F = 0x40   LIS2MDL; its WHO_AM_I is at 0x4F and not 0x0F
    0x5c   reg 0x0F = 0xb4   LPS22DF or LPS28DFW
    0x6a   reg 0x0F = 0x73   an IMU no kernel here knows; unused
    0x6b   reg 0x0F = 0x22   LSM6DSO16IS, the part this project reads

**Five, not the IKS4A1's seven**: there is nothing at 0x38 or 0x44, and the pressure part sits
at 0x5C rather than 0x5D. The two shields share only the LIS2MDL at 0x1E.

**The pre-flight is two scans, and 0x19 is why.** Measured on this shield on Friday
20 November 2026 at 11:23:

    i2cdetect -y 1       19 1e 5c 6a 6b    five
    i2cdetect -y -r 1       1e 5c 6a 6b    four, no 0x19

**That is the reverse of what the same two modes did on the IKS4A1 five hours earlier**, where
`-r` showed 0x19 in 2 of 2 scans and the default missed it in 1 of 2, and the reverse of e07's
table, which records 0x19 as answering `i2cdetect -r` only. Three readings, three different
behaviours, and one scan per mode cannot settle a responder like that. **So no single mode is
promised and the control is the union of the two:** five addresses across the pair,
`19 1e 5c 6a 6b`, which is exactly the survey of Tuesday 27 October 2026.

0x19 is the LIS2DUXS12 and **no project here reads it**, so this is a note on the pre-flight
and not a blocker. The address that matters is 0x6B.

    i2cget -y 1 0x6b 0x0f    must read 0x22

**Three things this wiring does not settle.** There is **no ST IIO driver on any card of this
bench**, measured Friday 13 and Thursday 19 November 2026, so the reading path is a user-space
I2C reader or an out-of-tree module build and not the `--live iio` this project names today.
The rule thresholds are in g against the ADXL345's 0.0383 m/s2 per count and the LSM6DSO16IS's
full scale is selectable, so they have to be read against the new part. And **the board is to
be named from `/proc/device-tree/model` and not from the card**, because the card name
`eplepi` appears in four sheets of this volume for four different boards.

**One point in this shield's favour and still unverified.** `st_lsm6dsx`'s device table includes
`lsm6dso16is`, measured Tuesday 27 October 2026, which is this part. Whether it includes the
IKS4A1's `lsm6dsv16x` is unknown, because the module exists on no card here and `modinfo` could
not be asked. If the out-of-tree route is ever taken, that is the first thing to measure.

## The one-HAT rule for this Pi

The panel is this project's HAT, on jumpers rather than seated, and it owns SPI0 CE0 and
CE1, GPIO17, GPIO24 and GPIO25. No other HAT and nothing of e03, e05 or e06 that uses
those lines may be on the board in the same sitting. The ADXL345 and the LEDs are the
only other parts, on the pins above.

## config.txt and the overlay, on the Pi 3B over SSH

The panel binds through the Linux volume's project 7 overlay, compiled on the board with
`dtc` (paths to verify on the day; the source is
`embedded-linux-bench/meta-bench/recipes-bench/bench-lcd35a-overlay/files/bench-lcd35a-overlay.dts`
in that repository):

    dtc -@ -I dts -O dtb -o bench-lcd35a.dtbo bench-lcd35a-overlay.dts
    sudo cp bench-lcd35a.dtbo /boot/firmware/overlays/

Then in `/boot/firmware/config.txt`:

    dtparam=spi=on
    dtoverlay=bench-lcd35a,speed=16000000
    dtparam=i2c_arm=on,i2c_arm_baudrate=100000
    dtoverlay=i2c-sensor,adxl345

The `speed=16000000` override is the jumper route; a seated panel on a stacking header
would drop it. `i2c-sensor,adxl345` is the Raspberry Pi overlay entry for the ADXL345
IIO driver (boot/overlays/README, read Wednesday 14 October 2026). Reboot.

## Pre-flight

    free -m
    vcgencmd get_throttled
    i2cdetect -y 1
    ls /sys/bus/iio/devices/ && cat /sys/bus/iio/devices/iio:device0/name
    cat /sys/bus/iio/devices/iio:device0/in_accel_scale
    ls /sys/bus/spi/drivers/ili9486/ /sys/bus/spi/drivers/ads7846/
    for c in /dev/dri/card*; do echo $c; cat /sys/class/drm/$(basename $c)/device/modalias; done
    ls -l /dev/input/by-path/ && sudo evtest
    cat /sys/class/vtconsole/vtcon*/name

`i2cdetect` must show `53` alone; the IIO name must read `adxl345`; `spi0.0` must sit
under ili9486 and `spi0.1` under ads7846; one card's modalias must name the panel's
SPI device; `evtest` must list `ADS7846 Touchscreen`. The scale line is what
`--g-per-unit` undoes: the driver reports m/s^2, and 0.1019716 is one over 9.80665.

Release the console from the panel before the run, or the kernel repaints over the
frames whenever it prints (the vtcon number is the one whose name says frame buffer):

    echo 0 | sudo tee /sys/class/vtconsole/vtcon1/bind

## The run

    cd ~/edge-llm-pi
    vcgencmd get_throttled
    sudo build/projects/e09-lcd-verdict-display/edgellm-e09 --project e09 \
      --n-predict 16 \
      --live iio /sys/bus/iio/devices/iio:device0:0.1 --g-per-unit 0.1019716 \
      --leds /dev/gpiochip0:4,27,22 \
      --grammar projects/e09-lcd-verdict-display/grammar.gbnf \
      --prompt projects/e09-lcd-verdict-display/prompt.txt \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf --state /tmp/e09.state \
      --log projects/e09-lcd-verdict-display/evidence/e09_run.csv \
      --panel-log projects/e09-lcd-verdict-display/evidence/e09_run_panel.csv \
      --max-events 4000

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
    vcgencmd get_throttled

Tilt, drop onto a cushion and tap the sensor by hand until the panel log holds 200
decision rows, touching the glass 50 times along the way. The loop asks the model only
when the rule's verdict changes, so a held tilt is one decision in and one out, and the
panel shows the rule's verdict seconds before the model's answer: that gap is the point
of the chapter, not a defect.

## Acceptance

    python tools/agreement.py projects/e09-lcd-verdict-display/evidence/e09_run.csv
    awk -F, '$2=="decision"{n++; if($6+0>=200)bad++} \
      END{print n" decisions, "bad+0" at or over 200 ms"}' \
      projects/e09-lcd-verdict-display/evidence/e09_run_panel.csv
    awk -F, '$2=="touch"{n++} END{print n" touches toggled a pane"}' \
      projects/e09-lcd-verdict-display/evidence/e09_run_panel.csv

Pass: 200 decision rows with none at or over 200 ms, 50 touch rows for 50 presses, zero
rows whose model column is not one of NONE, TAP, TILT, FREEFALL, and `get_throttled`
reading `0x0` before and after. The CI check is the oracle test over the replay:

    build/tests/oracle_check /tmp/e09_stub.csv tests/oracle/e09.expected.csv
