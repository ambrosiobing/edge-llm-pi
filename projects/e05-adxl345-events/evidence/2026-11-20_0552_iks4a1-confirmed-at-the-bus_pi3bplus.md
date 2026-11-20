# e05: the X-NUCLEO-IKS4A1 answers on seven addresses on board B, and 0x6B names itself.
# The substitution away from the ADXL345 is wired and identified.
# Friday 20 November 2026, 05:52

    board      Raspberry Pi 3 Model B Plus Rev 1.3, read from
               /proc/device-tree/model and not from the card name
    card       rpi3BV1p2, Raspberry Pi OS trixie, kernel 6.18.50+rpt-rpi-v8
    hat        none seated; the Explorer700 is off the board, which the scan shows
    shield     X-NUCLEO-IKS4A1 on four flying leads
    decision   Joseph, Thursday 19 November 2026: the three projects e05, e09 and e14
               take the IKS4A1

## THE WIRING, FOUR LEADS, AND WHY NOT FIVE

    header 17   3V3           shield CN6 pin 4
    header  3   GPIO2, SDA1   shield CN5 pin 9  (D14)
    header  5   GPIO3, SCL1   shield CN5 pin 10 (D15)
    header  9   GND           shield CN6 pin 6

**e07's sheet names five leads; the fifth is INT1 on header 18 and none of these three
projects reads it.** e05 polls, e09 polls, e14 polls, and on e14's board header 18 is the
MCP2518FD's interrupt anyway. Four leads also beat the ADXL345's seven, because the two
straps that part needed, CS to 3V3 and SDO to GND, do not exist on a shield.

**3V3 on 17 and GND on 9 rather than on 1 and 6**, which is the arrangement e09's sheet
already carries because the 3.5 inch panel takes pins 1 and 6. The same four leads therefore
serve e09's sitting without moving, and serve e05 and e14 just as well, since a board has two
3V3 pins and eight grounds.

## THE SCAN, AND IT IS THE POSITIVE CONTROL

    i2cdetect -y 1

         0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
    10: -- -- -- -- -- -- -- -- -- 19 -- -- -- -- 1e --
    30: -- -- -- -- -- -- -- -- 38 -- -- -- -- -- -- --
    40: -- -- -- -- 44 -- -- -- -- -- -- -- -- -- -- --
    50: -- -- -- -- -- -- -- -- -- -- -- -- -- 5d -- --
    60: -- -- -- -- -- -- -- -- -- -- 6a 6b -- -- -- --

**Exactly seven, and they are e07's measured table of Wednesday 30 September 2026 character
for character:** 0x19 LIS2DUXS12, 0x1E LIS2MDL, 0x38 STTS22H, 0x44 SHT40AD1B, 0x5D LPS22DF,
0x6A LSM6DSO16IS, 0x6B LSM6DSV16X. **No eighth address**, so the Explorer700 is off: it would
have added its PCF8574, its BMP280 and its DS3231.

## AND THE PART NAMES ITSELF, WHICH THE ADDRESS CANNOT

    i2cget -y 1 0x6b 0x0f    ->    0x70

**0x70 is the LSM6DSV16X, so this is the IKS4A1.** Had it read 0x22 the part would be the
LSM6DSO16IS and the shield the IKS5A1, because the two carry a shared part at swapped
addresses: 0x6B holds 0x70 on the IKS4A1 and 0x22 on the IKS5A1, measured Monday 9 November
2026. The three further discriminators agree: 0x38 and 0x44 are present, and the pressure part
sits at 0x5D rather than 0x5C.

**This is the first time the IKS4A1 has been identified on a Raspberry Pi of this bench.** The
September table was measured in the Linux volume's project 10 and the November one by the
NUCLEO-H7A3ZI-Q itself.

## TWO READINGS THAT WOULD HAVE READ AS A FAULT AND ARE NOT ONE

**The first attempt ran on the Yocto card and found no bus.** `ls /dev/i2c-*` gave only
`/dev/i2c-11` and `i2cdetect -y 1` said the node does not exist. I wrote that the ARM I2C
controller was not enabled and that `dtparam=i2c_arm=on` was missing. **That was wrong.**

    cat /sys/bus/i2c/devices/i2c-11/name    ->    bcm2835 (i2c@7e205000)

`7e205000` **is** the ARM controller on GPIO2 and GPIO3. The header bus was there all along
and numbered 11 on that image. Nothing in `config.txt` needed changing, and the step that
settled it was asking the bus for its own name instead of inferring from its number.

**The lesson, and it is this week's third of its kind: a bus number is not a bus identity.** A
kernel version string did not identify an image on Thursday 19 November 2026, a card name does
not identify a board, and a bus number does not identify a controller. In all three cases the
device answers when asked directly: `/proc/config.gz`, `/proc/device-tree/model`,
`/sys/bus/i2c/devices/i2c-N/name`.

The card was then replaced with rpi3BV1p2 and the bus appeared as `/dev/i2c-1` and
`/dev/i2c-2`, where 2 is the HDMI DDC bus and 1 is the header.

## WHAT THIS DOES NOT ESTABLISH

    established   the IKS4A1 is wired to board B on four leads, 17, 3, 5 and 9, and
                  answers on all seven of its addresses
    established   0x6B reads WHO_AM_I 0x70, so the part is the LSM6DSV16X and the shield
                  is the IKS4A1 and not the IKS5A1
    established   no HAT is on board B; the scan carries no eighth address
    established   i2c@7e205000 is the header controller whatever number it is given
    NOT MEASURED  any reading from any part of the shield. Seven addresses answering is
                  a contact proof, not a sensor proof
    NOT AVAILABLE a driver. No card of this bench carries an ST IIO driver, measured
                  Friday 13 and Thursday 19 November 2026, so the three projects need a
                  user-space I2C reader or an out-of-tree module build, and all three
                  currently say --live iio
    NOT CHECKED   the rule thresholds. e05 decides in g, below 0.3 and above 2, against
                  the ADXL345's 0.0383 m/s2 per count; the LSM6DSV16X's full scale is
                  selectable and the thresholds have to be read against it
    NOT RESOLVED  which board e05 and e09 belong to. Their sheets name a Raspberry Pi 3
                  Model B Rev 1.2; the shield is on a 3 Model B PLUS Rev 1.3
