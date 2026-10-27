# e05 wiring: the DFRobot SEN0032 ADXL345 on the Pi 3 Model B

Project e05 reads the ADXL345 through the kernel's IIO driver on the Raspberry Pi 3 Model B
Rev 1.2 (1 GB) with the card `eplepi` (Raspberry Pi OS, Debian 13 trixie, user `bing`), over
SSH. No HAT is seated; the part hangs on seven flying leads. The header of the SEN0032 is
NOT soldered: every contact is friction, and a part that answers nothing is a contact
question before it is anything else.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | VCC | not recorded; straight from pin 1, never through a breadboard rail |
| 3 | GPIO2, SDA1 | SDA | not recorded |
| 5 | GPIO3, SCL1 | SCL | not recorded |
| 6 | GND | SDO | not recorded; SDO low selects address 0x53 |
| 9 | GND | GND | not recorded |
| 16 | GPIO23 | INT1 | not recorded; wired, not read by this project |
| 17 | 3V3 | CS | not recorded; CS high selects I2C, not SPI |

The Pi's header pins 3 and 5 carry fitted 1.8 kOhm pull-ups, so no resistor is added. CS and
SDO are strapped on the leads because the SEN0032 carries no strap on the board; a part with
either lead open drifts between 0x53 and 0x1d or answers nothing.

## The one-HAT rule for this Pi

The Pi 3 Model B header is bare for e03, e05 and e06. The 3.5 inch LCD (e09) and the SIM7020E
(e16) each take the header alone; unseat them before e05, and keep the LEDs of e03 on BCM 17,
27 and 22 if they are still wired, they share nothing with these seven pins.

## config.txt

    dtparam=i2c_arm=on
    dtparam=i2c_arm_baudrate=100000
    dtoverlay=adxl345-i2c

The bus stays at 100 kHz on the current leads: at 400 kHz the part answered nothing on
Friday 9 October 2026.

**THE DRIVER SENTENCE THAT STOOD HERE WAS WRONG AND IS WITHDRAWN.** It read: "The overlay is
the Linux volume's project 5 overlay, which binds the mainline `adxl345` I2C driver at 0x53
and creates `/sys/bus/iio/devices/iio:deviceN`." That driver is built on **neither** card this
bench has, checked on Tuesday 27 October 2026 and recorded in
`evidence/2026-10-27_2110_iks5a1-substitution-survey_pi3bplus.md`:

    Yocto 6.6.63-v8, raspberrypi3-64    no CONFIG_ADXL345 symbol anywhere in the config
    Pi OS trixie 6.18.50                no adxl345_i2c module anywhere in /lib/modules

The sentence named an image and was never tested against it. **A kernel driver is a fact to
check on the image, not to infer from a part being mainline**, and "mainline" says a driver
exists in the tree, never that this image built it.

**AND PI OS CARRIES A DIFFERENT ADXL345 DRIVER THAT WOULD LOOK LIKE SUCCESS.**
`CONFIG_INPUT_ADXL34X_I2C=m` is the **evdev input** driver, not the IIO one. An ADXL345 there
binds, works, and appears under `/dev/input/` and never under `/sys/bus/iio/devices/`. This
project polls IIO sysfs at 50 Hz, so it would read nothing while a driver had loaded and the
part was functioning perfectly: a silent wrong-subsystem failure, which is harder to diagnose
than a missing driver because every obvious check passes.

Before this project is next built, the overlay file name and the bound driver are to be read
off the running image with `ls /boot/firmware/overlays | grep -i adxl` and
`ls /sys/bus/i2c/drivers/`, and the part replaced if nothing binds. The substitution directed
on Tuesday 27 October 2026 is tracked in the same evidence file: the target image does carry
`st_lsm6dsx_i2c`, whose table includes `lsm6dso16is`, which is the part the X-NUCLEO-IKS5A1
answers with at 0x6b.

## Pre-flight, on the Pi 3 over SSH

Contacts first, then the driver, then the attributes the loop will read.

    i2cdetect -y 1
    ls /sys/bus/iio/devices/
    cat /sys/bus/iio/devices/iio:device0/name
    ls /sys/bus/iio/devices/iio:device0/
    cat /sys/bus/iio/devices/iio:device0/in_accel_scale
    cat /sys/bus/iio/devices/iio:device0/sampling_frequency
    for a in x y z; do cat /sys/bus/iio/devices/iio:device0/in_accel_${a}_raw; done

`i2cdetect` must show `53` and nothing at `1d`; the `name` file must read `adxl345`. With the
driver bound, `i2cdetect` shows `UU` at 53, which is right. The three raw values at rest must
give about 256 counts on the axis pointing up (the scale is close to 0.0383 metres per
second squared per count at full resolution, to verify against the driver source), which is
1 g; a value that reads the same on every poll while the part is moved is a contact, not a
sensor.

    free -m
    vcgencmd get_throttled

The model file must fit under 60 percent of `MemAvailable` (SmolLM2-360M needs 450 MB
available) and the throttle bits must read `0x0` before and after the run.

## The run

The binary carries the engine; it is built in skyhorizon's WSL for aarch64 (see the chapter's
Steps) and copied to the board, or built on the board itself.

    cd ~/edge-llm-pi
    projects/e05-adxl345-events/target/release/edgellm-e05 --project e05 \
      --live iio /sys/bus/iio/devices/iio:device0:0.02 \
      --grammar projects/e05-adxl345-events/grammar.gbnf \
      --prompt projects/e05-adxl345-events/prompt.txt --n-predict 64 \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
      --log projects/e05-adxl345-events/evidence/e05_run.csv
    vcgencmd get_throttled

Stop it with Ctrl-C after the hundred hand-made events; the log is flushed per row.

## Acceptance

    python tools/agreement.py projects/e05-adxl345-events/evidence/e05_run.csv

The hundred events are made by hand, tap, tilt and drop onto foam, and labelled by the
person making them in `evidence/e05_labels.csv` (`t_s,label`), one row per onset in the log,
written during the run from the log's onset rows. The three-way agreement is two runs of the
agreement tool: a copy of the log with the rule column replaced by the label scores label
against model, a copy with the model column replaced by the label scores label against rule.
The sample rate check is the spacing of `t_s` in the log: the median must sit at the
configured 0.02 s, and the gaps during a decision are the stall the chapter discusses.

    awk -F, 'NR>2 {print $1 - p} {p = $1}' projects/e05-adxl345-events/evidence/e05_run.csv \
      | sort -n | awk '{a[NR] = $1} END {print "median", a[int(NR / 2)], "max", a[NR]}'
