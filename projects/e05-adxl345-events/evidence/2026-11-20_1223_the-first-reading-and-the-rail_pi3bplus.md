# e05: the first sensor reading of this volume from an X-NUCLEO shield, gravity to 2.1 per
# cent, and a rule that sat on the rail of the default range.
# Friday 20 November 2026, 12:23

    board      Raspberry Pi 3 Model B Plus Rev 1.3, card rpi3BV1p2, from
               /proc/device-tree/model and not from the card name
    kernel     6.18.50+rpt-rpi-v8, Raspberry Pi OS trixie
    shield     X-NUCLEO-IKS5A1 on four flying leads, header 1, 3, 5, 6
    driver     st_lsm6dsx built out of tree the same day; see
               evidence/2026-11-20_1214_the-driver-builds-out-of-tree_pi4b.md
    client     instantiated by hand, no overlay:
               echo lsm6dso16is 0x6b > /sys/bus/i2c/devices/i2c-1/new_device

## IT BINDS, AND THE PART IDENTIFIES ITSELF THROUGH THE DRIVER

    st_lsm6dsx_i2c 1-006b: supply vdd not found, using dummy regulator
    st_lsm6dsx_i2c 1-006b: mounting matrix not found: using identity...
    i2c i2c-1: new_device: Instantiated device lsm6dso16is at 0x6b

**No `unsupported whoami`**, so the driver read the identity register, found `0x22` and accepted
the part. That is a second, independent identification of this shield after the `i2cget`
reading, and this one is made by the code that will do the work.

The two messages about `vdd` and the mounting matrix are informational: the client was made by
hand, so there is no regulator and no orientation in any devicetree, and the driver takes
defaults. `taints kernel` is the ordinary mark for a module from outside the tree.

## TWO IIO DEVICES, AND THE ACCELEROMETER IS NOT NUMBER ZERO

    iio:device0   lsm6dso16is_gyro     in_anglvel_x_raw, y, z
    iio:device1   lsm6dso16is_accel    in_accel_x_raw, y, z

**FOUR projects hard-code `iio:device0` and would therefore read the gyroscope.** The first
version of this record said three; e19 was found on the fourth reading of the four files, which
is the second time today that project has been missed in a list.

    e05   --live iio /sys/bus/iio/devices/iio:device0:0.02
    e09   --live iio /sys/bus/iio/devices/iio:device0:0.1
    e14   --iio /sys/bus/iio/devices/iio:device0
    e19   --live iio /sys/bus/iio/devices/iio:device0:0.1

Their rules decide on acceleration in g and would be handed degrees per second. **e07 already
does this correctly** and says why in its own chapter: the IIO devices are "found by their
`name` attribute" because the numbers change with probe order. The other three fixed the
number. A device number is not a device identity, which is this week's fifth label of that
kind.

The resolution that belongs in the pre-flight of all four sheets:

    ACC=$(for d in /sys/bus/iio/devices/iio:device*; do \
            grep -qx lsm6dso16is_accel $d/name && echo $d; done)

## THE FIRST READING: GRAVITY TO 2.1 PER CENT

At the default range, `in_accel_scale` 0.000598205 m/s2 per count, 12.5 Hz:

    raw     x = 162      y = -313      z = 16741
    m/s2    x = 0.0969   y = -0.1872   z = 10.0146
    magnitude 10.0167 m/s2 = 1.0215 g against 9.80665

The shield lay flat with Z up. **2.1 per cent high, which is inside the ordinary tolerance of
such a part**, and it is the first number this volume has from either X-NUCLEO shield.

## AND THE RANGE HAD TO MOVE, BECAUSE A RULE SAT ON THE RAIL

    0.000598205   +-2 g    16393 counts per g
    0.001196411   +-4 g     8196
    0.002392822   +-8 g     4098
    0.004785645   +-16 g    2049

e05's rule is "TAP when the peak magnitude is above 2 g". At the default range **2 g is
32786 counts against a 16-bit signed rail of 32768**, so the value cannot exceed the threshold
and **that rule could never have fired**. Nobody had computed this: the chapter's thresholds
were written against the ADXL345's 0.0383 m/s2 per count and carried over unchanged.

Set to +-4 g, 2 g sits at 16393 of 32768, half scale, and the 0.3 g freefall threshold still
has 2459 counts beneath it.

**The change was verified by a prediction rather than by inspection.** Doubling the range must
halve the raw value and leave the product alone:

    predicted   z about 8370
    measured    8387, 8383, 8376
    product     8387 x 0.001196411 = 10.0344 m/s2 against 10.0146 at the other range

**Consistent to two parts in a thousand across two ranges**, which confirms the IIO convention
for this part instead of assuming it.

## THE NOISE, AND A FIGURE I SAID MOVES AND WHICH DOES NOT

Three consecutive readings at 52 Hz, +-4 g:

    z   8387  8383  8376    span 11 counts = 0.0132 m/s2 = 0.00134 g
    x     67    67    72    span  5
    y   -164  -165  -165    span  1

**1.3 mg of noise against a 0.3 g threshold is a factor of 223.** Both of e05's thresholds are
reachable and comfortably resolved at this range.

`sampling_frequency_available` reads 12.5, 26, 52, 104, 208, 416, 833, so **50 Hz does not
exist on this part** and neither does the 10 Hz of e09 and e19.

**A first version of this record said that moves e05's acceptance figure. It does not, and the
correction matters.** The `:0.02` in `--live iio <path>:0.02` is the **reader's poll interval**,
not the part's output rate. The log's rows come from the poller, so the acceptance check that
"the median must sit at the configured 0.02 s" is a statement about the program and is
unaffected by the part's rate. What the part's rate decides is only whether each poll finds
fresh data, and for that the rate must be **above** the poll rate: 52 Hz over e05's 50 Hz, and
12.5 Hz over e09's and e19's 10 Hz. Both hold.

So nothing in the acceptance moves. **Two configuration requirements appear instead**, which is
a different kind of statement:

    1  the range must be set to +-4 g or wider, or e05's TAP rule cannot fire
    2  the output rate must be set at or above the poll rate; there is no exact 50 or 10,
       and 52 and 12.5 are the nearest above

I wrote the consequence before working it through, for the second time today.

## WHAT THIS DOES NOT ESTABLISH

    established   st_lsm6dsx binds to lsm6dso16is at 0x6b and creates two IIO devices
    established   the accelerometer is iio:device1 here and the gyroscope iio:device0,
                  and three projects hard-code device0
    established   gravity reads 1.0215 g at +-2 g and 1.0235 g at +-4 g
    established   the range is selectable from sysfs, four ranges, and the product of raw
                  and scale is preserved across two of them to 0.2 per cent
    established   e05's 2 g TAP threshold is UNREACHABLE at the default +-2 g range and
                  reachable at +-4 g
    established   noise is 11 counts on z, 1.3 mg, over three reads at 52 Hz
    established   50 Hz is not available on this part, nor 10; 52 and 12.5 are the
                  nearest above, which is what matters, because the rate only has to
                  exceed the reader's poll interval
    WITHDRAWN     my claim that this moves e05's acceptance figure for sample spacing.
                  That figure is about the poller and not about the part
    NOT MEASURED  anything in motion. Three static reads are not a tap, a tilt or a
                  freefall, and the hundred hand-made events are untouched
    NOT MEASURED  the gyroscope, which no project reads
    NOT RUN       any project. No binary has read this device
    NOT PERSISTENT the module, the client and the two attributes. All three live in
                  /tmp or in sysfs and a reboot clears them; what a card keeps is a
                  change to that card's state and so Joseph's call
    NOT FIXED     the four run lines. They still name iio:device0
    NOT SET       the range and rate persistently. Both were written to sysfs by hand and
                  a reboot clears them
