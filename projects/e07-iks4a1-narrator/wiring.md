# e07 wiring: the X-NUCLEO-IKS4A1 on five flying leads to the Pi 3 Model B Plus

Board: Raspberry Pi 3 Model B Plus, 1 GB, card `eplepi` (Raspberry Pi OS, Debian 13
trixie, kernel 6.18.50+rpt-rpi-v8, user `bing`), over SSH only; the serial console of
this board is dead (pinmux). No HAT is seated. The shield hangs off the header on five
leads to its Arduino R3 connectors, which is the wiring measured on Wednesday
30 September 2026 in the Linux volume's project 10.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | shield CN6 pin 4 (3.3 V) | not recorded |
| 3 | GPIO2, SDA1 | shield CN5 pin 9 (D14, SDA) | not recorded |
| 5 | GPIO3, SCL1 | shield CN5 pin 10 (D15, SCL) | not recorded |
| 6 | GND | shield CN6 pin 6 (GND) | not recorded |
| 18 | GPIO24 | shield CN9 pin 6 (LSM6DSV16X INT1) | not recorded; unused by e07 |

Header pins 3 and 5 carry the Pi's fitted 1.8 kOhm pull-ups, so no resistor is added.
The bus runs at the default 100 kHz. INT1 is wired because project 10 uses it for the
FIFO watermark; this project polls sysfs and never touches GPIO24.

## Addresses, measured Wednesday 30 September 2026

| Address | Part | Driver | Device | Read by e07 |
|---|---|---|---|---|
| 0x19 | LIS2DUXS12 | st_lis2duxs12 not in the image | none | no; answers `i2cdetect -r` only |
| 0x1e | LIS2MDL | st_magn | iio `lis2mdl` | yes, field magnitude |
| 0x38 | STTS22H | none in mainline | none | no |
| 0x44 | SHT40AD1B | sht4x (hwmon) | hwmon `sht4x` | yes, temperature and humidity |
| 0x5d | LPS22DF, SA0 high | st_pressure | iio `lps22df` | yes, pressure |
| 0x6a | LSM6DSO16IS, WHO_AM_I 0x22 | st_lsm6dsx | iio `lsm6dso16is_accel`, `_gyro` | logged as a witness |
| 0x6b | LSM6DSV16X, WHO_AM_I 0x70 | st_lsm6dsx | iio `lsm6dsv16x_accel`, `_gyro` | yes, motion |

Nothing answers at 0x18. The two IMUs are the reverse of what the overlay once
assumed: 0x6a is the ISPU part. The overlay is the Linux volume's project 10
`bench-iks4a1-overlay.dts`, with the 0x6a node added on Thursday 1 October 2026.

## The one-HAT rule for this Pi

The Pi 3B+ header must be bare. If the Explorer700 (e08) or the SIM7070G (e17, e19)
is seated from a previous sitting, it comes off first: the shield's leads go to pins
1, 3, 5, 6 and 18 of the header itself.

## Pre-flight, on the Pi 3B+ over SSH

    sudo modprobe i2c-dev
    i2cdetect -y 1
    i2cdetect -y -r 1
    i2cget -y 1 0x6a 0x0f
    i2cget -y 1 0x6b 0x0f
    for d in /sys/bus/iio/devices/iio:device*; do echo "$d $(cat $d/name)"; done
    for h in /sys/class/hwmon/hwmon*; do echo "$h $(cat $h/name)"; done
    free -m
    vcgencmd get_throttled

`dtparam=i2c_arm=on` alone enables the controller; `modprobe i2c-dev` is what creates
`/dev/i2c-1`. The default scan shows 0x44 and misses 0x19 on most runs; `-r` shows
0x19 and misses 0x44; neither mode enumerates this board on its own. The two `i2cget`
reads must return 0x22 and 0x70 in that order. The IIO list must show six names:
`lsm6dsv16x_gyro`, `lsm6dsv16x_accel`, `lsm6dso16is_gyro`, `lsm6dso16is_accel`,
`lis2mdl`, `lps22df`, in whatever `iio:deviceN` order this boot produced; the binary
binds by name. The hwmon list must show `sht4x`. `MemAvailable` must be at least
818 MB for the 0.5B model under the 60 percent rule, else the run uses
SmolLM2-360M-Instruct. `get_throttled` must read `0x0` before and after the day.

## config.txt

    dtparam=i2c_arm=on
    dtoverlay=bench-iks4a1

The overlay `.dtbo` is the one compiled for the Linux volume's project 10 and already
on the card's boot partition; on the Yocto image the `bench-iks4a1` recipe deploys it.

## The run, one day

    cd ~/edge-llm-pi
    cmake -S . -B build -DGGML_NATIVE=ON -DEDGELLM_IO_IIO=ON -DEDGELLM_BUILD_TESTS=OFF
    cmake --build build -j2 --target edgellm-e07
    nohup build/projects/e07-iks4a1-narrator/edgellm-e07 --project e07 \
      --live iio /sys/bus/iio/devices --interval 60 --every 60 --max-events 1440 \
      --grammar projects/e07-iks4a1-narrator/grammar.gbnf \
      --prompt projects/e07-iks4a1-narrator/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --state /tmp/e07.state \
      --log projects/e07-iks4a1-narrator/evidence/e07_day.csv > e07_day.err 2>&1 &
    vcgencmd get_throttled

`-j2`, not `-j4`: a 1 GB board swaps at four parallel compiles of the engine tree. The
run ends by itself after 1440 events, 24 hours later; `e07_day.err` carries the bound
device directories and the end-of-run line.

## Acceptance

    python projects/e07-iks4a1-narrator/tools/hallucination_check.py \
      projects/e07-iks4a1-narrator/evidence/e07_day.csv

Prints N 1440, asked 24, the trend agreement over the 24 answers with its 3 by 3
table, and the count of numbers in the 24 summaries that are not among that event's
inputs. The count is published as it is; the chapter's acceptance is that it is
counted, that every answer carries a trend literal, and that the throttle bits read
`0x0` before and after. `tools/agreement.py` scores the house key `decision`; this
project's key is `trend`, so the project's own tool prints the table.
