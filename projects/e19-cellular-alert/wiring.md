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
