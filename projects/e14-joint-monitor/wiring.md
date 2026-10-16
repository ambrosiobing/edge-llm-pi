# e14 wiring: the WS-28164 CAN HAT and the ADXL345 on one Pi 4B header

Project e14 watches one classic CAN bus and one accelerometer and recommends INSPECT to a
person. Nothing is actuated: no line is driven, no frame is sent by the service, and the
model's answer reaches the log and nothing else.

The board is the Raspberry Pi 4 Model B with **card B**, the spare Verbatim 32 GB, which
is the card that carries the CAN overlays; card A carries the MCC 118 and the two cannot
share one `config.txt`, because both claim SPI0 chip selects. The HAT is the Waveshare
WS-28164 (RS232, RS485, CAN, CAN FD), read from the vendor wiki and schematic on
Tuesday 6 October 2026: **two controllers**, an MCP2518FD with an MCP2562FD on SPI0 chip
select 1 with interrupt GPIO24 and a 40 MHz crystal, and an MCP2515 with an SN65HVD230 on
SPI0 chip select 0 with interrupt GPIO23 and 16 MHz. Both 120 ohm terminations are
fitted. The bus is the HAT's own two nodes joined by **two jumper wires** between the CAN
FD and the CAN screw terminals, proven as a two node classic CAN bus on
Tuesday 7 October 2026. Classic frames only: project e14 sends nothing and reads
everything, and a CAN FD phase waits for a transceiver that is not bought.

The sensor is the DFRobot SEN0032 ADXL345 at address 0x53 on I2C bus 1.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | ADXL345 VCC, straight to the pin and never through a breadboard rail | not recorded |
| 3 | GPIO2, SDA1 | ADXL345 SDA | not recorded |
| 5 | GPIO3, SCL1 | ADXL345 SCL | not recorded |
| 6 | GND | ADXL345 SDO, tied low, which is what makes the address 0x53 | not recorded |
| 9 | GND | ADXL345 GND | not recorded |
| 17 | 3V3 | ADXL345 CS, tied high, which is what keeps the part in I2C mode | not recorded |
| 19, 21, 23 | GPIO10, GPIO9, GPIO11 | the HAT: SPI0 MOSI, MISO, SCLK | on the HAT |
| 24, 26 | GPIO8, GPIO7 | the HAT: SPI0 chip select 0 (MCP2515) and 1 (MCP2518FD) | on the HAT |
| 16 | GPIO23 | the HAT: MCP2515 interrupt. **Not available for the ADXL345 INT1** | on the HAT |
| 18 | GPIO24 | the HAT: MCP2518FD interrupt | on the HAT |
| 27, 28 | ID_SD, ID_SC | the HAT's identification EEPROM, if fitted | on the HAT |

The pads of the SEN0032 are read from the **silkscreen**, not from the schematic's J1
numbering, which puts the supply one pad out: from the end opposite the mounting hole,
GND, VCC, CS, INT1, INT2, SDO, SDA, SCL.

**INT1 is not wired in this project.** Project e05 puts it on header pin 16, GPIO23, and
on this board GPIO23 is the MCP2515's interrupt line. So e14 polls the IIO attributes at
50 Hz instead of waiting on an interrupt, and the chapter prices that: at 100 kHz, three
axes of two bytes each with their register writes is about 1.8 ms of bus time per sample,
so 50 Hz is about nine percent of the I2C bus and nothing else needs it.

## What the HAT leaves free, and the one HAT rule for this Pi

With only the two CAN overlays loaded (not the wiki's full block, which adds SPI1 and the
SC16IS752 for RS232 and RS485), the HAT claims SPI0's five pins, GPIO23, GPIO24 and the
EEPROM pins. **GPIO2 and GPIO3 are untouched**, and they are the two pins that carry the
Pi's fitted 1.8 kOhm pull-ups, so the ADXL345 needs no resistor and this bench has none to
add. Electrically free with that configuration: header pins 1, 3, 5, 6, 7, 9, 11, 12, 13,
15, 17, 20, 22, 25, 29 to 40. The sensor is on I2C and the HAT is on SPI: they share no
pin and no controller, which is why this is the one project in the volume with a HAT and a
loose part at the same time.

**Mechanically this is to verify, and it can stop the sitting.** Whether the WS-28164's
2x20 header passes through to a second row is not recorded (to verify on the bench), and
the 2x20 stacking header that would free the pins under a HAT is **not on this bench**. If
the HAT's header does not pass through, the six leads of the ADXL345 cannot reach pins 1
to 17 while the HAT is seated, and the hundred minute run cannot be taken until a stacking
header is bought. Read the HAT's header before anything else and write what you see into
this file with the date.

**Every contact is a friction contact.** The SEN0032's header is not soldered and this
bench has no soldering iron: the working arrangement is male pins pushed through the
plated holes and down into a breadboard. Assume at every step that any one of the six
contacts can be open, and that an intermittent VCC does more than go quiet, because the
ADXL345's protection diodes then clamp SDA and SCL toward its dead rail and take the bus
down with it, which has been observed on this bench.

## config.txt, card B

    dtparam=spi=on
    dtoverlay=mcp2515,spi0-0,oscillator=16000000,interrupt=23
    dtoverlay=mcp251xfd,spi0-1,interrupt=24
    dtparam=i2c_arm=on
    dtparam=i2c_arm_baudrate=100000
    dtoverlay=i2c-sensor,adxl345,addr=0x53

The baud rate line is not optional: **400 kHz answered nothing on this wiring** on
Friday 9 October 2026 and 100 kHz works. The exact spelling of the sensor overlay is to
verify against `/boot/firmware/overlays/README` on the card; the Linux volume's project 5
binds the same part through the mainline `adxl345` IIO driver and is the reference.

## Pre-flight, on the Pi 4B over SSH

The interface names swap between boots. This is the first command of every sitting, and
nothing else is run until its output has been read:

    dmesg | grep mcp25
    ip -details -statistics link show

Then the two interfaces up at 500 kbit/s, the bus confirmed, and the sensor found:

    sudo ip link set can0 up type can bitrate 500000
    sudo ip link set can1 up type can bitrate 500000
    i2cdetect -y 1
    i2cget -y 1 0x53 0x00

`i2cdetect` must show `53`, and the DEVID read must answer `0xe5`; anything else is a
contact, not a part. Then the IIO device and the memory:

    ls -l /sys/bus/iio/devices/
    cat /sys/bus/iio/devices/iio:device0/name
    cat /sys/bus/iio/devices/iio:device0/in_accel_scale
    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled

`get_throttled` must read `0x0` before and after the hundred minute run. The model file
must fit under 60 percent of `MemAvailable`, and for this project the figure to check is
the **pair**, the service plus the daemon, not one process.

## The two processes

The engine daemon first, in its own terminal or its own unit:

    target/release/edgellm-e14-engine-daemon \
      --socket /run/edgellm/e14-engine.sock \
      --grammar projects/e14-joint-monitor/grammar.gbnf \
      --prompt projects/e14-joint-monitor/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --n-predict 64 --n-ctx 512 --threads 4

Then the monitor, with the interface name read from `dmesg` above:

    target/release/edgellm-e14 --project e14 \
      --live can0 --iio /sys/bus/iio/devices/iio:device0 \
      --socket /run/edgellm/e14-engine.sock \
      --minute-s 60 --sample-hz 50 --timeout-ms 2000 \
      --log projects/e14-joint-monitor/evidence/e14_run.csv

## Injecting the hundred minutes

Floods from the other node, in a third terminal, during the minutes the fault catalogue
names:

    cansend can1 123#DEADBEEF
    for i in $(seq 1 2000); do cansend can1 123#DEADBEEF; done

An error counter that rises needs a fault, not traffic: pull one of the two jumper wires
for a second, or bring the far interface down while the near one sends, and read the
counters before and after:

    cat /sys/class/net/can0/statistics/rx_errors /sys/class/net/can0/statistics/tx_errors

Taps are hand taps on the breadboard beside the module, which is what the ADXL345 reads
as a peak above 2 g.

## Recovery, measured by the service's own clock

With both processes running by hand, stop the daemon with signal 9 and read the log:

    pkill --signal 9 -f edgellm-e14-engine-daemon

Every minute without an answer carries `unavailable` in the engine column, and the first
minute after the daemon is started again carries a number in `round_trip_ms`. The
difference of the two timestamps is the recovery time. Under systemd the same measurement
is taken with

    sudo systemctl kill --signal=SIGKILL edgellm-e14-daemon.service

and the chapter says what `BindsTo` does to that reading.

## Acceptance

    cargo test --manifest-path projects/e14-joint-monitor/Cargo.toml
    python tools/agreement.py projects/e14-joint-monitor/evidence/e14_run.csv --md
    cat /proc/$(pidof edgellm-e14)/status | grep VmRSS
    cat /proc/$(pidof edgellm-e14-engine-daemon)/status | grep VmRSS

The oracle test is the host check and crosses the socket. On the bench the three numbers
are the agreement table over the hundred minutes, the resident memory of the pair against
project e13's single process, and the recovery time above, each with `0x0` from
`vcgencmd get_throttled` before and after.
