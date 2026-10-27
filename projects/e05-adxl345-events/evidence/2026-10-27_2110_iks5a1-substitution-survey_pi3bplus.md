# Can e05 read an IKS5A1 where it reads an ADXL345, Tuesday 27 October 2026, 21:10

    board      Raspberry Pi 3 Model B Plus Rev 1.3, revision code a020d3, 905 MB
    shield     X-NUCLEO-IKS5A1 on five flying leads, e07's pin mapping
    cards      two, swapped in the same board with the shield left wired:
               Raspberry Pi OS trixie, kernel 6.18.50+rpt-rpi-v8, user bing
               Yocto, raspberrypi3-64, kernel 6.6.63-v8, BusyBox, root
    tools      i2cdetect and i2cget from i2c-tools on the Pi OS card;
               sysfs and dmesg alone on the Yocto card

**This closes no clause of e05 and is not a measurement of the project.** It is the survey
that had to come before the substitution Joseph directed on Tuesday 27 October 2026, when he
said the ADXL345 is no longer reliably functioning and asked for an IKS4A1 or IKS5A1 in its
place. Fifty-six files name the ADXL345, including a directory, a chapter filename, two
grammars, three replay generators and four C sources, so the question of whether the
replacement can be read at all is worth a sitting of its own before any of that moves.

**The answer is yes on the image e05 deploys to, and the reason is the opposite of what this
project's own wiring file says.**

## The method, which is one variable

The shield stayed wired across both cards and only the image changed. The same board, the same
five leads, the same bus: that is the same one-variable substitution used on the power adapter
earlier the same evening, and it is why the two kernels can be compared at all.

Pin mapping taken from `projects/e07-iks4a1-narrator/wiring.md`, which already hangs an
X-NUCLEO shield off a Pi 3B+ header. The IKS5A1 is the same Arduino form factor, so the
connector positions carry over; **the CN numbering is e07's, measured on the IKS4A1, and is to
confirm against the IKS5A1's own silkscreen.**

    header 1    3V3           shield CN6 pin 4
    header 3    GPIO2, SDA1   shield CN5 pin 9  (D14)
    header 5    GPIO3, SCL1   shield CN5 pin 10 (D15)
    header 6    GND           shield CN6 pin 6
    header 18   GPIO24        shield CN9 pin 6  (INT1), wired, nothing reads it

## Five parts, identified by command set rather than by address

    address  register  value   part
    0x19     0x0F      0x47    LIS2DUXS12; answers only to i2cdetect -r
    0x1e     0x4F      0x40    LIS2MDL magnetometer; its WHO_AM_I is at 0x4F, not 0x0F
    0x5c     0x0F      0xb4    LPS22DF/LPS28DFW pressure family
    0x6a     0x0F      0x73    an IMU; no kernel on this bench knows this value
    0x6b     0x0F      0x22    LSM6DSO16IS

An address only says something answered. The `WHO_AM_I` register says what answered, and the
0x19 reading reproduces exactly the signature e07 recorded for the LIS2DUXS12 on the IKS4A1,
which is an independent check that the method works on this bus.

**The shield is not the IKS4A1**, by three differences from e07's measured table: nothing at
0x38, pressure at 0x5c rather than 0x5d, and **two distinct IMUs** where the IKS4A1 has one.
The two shields share only the LIS2MDL at 0x1e.

## The driver situation, which reverses this project's premise

    Yocto 6.6.63-v8, raspberrypi3-64
      registered on the I2C bus: dummy sht4x st-magn-i2c st-press-i2c st_lsm6dsx_i2c stmpe-i2c
      CONFIG_IIO_ST_LSM6DSX=m, _I2C=m, _SPI=m
      no CONFIG_ADXL345 symbol anywhere in the config

    Raspberry Pi OS trixie 6.18.50+rpt-rpi-v8
      registered on the I2C bus: dummy stmpe-i2c
      # CONFIG_IIO_ST_LSM6DSX is not set
      adxl345_i2c: no module anywhere in /lib/modules

**`wiring.md` of this project states that the mainline `adxl345` I2C driver binds at 0x53 and
creates `/sys/bus/iio/devices/iio:deviceN`. That driver is built on neither card.** The claim
names the Linux volume's project 5 as its source and has evidently never been tested against
the image it refers to. It is wrong and the chapter has to say so.

So on the image e05 actually deploys to, **the ADXL345 is the part with no driver and the
IKS5A1 carries one that is supported**: `st_lsm6dsx_i2c`'s device table includes
`lsm6dso16is`, and `lsm6dso16is` expects `WHO_AM_I` 0x22, which is what 0x6b returns. e05
needs one accelerometer and 0x6b provides it.

**A trap on Pi OS worth recording before it costs someone an evening.** That kernel carries
`CONFIG_INPUT_ADXL34X_I2C=m`, which is the **evdev input** driver and not the IIO one. An
ADXL345 there binds, works, and appears under `/dev/input/`, never under
`/sys/bus/iio/devices/`. A project polling IIO sysfs would see nothing while a driver had
loaded and the part was functioning, which is a silent wrong-subsystem failure rather than a
missing-driver one.

## The overlay on the Yocto card declares the two IMUs the wrong way round

    st_lsm6dsx_i2c 1-006a: unsupported whoami [73]
    st_lsm6dsx_i2c 1-006b: failed to read whoami register
    st_lsm6dsx_i2c: probe of 1-006b failed with error -5

    1-006a  declared lsm6dso16is  actual 0x73
    1-006b  declared lsm6dsv16x   actual 0x22

`lsm6dso16is` expects 0x22 and the 0x22 part is at **0x6b**, so the declarations are swapped
relative to what is fitted. **The driver states this itself**, which is a stronger
identification than the register reads above: the code that would do the reading refused the
part it was pointed at, on the grounds of its command set.

`lis2mdl` bound by itself and is `iio:device0`, because both shields put it at 0x1e. So the
binding machinery on that card works and only the IMU declaration is wrong.

**The swap could not be tested from the console.** `delete_device` returns "Can't find device
in list" for a device-tree-instantiated client; that interface removes only what `new_device`
created. Instantiating over the top returns `-EBUSY`. The fix is one overlay line and a
reboot, and **that overlay belongs to the Embedded Linux volume's card**, so it is not changed
here. e05 inherits it as a dependency rather than owning it.

Nothing was written to the Yocto card. The two `new_device` and `delete_device` attempts are
runtime-only and do not survive a reboot.

## What is established and what is not

    established   the IKS5A1's five parts and their addresses, by command set
    established   the target image has an ST IMU driver and no ADXL345 driver
    established   the part at 0x6b is in that driver's table
    established   this project's wiring.md is wrong about its own image
    NOT shown     that the driver binds and produces in_accel_* attributes,
                  because the overlay holds the address and cannot be released
    NOT shown     what the part at 0x6a is; outside this kernel's table, and
                  e05 does not need it
    NOT shown     anything about e05's acceptance, none of which was exercised

## What the substitution now costs, stated before it starts

e05's four clauses are accelerometer-generic and **none of them changes**: the three-way
agreement, the 0.020 s median sample spacing, the comparison of binary size and resident
memory against e03, and the rest check of the scale within 5 percent of 1 g after the driver's
scale. The fourth is the only one that touches the sensor at all and it tests the driver's
scale, which any accelerometer must satisfy.

What changes is identity and wiring: the address 0x53 by strapping SDO low becomes a fixed
0x6b, the strap paragraph goes, the interrupt pin moves, and the parts table names the shield.
Plus the driver claim corrected above, and a dependency recorded on an overlay fix in another
volume.

## Deviations

1. **Identification ran on a 3B+ Rev 1.3, not the Pi 3 Model B Rev 1.2 the chapter allocates.**
   For establishing what answers on a bus and which drivers a kernel carries, the board is not
   the variable; for anything e05 publishes, it will be.
2. **The shield sat on flying leads on a bare header**, not in any fixed mounting, and was
   wired and unwired with the board powered down each time.
3. **No part of e05 was built or run.** No binary, no replay, no log.
