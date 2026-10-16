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
      --live iio /sys/bus/iio/devices/iio:device0:0.1 --g-per-unit 0.1019716 \
      --leds /dev/gpiochip0:4,27,22 \
      --grammar projects/e09-lcd-verdict-display/grammar.gbnf \
      --prompt projects/e09-lcd-verdict-display/prompt.txt \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf --state /tmp/e09.state \
      --log projects/e09-lcd-verdict-display/evidence/e09_run.csv \
      --panel-log projects/e09-lcd-verdict-display/evidence/e09_run_panel.csv \
      --max-events 4000
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
