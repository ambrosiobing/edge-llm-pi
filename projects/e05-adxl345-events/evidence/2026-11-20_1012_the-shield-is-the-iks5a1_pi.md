# e05: the shield for the four accelerometer projects is the X-NUCLEO-IKS5A1 and not the
# IKS4A1. Joseph's decision, Friday 20 November 2026, 10:12.

    decided    Joseph, Friday 20 November 2026, after the IKS4A1 had been wired and
               identified at the bus the same morning
    affects    e05, e09, e14, e19, and through the allocation below e07 and e11
    part read  LSM6DSO16IS at 0x6B, WHO_AM_I 0x22 at register 0x0F
    wiring     unchanged: four leads, 3V3, SDA, SCL, GND to CN6 pin 4, CN5 pin 9,
               CN5 pin 10, CN6 pin 6

## THE ALLOCATION, AND A CORRECTION OF MINE THAT CHANGED THE ARGUMENT

    IKS5A1   e05, e09, e14, e19 on the Pi, and e11 on the Nucleo
             one shield, five projects, two hosts; it moves between sittings
    IKS4A1   e07 on the Pi, uncontested

**I argued against this decision with a wrong reason and the correction is the better case for
it.** I wrote that if the Pi side takes the IKS5A1, e07 is left without a shield. The opposite
holds: e07 needs the IKS4A1, which this decision frees, so e07 gains an uncontested shield.
**The real cost is e11**, which reads the IKS5A1 on the NUCLEO-H7A3ZI-Q over USB CDC, and must
now share one shield with four Pi projects across two hosts.

That cost is bounded by a rule the volume already has: the two shields are never fitted
together, one is bagged before the other leaves its bag. Moving one shield between a Nucleo and
a Pi between sittings is not that case and costs four leads.

## WHAT IS ALREADY MEASURED ABOUT THIS SHIELD

Tuesday 27 October 2026, on a Raspberry Pi 3 Model B Plus with e07's lead plan, identified by
command set and not by address:

    0x19   reg 0x0F = 0x47   LIS2DUXS12; answers i2cdetect -r only
    0x1e   reg 0x4F = 0x40   LIS2MDL; its WHO_AM_I is at 0x4F and not 0x0F
    0x5c   reg 0x0F = 0xb4   LPS22DF or LPS28DFW, the pressure family
    0x6a   reg 0x0F = 0x73   an IMU no kernel on this bench knows
    0x6b   reg 0x0F = 0x22   LSM6DSO16IS

**Five parts, not the IKS4A1's seven.** The two shields share only the LIS2MDL at 0x1E. Absent
here: 0x38 STTS22H and 0x44 SHT40AD1B, and the pressure part sits at 0x5C rather than 0x5D.
**0x6A is unusable**: its 0x73 is in no driver's table on this bench, which leaves 0x6B as the
one accelerometer, and one is what all four projects need.

## WHAT CHANGES IN THE FOUR PROJECTS

    the part                 LSM6DSV16X 0x6B 0x70   ->   LSM6DSO16IS 0x6B 0x22
    the identification       i2cget 0x6b 0x0f = 0x70  ->  = 0x22
    the wiring               unchanged, four leads, both arrangements
    the scan expectation     seven addresses          ->   five
    the driver situation     unchanged: none on any card of this bench

**And the scan is simpler on this shield.** On the IKS4A1 neither `i2cdetect` mode showed all
seven, because 0x44 answers a write probe only and 0x19 a read probe only. The IKS5A1 has no
0x44, so **`i2cdetect -y -r 1` alone shows all five**, and the default mode is the one that
misses 0x19 intermittently. The union is still the honest control; the difference is that one
mode now suffices.

## ONE POINT THAT FAVOURS THIS DECISION AND IS STILL UNVERIFIED

`st_lsm6dsx`'s device table **includes `lsm6dso16is`**, measured on Tuesday 27 October 2026,
and that is exactly the part at 0x6B on this shield. Whether it also includes `lsm6dsv16x`, the
IKS4A1's part, **is not known**: `modinfo` could not answer because the module exists on no
card of this bench, and my guess that mainline splits the families is a guess that failed at
this very point on Thursday 19 November 2026.

So **if the out-of-tree module route is ever taken, this decision may be the one that works and
the other the one that does not**, and that is measurable before the work: build
`st_lsm6dsx` out of tree once and ask `modinfo`. Until then it is a reason to prefer this
shield and not a reason to claim it.

## AND THE SHIELD IS NOW ON A BUS, AT 11:23 THE SAME DAY

Wired to board B, a Raspberry Pi 3 Model B Plus Rev 1.3 with card `rpi3BV1p2`, on the four
leads of e05's arrangement, header 1, 3, 5 and 6:

    i2cdetect -y 1       19 1e 5c 6a 6b    five
    i2cdetect -y -r 1       1e 5c 6a 6b    four, no 0x19
    i2cget -y 1 0x6b 0x0f    ->    0x22

**0x22 is the LSM6DSO16IS, so this is the IKS5A1**, and the five addresses are the survey of
Tuesday 27 October 2026 character for character: 0x19, 0x1E, 0x5C, 0x6A, 0x6B. Nothing at 0x38
or 0x44 and the pressure part at 0x5C, which are three of the discriminators between the two
shields.

**A prediction of mine was refuted by the first scan that tested it, twenty minutes after I
wrote it into four wiring sheets.** I wrote that `i2cdetect -y -r 1` alone would show all five
on this shield, because 0x19 answers a read probe only, which is what e07's table records and
what this morning's IKS4A1 scans showed: `-r` had 0x19 in 2 of 2 and the default in 1 of 2.
Here the default has it and `-r` does not. **Three readings of that one address, three different
behaviours**, and one scan per mode was never enough to promise either. The sheets now ask for
both modes and call the union the control, which is what they should have said from the first
record of this morning.

0x19 is the LIS2DUXS12 and no project of this volume reads it, so the cost of the wrong
prediction is a pre-flight that would have reported a fault where there is none.

## WHAT THIS DOES NOT ESTABLISH

    established   the decision: e05, e09, e14 and e19 read the IKS5A1's LSM6DSO16IS at 0x6B
    established   the allocation leaves e07 the IKS4A1 uncontested and makes e11 share
    established   the IKS5A1's five parts, measured Tuesday 27 October 2026 by command set
    established   the wiring is the same four leads on the same Arduino connectors
    CORRECTED     my argument that this decision leaves e07 without a shield; it does the
                  opposite, and the cost falls on e11
    established   the IKS5A1 on board B at 11:23 on Friday 20 November 2026: five
                  addresses over two scan modes and 0x6B reading 0x22
    REFUTED       my own prediction, written into four sheets twenty minutes earlier, that
                  one scan mode would show all five; 0x19 has now behaved three ways in
                  three readings and no mode is promised
    NOT VERIFIED  whether st_lsm6dsx knows lsm6dsv16x, which is the one point that would
                  make this decision better than a preference
    NOT AVAILABLE a driver, on either shield, on any card
