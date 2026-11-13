# e07: on the image this project's own sheet names, three of its four parts have no
# driver, and one of the three is the number the rule reads. Friday 13 November 2026, 19:00

    host       Raspberry Pi 4 Model B, card eplepi, Raspberry Pi OS trixie,
               kernel 6.18.50+rpt-rpi-v8, the exact kernel wiring.md line 4 names
    method     module and directory survey only; no shield was wired and no run taken
    why here   this is a verdict on what blocks acceptance, not a measurement of it

**THE CLAUSE IS BLOCKED AND THE REASON IS NOT THE SHIELD.** e07 reads four parts over
IIO and hwmon. On Raspberry Pi OS trixie three of the four have no driver in the kernel
package, and the missing one that matters most is the pressure part, which this chapter's
own table calls "pressure, the one number the rule reads". So the control rule cannot run
on this image either, and the failure is not about what the model adds.

## THE SURVEY

    part                      driver the sheet names   on trixie 6.18.50+rpt-rpi-v8
    0x1e LIS2MDL, field       st_magn                  MISSING
    0x44 SHT40AD1B, climate   sht4x                    present
    0x5d LPS22DF, pressure    st_pressure              MISSING
    0x6b LSM6DSV16X, motion   st_lsm6dsx               MISSING

    /lib/modules/6.18.50+rpt-rpi-v8/kernel/drivers/iio/magnetometer
        does not exist at all: no magnetometer driver of any make is packaged
    /lib/modules/6.18.50+rpt-rpi-v8/kernel/drivers/iio/pressure
        bmp280-i2c.ko.xz, bmp280.ko.xz, bmp280-spi.ko.xz, ms5637.ko.xz, and no ST part
    /lib/modules/6.18.50+rpt-rpi-v8/kernel/drivers/iio/imu
        bno055, inv_mpu6050, and no ST part
    /boot/firmware/overlays/bench-iks4a1.dtbo
        not on this card

Only temperature and humidity survive, which is one of the two halves the chapter is
named for.

## WHY THE SHEET SAYS OTHERWISE, AND WHAT IT IS ACTUALLY DESCRIBING

`wiring.md` line 4 names trixie 6.18.50+rpt-rpi-v8 and its address table then gives a
Driver and a Device column for every part. Those columns describe the **Yocto 6.6.63-v8
raspberrypi3-64** image, where `st_lsm6dsx_i2c`, `st-magn-i2c`, `st-press-i2c` and `sht4x`
were all found registered on Tuesday 27 October 2026 with `CONFIG_IIO_ST_LSM6DSX=m`. On
that same day the Pi OS card was measured to hold only `dummy` and `stmpe-i2c` under
`/sys/bus/i2c/drivers` with `# CONFIG_IIO_ST_LSM6DSX is not set`.

**This is the second sheet in this volume to claim a binding on an image it names and
was never tested against.** e05's sheet claims the mainline `adxl345` I2C driver binds and
creates an IIO device; that driver is built on neither card. The pattern is a table whose
address column was measured and whose driver column was assumed.

The sheet also says the overlay `.dtbo` is "already on the card's boot partition". It is
not on this card. That half is established for card eplepi only, the overlay being a
per-card file, while the driver absence is a property of the kernel package and therefore
of every trixie card of this version.

## WHAT WOULD UNBLOCK IT

Three routes, none of them cheap, and none taken here:

1. **The Yocto image**, where the four drivers are registered. A different card and a
   different volume's build.
2. **Build the four modules for trixie** out of tree, against the running kernel's
   headers.
3. **Read the parts from user space over I2C** and drop the IIO path, which changes what
   `--live iio` means and touches the shared `io.h` backend, not just this project.

## WHAT THIS DOES NOT ESTABLISH

    established   st_magn, st_pressure and st_lsm6dsx are absent from the trixie
                  kernel package 6.18.50+rpt-rpi-v8; sht4x is present
    established   the iio/magnetometer directory does not exist on that image
    established   the pressure part, which the chapter calls the one number the rule
                  reads, is among the missing
    established   bench-iks4a1.dtbo is not on card eplepi
    NOT SHOWN     the same survey on the Pi 3B+ card this project targets. The kernel
                  package is the same version and the driver result follows from it; the
                  overlay file is per card and was not checked there
    NOT TRIED     any of the three routes above
    NOT MEASURED  anything about the model or the rule. No shield was wired, no event
                  logged, and the 24 hour run was never started
