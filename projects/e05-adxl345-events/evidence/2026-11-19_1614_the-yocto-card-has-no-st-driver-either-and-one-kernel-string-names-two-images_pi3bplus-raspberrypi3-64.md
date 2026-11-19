# e05: the Yocto card carries no ST IIO driver either, so no card on this bench has one,
# and one kernel string names two different images. Thursday 19 November 2026, 16:14

    board      Raspberry Pi 3 Model B Plus Rev 1.3, card raspberrypi3-64
    kernel     6.6.63-v8, hostname raspberrypi3-64, BusyBox userland
    hat        Waveshare Explorer700 seated; no ST shield on the bus during this block
    clock      Thursday 19 November 2026, 16:14 WEST read on the authoring laptop. The
               board's own clock was not read in this block and is not claimed here.
    question   which WHO_AM_I values `st_lsm6dsx_i2c` knows on this card, which was to
               decide the IKS4A1 against the IKS5A1 for e05, e09 and e14

## THE DRIVER IS ABSENT, AND THE RUNNING KERNEL'S OWN CONFIGURATION SAYS SO

From `/proc/config.gz`, which is the configuration of the kernel that is running:

    CONFIG_SENSORS_SHT4x=m
    # CONFIG_IIO_ST_LSM6DSX is not set
    # CONFIG_IIO_ST_MAGN_3AXIS is not set
    # CONFIG_IIO_ST_PRESS is not set

**"is not set" excludes both `y` and `m`**, so neither a module nor a built-in path is left
open, and `modprobe st_lsm6dsx_i2c` returns 1 with "not found in directory
/lib/modules/6.6.63-v8". A `find` over the whole module tree for `st_lsm6dsx*`, `st_magn*`,
`st_press*` and `sht4x*` returns nothing.

**So no card on this bench carries an ST IIO driver.** The trixie card was measured on
Friday 13 November 2026 and holds none; this Yocto card holds none either.

## ONE KERNEL STRING AND ONE HOSTNAME NAME TWO DIFFERENT IMAGES

    Tuesday 27 October 2026    Yocto 6.6.63-v8, raspberrypi3-64
                               registered: dummy sht4x st-magn-i2c st-press-i2c
                                           st_lsm6dsx_i2c stmpe-i2c
                               CONFIG_IIO_ST_LSM6DSX=m, _I2C=m, _SPI=m

    Thursday 19 November 2026  Yocto 6.6.63-v8, raspberrypi3-64
                               registered: dummy stmpe-i2c
                               # CONFIG_IIO_ST_LSM6DSX is not set
                               not one of the four modules exists in the tree

**Same `uname -r`, same hostname, opposite answer.** Both readings are in this repository
and they cannot both describe the card now in the board. Three explanations stand open and
this record picks none of them: the card was written again between the two dates; there is a
second Yocto card carrying the same version string and the same hostname; or the October
configuration lines were read from a build tree rather than from the running board, in which
case the registered-driver list of that day came from somewhere this record cannot name.

The question for the bench, which decides between them, is simply whether the card in the
board today is the same physical card as on Tuesday 27 October 2026.

**The finding that survives whichever it is: a kernel version string and a hostname do not
identify an image.** Every driver claim in this volume is dated and carries its card already;
from today it has to carry something that distinguishes two cards of the same version, and
the configuration excerpt is that something.

## THE MODULE TREE CONTRADICTS ITS OWN CONFIGURATION

`CONFIG_SENSORS_SHT4x=m` is set and **no `sht4x` module exists anywhere under
`/lib/modules`**. `modules.alias` holds 37 lines in total, and `kernel/drivers/iio/` does not
exist as a directory. This image was built with a configuration whose modules it does not
ship.

That matters for the method and not only for this card: on an image like this, **a search of
the module tree is not a test of the kernel's capability in either direction.** The
configuration excerpt is the instrument and `modprobe` is the test.

## TWO FALSE NEGATIVES THIS CONTROL CAUGHT

The block before this one grepped `modules.alias` for `lsm6ds`, found nothing, and listed
`/sys/bus/i2c/drivers` as `dummy stmpe-i2c`. **Neither reading was evidence.**

- `modules.alias` holds 37 lines. A grep that finds nothing in a file with almost nothing in
  it has not measured absence. Counting the lines first is what turned that into a fact.
- `/sys/bus/i2c/drivers` lists drivers that are **loaded**, and a module loads when a device
  claims it. On Tuesday 27 October 2026 the IKS5A1 hung on the bus; today no ST part does.
  The short list was never an absence proof, and reading it as one repeats the mistake this
  bench made four times on Friday 13 November 2026 and Saturday 14 November 2026.

The configuration excerpt and the `modprobe` return code are the two readings that carry the
claim, and both were in the same block only because the earlier one had been wrong.

## WHAT THIS DECIDES FOR THE SUBSTITUTION, WHICH IS NOT WHAT IT WAS ASKED TO DECIDE

The planned first step was to read `st_lsm6dsx_i2c`'s device table and let it choose the
shield: the IKS4A1 if `lsm6dsv16x` is in it, the IKS5A1 if only `lsm6dso16is` is. **That step
has no answer on either card, because the driver is on neither.** The question was
unanswerable with the hardware present and, as it turns out, also unnecessary.

Route 1 of the three e07 lists under WHAT WOULD UNBLOCK IT, "the Yocto image, where the four
drivers are registered", **is refuted as of today.** Two remain: build the modules out of
tree against the running kernel's headers, or read the parts from user space over I2C and
drop the IIO path.

With the driver route closed on every card, the shield choice is decided by the parts the
chapters need rather than by driver support, and then **the IKS4A1 is the one to fit**: e05,
e09 and e14 each need one accelerometer, which both shields carry, while e07 needs pressure,
humidity and temperature as well, and the IKS5A1 has nothing at 0x38 or 0x44. One shield then
serves four projects and never has to be moved inside a sitting, which the volume's own rule
that the two shields are never fitted together makes worth something. This is a
recommendation and not a measurement, and the decision is Joseph's.

## WHAT THIS DOES NOT ESTABLISH

    established   the running Yocto kernel 6.6.63-v8 on card raspberrypi3-64 has
                  IIO_ST_LSM6DSX, IIO_ST_MAGN_3AXIS and IIO_ST_PRESS all unset
    established   modprobe st_lsm6dsx_i2c returns 1 on that card and no such module
                  exists in /lib/modules
    established   no card on this bench carries an ST IIO driver
    established   that card's module tree ships no sht4x module although its own
                  configuration sets CONFIG_SENSORS_SHT4x=m, and modules.alias holds
                  37 lines
    established   a kernel version string plus a hostname does not identify an image:
                  two readings of 6.6.63-v8 raspberrypi3-64 disagree on the config
    REFUTED       e07's unblocking route 1, the Yocto image, as of today
    NOT ESTABLISHED  which of the three explanations for the disagreement holds; the
                  bench question is whether this is the same physical card as on
                  Tuesday 27 October 2026
    NOT MEASURED  which WHO_AM_I values st_lsm6dsx knows, the question this block was
                  written for. No card on this bench can answer it.
    NOT MEASURED  anything about the shields themselves; none was on the bus
    NOT A MEASUREMENT  the recommendation to fit the IKS4A1. It follows from the parts
                  tables of Tuesday 27 October 2026 and Friday 6 November 2026, not
                  from anything read today.
