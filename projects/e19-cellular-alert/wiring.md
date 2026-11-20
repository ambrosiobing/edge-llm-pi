# e19 wiring: the SIM7070G HAT on the header, the ADXL345 on pins 3 and 5

Board: Raspberry Pi 3 Model B Plus, card `eplepi` (Raspberry Pi OS, Debian 13 trixie, user
`bing`). One 40-pin HAT per header, and on this header it is the Waveshare SIM7070G Cat-M,
NB-IoT and GPRS HAT. The accelerometer is the DFRobot SEN0032 ADXL345 at address 0x53 on the
I2C bus of header pins 3 and 5, wired through the HAT's pass-through header.

**Check the pass-through by eye before anything else.** The HAT leaves pins 3 and 5
electrically free: by its vendor's own claim list it attaches 5 V on pins 2 and 4, ground, the
module's UART on GPIO14 and GPIO15 (pins 8 and 10), the power key on pin 7 (GPIO4), and the
module's DTR on pin 37 (GPIO26) only when the DTR jumper sits in position A, which is not its
default. GPIO2 and GPIO3 appear in none of those nets. Whether the board in hand carries a
40-pin pass-through so that those pins are also mechanically reachable is a fact about one
board and is **to verify**; the 2x20 stacking header that would free a covered header is not
on this bench. If this board has no pass-through, the accelerometer moves to the Pi 3 Model B
of project e05 and this project's events arrive over the LAN instead, which makes the sensor
half of the chapter modelled and is said so in the chapter rather than worked around here.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | ADXL345 VCC | not recorded |
| 3 | GPIO2 | ADXL345 SDA, I2C1 data | not recorded |
| 5 | GPIO3 | ADXL345 SCL, I2C1 clock | not recorded |
| 6 | GND | ADXL345 SDO, strapped low to select 0x53 | not recorded |
| 9 | GND | ADXL345 GND | not recorded |
| 16 | GPIO23 | ADXL345 INT1, not used by this project | not recorded |
| 17 | 3V3 | ADXL345 CS, strapped high to select I2C | not recorded |
| 2, 4 | 5V | HAT supply, through the header | the HAT's own |
| 7 | GPIO4 | HAT PWRKEY, the HAT's own, never driven from anything else | the HAT's own |
| 8, 10 | GPIO14, GPIO15 | module UART, used only on the UART route | the HAT's own |
| 37 | GPIO26 | module DTR, only with the DTR jumper in position A, not the default | the HAT's own |

Pin 6 is a ground pin and carries two things: the HAT takes its ground through the header
underneath, and the SDO strap lands on the same pin on the pass-through side. That is one net
at 0 V and not a contention. The straps on pins 17 and 6 are what fix the address at 0x53: the
SEN0032 breakout has no strap of its own, and a floating CS or SDO makes the part drift between
0x53 and 0x1d. **Never move the HAT's IO level jumper**: it defaults to 3.3 V and its other
position makes the HAT's logic 5 V on a header whose every pin is a 3.3 V input.

The SEN0032's header is not soldered, so every contact on the four sensor wires is friction. A
reading that stops for no reason is a contact, not a driver.

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
| 1 | 3V3 | CN6 pin 4 (3.3 V) | not recorded |
| 3 | GPIO2, SDA1 | CN5 pin 9 (D14, SDA) | not recorded |
| 5 | GPIO3, SCL1 | CN5 pin 10 (D15, SCL) | not recorded |
| 6 | GND | CN6 pin 6 (GND) | not recorded |

Four leads where the ADXL345 needed seven, because the two straps on pins 17 and 6 that fixed
0x53 do not exist on a shield. **The HAT question above is unchanged and still decides this**:
the SIM7070G claims none of GPIO2, GPIO3, pin 1 or pin 6 by its vendor's net list, and whether
the board in hand carries a pass-through that makes them mechanically reachable is to verify.
If it does not, the fallback in the paragraph above holds unchanged, with the shield on e05's
board and the events over the LAN. **This project was not in the substitution list of
Thursday 19 November 2026** and was found on Friday 20 November 2026 to read the retired part
with nothing in its manifest saying so.

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

## Pre-flight, on the Pi 3B+ over SSH

The Pi 3B+ serial console is dead on this bench (the pinmux leaves GPIO14 unmuxed), so SSH is
the console.

    i2cdetect -y 1
    i2cget -y 1 0x53 0x00
    ls -l /dev/serial/by-id/ /dev/ttyUSB*
    ls -d /sys/bus/iio/devices/iio:device*
    free -m
    vcgencmd get_throttled

`i2cdetect` must show `53` and nothing at `1d`; `i2cget` on register 0x00 must answer `0xe5`,
the ADXL345 device identifier. The bus must be at 100 kHz: 400 kHz answered nothing on these
leads. `free -m` decides the model rung, and `get_throttled` must read `0x0` before and after
a timed run.

The modem's AT channel over the HAT USB cable is normally `/dev/ttyUSB2`; confirm it against
`/dev/serial/by-id/` before using it, because the index depends on which interfaces the module
exposes. Both antennas go on before power, and the SIM goes in before power.

    printf 'AT\r' > /dev/ttyUSB2
    printf 'AT+CPIN?\r' > /dev/ttyUSB2
    printf 'AT+CREG?\r' > /dev/ttyUSB2
    printf 'AT+CSQ\r' > /dev/ttyUSB2

Read the answers with `cat /dev/ttyUSB2` in a second session. Without a SIM, `AT+CPIN?`
answers an error and `AT+CREG?` never reaches status 1 or 5; the project then composes and
checks messages and sends none, which is a valid run and is what the log says.

## config.txt

    dtparam=i2c_arm=on
    dtparam=i2c_arm_baudrate=100000

The IIO binding for the ADXL345 is the overlay of the Linux volume's project 5 and its lines
live in that volume; they are not repeated here. The UART route additionally needs
`enable_uart=1` and the HAT's UART jumper in the position that connects the module to GPIO14
and GPIO15, which the manual does not state (**to verify** on the silkscreen); the USB route
needs neither.

## The run

    cd ~/edge-llm-pi
    build/projects/e19-cellular-alert/edgellm-e19 --project e19 \
      --replay projects/e19-cellular-alert/replay/alerts_50.csv \
      --grammar projects/e19-cellular-alert/grammar.gbnf \
      --prompt projects/e19-cellular-alert/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --state /tmp/e19.state \
      --log projects/e19-cellular-alert/evidence/e19_run.csv \
      --alerts projects/e19-cellular-alert/evidence/e19_alerts.csv
    vcgencmd get_throttled

The live run is the same binary with the accelerometer behind `--live iio` and, for a send, the
modem's AT device and a destination number:

    cd ~/edge-llm-pi
    build/projects/e19-cellular-alert/edgellm-e19 --project e19 \
      --live iio /sys/bus/iio/devices/iio:device0:0.1 \
      --grammar projects/e19-cellular-alert/grammar.gbnf \
      --prompt projects/e19-cellular-alert/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --modem /dev/ttyUSB2:115200 --sms-to +4300000000000 \
      --log projects/e19-cellular-alert/evidence/e19_live.csv \
      --alerts projects/e19-cellular-alert/evidence/e19_live_alerts.csv

Without `--modem` the send path is reported as not fitted on every row of the alert ledger and
nothing is handed to the modem.

## Acceptance

    build/tests/oracle_check projects/e19-cellular-alert/evidence/e19_run.csv \
      tests/oracle/e19.expected.csv
    ctest --test-dir build -R 'rules_test|e19_compose_check' --output-on-failure

The oracle compares the rule column of the fifty events and the model column row by row; its
empty model cells are the proof that the model was asked only on the four events the send rule
armed. What passes the chapter is the program's own summary line: every message that was sent
checked field by field against the event it was composed from, zero messages sent on an event
the rule did not arm, and the count of messages refused for carrying a number that is in no
field of the event, published as a rate. Delivery is a separate question and is not measured
without a second phone: `+CMGS` returns the module's message reference, which says a message
left the module and nothing about arrival.
