# e05: st_lsm6dsx builds out of tree against the installed headers, in one make, and its
# table carries BOTH shields' parts. The blocker of five projects was a kernel config and
# not a limit of this bench. Friday 20 November 2026, 12:14

    board      Raspberry Pi 4 Model B Rev 1.4, card eplepi, nothing wired to the header
    kernel     6.18.50+rpt-rpi-v8, Raspberry Pi OS trixie
    headers    linux-headers-rpi-v8 1:6.18.50-1+rpt1, ALREADY INSTALLED, and
               /lib/modules/6.18.50+rpt-rpi-v8/build exists
    source     drivers/iio/imu/st_lsm6dsx/ from raspberrypi/linux branch rpi-6.18.y,
               seven files, 150 kB, fetched into /tmp/stlsm

## THE FOUR PREREQUISITES WERE ALL THERE, AND NOBODY HAD LOOKED

    /lib/modules/$(uname -r)/build     exists
    linux-headers-rpi-v8               installed and matching the running kernel
    CONFIG_IIO=m                       CONFIG_IIO_BUFFER=y
    CONFIG_IIO_KFIFO_BUF=m             CONFIG_IIO_TRIGGER=y
    CONFIG_REGMAP_I2C=m

**Every dependency st_lsm6dsx needs is enabled on this card**, and the headers were on it
before today. The driver's absence was `# CONFIG_IIO_ST_LSM6DSX is not set` in the kernel
package's own configuration and nothing else.

## THE BUILD, AND IT TOOK ONE MAKE

The kernel's own Makefile uses `obj-$(CONFIG_IIO_ST_LSM6DSX)`, which expands to nothing when
that symbol is unset, so it was replaced with four fixed lines:

    st_lsm6dsx-y := st_lsm6dsx_core.o st_lsm6dsx_buffer.o st_lsm6dsx_shub.o
    obj-m += st_lsm6dsx.o
    obj-m += st_lsm6dsx_i2c.o
    obj-m += st_lsm6dsx_spi.o

    make -C /lib/modules/$(uname -r)/build M=/tmp/stlsm modules

**Three modules, no patch, no warning worth the name:** `st_lsm6dsx.ko`,
`st_lsm6dsx_i2c.ko`, `st_lsm6dsx_spi.ko`.

## AND THE TABLE CARRIES BOTH SHIELDS, WHICH REFUTES ME

    modinfo st_lsm6dsx_i2c.ko | grep '^alias: *i2c:'

    i2c:lsm6dsv16x      the IKS4A1's part at 0x6B, WHO_AM_I 0x70
    i2c:lsm6dso16is     the IKS5A1's part at 0x6B, WHO_AM_I 0x22

    st_lsm6dsx.h:37     #define ST_LSM6DSV16X_DEV_NAME  "lsm6dsv16x"
    st_lsm6dsx.h:38     #define ST_LSM6DSO16IS_DEV_NAME "lsm6dso16is"

**`st_lsm6dsx` knows both, so the driver route was open for either shield.** I had written, on
Thursday 19 November 2026 and again on Friday the 20th, that mainline probably splits the
families and that this was the one point favouring the IKS5A1. **It is refuted from the binary
of the module itself.** The 27 October record's statement that `lsm6dso16is` is in the table
was true and incomplete: nobody had looked for the other name, and I built an argument on the
half that had been checked.

Joseph's switch to the IKS5A1 was therefore a free choice and not a necessity, and the
allocation stands on the parts and on e07's need for the STTS22H and the SHT40AD1B, which is
where it started.

## WHAT THIS CHANGES FOR FIVE PROJECTS

e05, e09, e14 and e19 read an accelerometer and e07 reads four parts, and all five were
recorded as blocked on an image with two routes out: four out-of-tree modules, or a user-space
I2C reader that would touch the shared `io.h` and therefore every one of them.

**The first route is now measured and cheap.** `--live iio` can stay exactly as the chapters
write it, and `io.h` is not touched. The same build very probably serves `st_magn` and
`st_pressure`, which would unblock e07's climate half as well, and that is the next thing to
measure rather than to assume.

## WHAT THIS DOES NOT ESTABLISH

    established   st_lsm6dsx, _i2c and _spi build out of tree against the installed
                  headers of 6.18.50+rpt-rpi-v8 with a four-line Makefile
    established   every Kconfig dependency of that driver is enabled on card eplepi
    established   the module's own alias table carries lsm6dsv16x AND lsm6dso16is
    REFUTED       my claim that mainline probably splits the two families, and with it the
                  one point I had offered in the IKS5A1's favour
    NOT LOADED    the module. Nothing is wired to this board, so nothing could bind
    NOT MEASURED  whether it binds and produces an /sys/bus/iio/devices entry. An I2C
                  driver needs an instantiated client: either a devicetree node or
                  echo lsm6dso16is 0x6b > /sys/bus/i2c/devices/i2c-1/new_device
    NOT MEASURED  whether st_magn and st_pressure build the same way, which is e07's half
    NOT MEASURED  anything from any sensor. A module that loads is not a reading
    NOT DECIDED   where these modules live for good. They are in /tmp, which a reboot
                  clears; a card that keeps them needs /lib/modules and a depmod, and that
                  is a change to a card's state and so Joseph's call
