# e06: the repeatability at 200 mm, and the speed window it closes, Friday 30 October 2026, 13:07

    board      Raspberry Pi 3 Model B Plus Rev 1.3, hostname rpi3BV1p2, freshly booted
    sensor     Adafruit VL53L4CD, ADA5396, sensor id 0xebaa at 0x29 on /dev/i2c-1, alone
               on the bus, wiring two of Thursday 29 October 2026, undisturbed
    driver     ST ULD STSW-IMG026 at /home/bing/uld/VL53L4CD_ULD_V2.2.3/VL53L4CD_ULD_Driver
    engine     none: the stub, so no model was loaded and no inference ran
    sampling   interval 0.1 s, timing budget 50 ms
    target     a wall. THE DISTANCE WAS SET BY AIM, NOT BY PLACEMENT: the sensor was
               rotated a millimetre at a time until it read the wanted value, then fixed
               and left untouched for the run
    tree       9dc8c53, which is behind the authoring laptop; e06's code is untouched by
               the commits it lacks
    register   0x0 before and 0x0 after every run; 38.6 to 40.2 C throughout

**This closes the second of e06's four acceptance clauses.** The acceptance reads "200
approaches by hand at three speeds ...; **repeatability at a fixed 200 mm over 100 samples,
mean and spread published**; the decision latency ...; throttle bits `0x0` before and after".
The 200 approaches and the latency are untouched and still wait on cooling.

## The measurement

    n 100   mean 200.56 mm   sd 3.18874 mm

100 samples, **0 with a range status other than 0**, register clean at both ends.

**THE MEAN IS NOT AN ACCURACY FIGURE AND MUST NOT BE READ AS ONE.** The distance was reached
by rotating the sensor against a wall until it read about 200, so the setpoint was chosen by
the instrument being tested. Quoting `200.56` against a nominal `200` and calling the
difference an offset would be circular: it is where the nudging stopped, not a comparison with
an independent length. **No offset is claimed here and none can be from this run.** Measuring
one needs a ruler or a gauge block setting the distance, with the sensor reading the result
rather than defining it.

**The spread is unaffected by that and is the figure the clause asks for.** Repeatability is
variation about the setpoint, whatever the setpoint happens to be, so `sd 3.19 mm` stands on
its own terms.

**And it cost nothing thermally**, which is the whole reason this clause was reachable while
the other three are not. The chapter's step 7 says to run it with the stub "so that no model
time is spent", and with no model there is no load: ten seconds of sampling moved the board
from 38.6 to 39.2 C.

## THE SPEED WINDOW, WHICH IS WHAT THIS FIGURE IS ACTUALLY FOR

The chapter says why it wants this number, and it is not for its own sake:

> The falling run is strict: five ranges each lower than the one before, at 1 mm resolution. A
> hand that moves 1 mm per sample, 10 mm per second, is inside the sensor's own spread and
> will not read as a run; the repeatability test measures that spread, and **the slowest
> approach the rule can see is the one whose step per sample exceeds it**.

At 0.1 s per sample, a hand at speed `v` steps `v / 10` mm between samples. Setting that
against the measured spread gives a floor, and e06's third clause gives a ceiling:

    sensor floor   step must exceed the spread     v > 31.9 mm/s at one sd
                                                   v > 63.8 mm/s at two sd
    model ceiling  600/v must exceed 0.4 + L       v < 47.5 mm/s, with L = 12.24 s measured

    THE USABLE WINDOW ON THIS BOARD   31.9 to 47.5 mm/s, a ratio of 1.49
    AT TWO STANDARD DEVIATIONS        EMPTY

**A ratio of 1.49 is not three speeds. It is barely one.** And the one-sd reading is the
generous one: the rule needs **five consecutive** readings each lower than the last, and a
step of one sd will not deliver that reliably against noise of one sd. The honest requirement
sits nearer two sd, where board B has no window at all.

### Every speed in the run sheet is wrong

The run sheet written on Thursday 29 October 2026 proposed 25, 50 and 100 mm/s for this board,
chosen from the latency threshold alone because the spread had not been measured. All three
fail, and they fail at opposite ends:

    25 mm/s    step 2.5 mm against a 3.19 mm spread   BELOW THE FLOOR, the rule cannot see it
    50 mm/s    600/v = 12.0 s against 0.4+L = 12.64   THE MODEL CANNOT KEEP UP
    100 mm/s   600/v = 6.0 s                          THE MODEL CANNOT KEEP UP

That sheet was marked DO NOT RUN THIS YET for a thermal reason. It turns out it should not
have been run for a second, independent reason, and **ten seconds of a fixed aim at a wall is
what found it**. Had cooling arrived first, the afternoon would have been spent producing a log in
which the slow speed showed no falling runs and the two fast speeds showed no model answers,
and the cause would have been argued about afterwards.

### Which board e06 should run on, as a measured recommendation

The floor is a property of the sensor and does not move between boards. Only the ceiling does,
because it is set by the decision latency.

    configuration                              L        ceiling    2 sd window
    board B, 4 threads, live, MEASURED        12.24 s   47.5 mm/s  EMPTY
    board A, 2 threads, replay, MEASURED       8.51 s   67.3 mm/s  63.8 to 67.3, ratio 1.06
    board A, 4 threads, PROJECTED              5.88 s   95.6 mm/s  63.8 to 95.6, ratio 1.50

**So e06 should run on board A, at four threads, and the thread count is not a detail.** The
measured two-thread figure leaves a two-sd window of ratio **1.06**, which is one speed and
not three. Only the four-thread projection opens it to 1.50.

Both board A figures deserve their labels. The 8.51 s is **measured**, the sum of 4388.0 ms
prompt and 4123.9 ms generate from the replay run of Thursday 29 October 2026 at two threads
with the same 64-token budget. The 5.88 s is **projected** from it using the thread ratios
measured on e03 on Tuesday 27 October 2026, 1.94 on prompt and 1.14 on generation, and has
never been measured for e06 at all.

**That projection is now load bearing and should not be**, because it decides whether e06 has
a usable window. Measuring board A's L for e06 at four threads costs one replay run with the
real model and no sensor, since latency is a property of the board and the model rather than
of the target. It should be done before any speed is chosen.

## A SECOND FINDING, FROM A RUN THAT WAS A MISTAKE

While the sensor was being rotated toward 200 mm it passed through an aim that read about
300, and that log was kept rather than overwritten:

    n 100   mean 303.82 mm   sd 6.17486 mm   below 300 mm: 29 of 100

**The rule's threshold is 300 mm, and a completely stationary object straddled it 29 percent
of the time.** The rule is correct on every sample; the flicker is the instrument. A hand
paused anywhere near 300 mm will therefore generate rule transitions that no movement caused,
which bears directly on the agreement table e06's first clause asks for.

This was not planned and is kept as `e06_repeat_304mm.csv`, named for what it measured
rather than what it was aimed at. The same obliquity caveat applies to it, more strongly,
because the angle is larger at the greater distance.

## The spread against distance, from four fixed targets

    distance        n     mean        sd        bad statuses
    about 24 mm    100    24.05 mm    1.90 mm   0
    about 24 mm    100    23.66 mm    1.77 mm   0
    200 mm         100   200.56 mm    3.19 mm   0
    about 304 mm   100   303.82 mm    6.17 mm   0

The spread grows with distance, roughly doubling from 24 to 200 mm and again to 304. That is
expected for this sensor and it is why the chapter insists the figure be taken at 200 mm
rather than wherever is convenient: a spread measured close in would understate the floor and
the speed window computed from it would be wrong in the permissive direction.

The two runs at 24 mm agree to 0.13 mm on the spread, which is the only repeatability check
available on the repeatability measurement itself.

## THE OBLIQUITY CAVEAT, WHICH FOLLOWS FROM THE METHOD

Setting the distance by rotating the sensor means the beam meets the wall **at an angle**
rather than square on, and the angle grows as the distance is increased. The VL53L4CD
illuminates a cone of roughly 27 degrees, so an oblique wall returns a spot that is stretched
along the surface, with near and far edges at different true distances inside one measurement.

**That should widen the spread rather than narrow it.** So `sd 3.19 mm` is most likely an
upper bound on what a square-on target would give, and every figure derived from it is
conservative in the safe direction: the sensor floor of 31.9 mm/s is if anything too high, and
the usable speed window computed below is if anything too narrow.

A square-on target at a ruler-set distance would settle both this and the offset in one run.
It is not required by the clause, which asks only for the spread, and it is worth doing if the
window arithmetic below ever needs to be defended tightly.

## Four attempts it took, and why each failed

Worth recording because the failures were all aim and none were the instrument.

1. **A hand at about 27 mm.** Mean 26.65, sd 5.86, and the trace wandered from 18 to 52 mm.
   A hand cannot hold still to better than the spread being measured, so target motion and
   sensor spread were confounded and the larger number is the hand.
2. **A fixed surface at about 24 mm, twice.** Stable and self-consistent, and nowhere near
   200 mm. The two runs agreeing to a third of a millimetre is what showed the beam was
   parked on something close rather than on the intended target.
3. **An aiming check into open space** returned 87 mm, which proved the sensor responds to
   what is in front of it and that the beam was simply not landing where it was assumed to.
4. **Rotating the sensor against a wall**, a millimetre at a time, watching the live reading
   climb until it sat near the wanted value, then fixing it. That is what finally worked, and
   it is the method the record above is careful about.

The sensor was never at fault: **430 samples across all runs, zero with a range status other
than 0.**

## Two things that bit, and a label I do not trust

**The binary refused `--stub` and was right to.** `build/` on this board is a real-engine
build, and the chapter's step 7 needs the stub. A second configure into `build-stub` with
`-DEDGELLM_STUB_MODEL=ON`, the I2C backend and the ULD path solved it, and leaves the
real-engine build untouched for the eventual 200 approaches. The configure was verified to
have kept the target rather than dropping it silently:

    edgellm-e06: live sampler on, ULD from .../VL53L4CD_ULD_Driver, header VL53L4CD_api.h

**I counted the threshold crossings by eye and was wrong by more than three times**, reading
8 where the `awk` found 29. The scroll is not a count, and this volume has written that down
before.

**And the summary line says `source replay` on a run driven by a live sensor.** It may be
literally true of the internals, since the live sampler writes into a pipe the loop reads with
its replay reader, but a log claiming "replay" for a run where a sensor was physically present
will mislead a later reader. Not chased here; recorded.

## What is established and what is not

    established   clause two: 100 samples at 200 mm, mean 200.56 mm, sd 3.19 mm
    established   the sensor floor is 31.9 mm/s at one sd, 63.8 at two
    established   the usable window on board B is 31.9 to 47.5 mm/s, and empty at two sd
    established   all three speeds in Thursday's run sheet are outside it
    established   a stationary target at 304 mm crosses the rule's threshold 29 percent
    established   the spread grows with distance across four fixed targets
    NOT shown     e06's first clause, the 200 approaches, still blocked on cooling
    NOT shown     e06's third clause, the latency, which needs the model to run
    NOT measured  board A's L for e06 at four threads, which is the figure the board
                  recommendation rests on; 8.51 s is measured at TWO threads and leaves
                  a two-sd window of ratio 1.06, which is one speed and not three
