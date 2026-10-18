# e02 clause two, the pair, Sunday 18 October 2026

Written by hand, like the two files beside it, because e02 has no runner as e01 does. Both
halves were measured in one sitting on one supply at one thread count, which is what the
acceptance asks for and what no earlier attempt had.

| | |
|---|---|
| Date | Sunday 18 October 2026, 21:47:21 to 22:08:21 local |
| Board | Raspberry Pi 4 Model B, 8 GB, card `eplepi`, user `bing`, bare, no heatsink or fan |
| Supply | ISY IAC-5065, 65 W USB-C charger, 5 V rail labelled 5.0 V at 3.0 A, 15.0 W |
| Model | `qwen2.5-0.5b-instruct-q4_k_m.gguf`, 491400032 bytes, `n_ctx` 512 |
| Threads | 2 |
| Rows | 100 each pass, the first hundred of `replay/synthetic_1000.csv` |
| Full ARM clock | 1.800 GHz, read with `vcgencmd measure_clock arm` |
| Board revision | `d03114`, Raspberry Pi 4 Model B Rev 1.4, 8 GB, Sony UK |
| Firmware | Monday 14 September 2026, `a86983925695a7e63166327d7c002d64040ed31d` |
| Clock setting | `arm_boost=1` in `/boot/firmware/config.txt` |
| Result | ratio **4.47** against an acceptance of at least 2.0 |
| Verdict | **measured, not accepted**: the register ended at `0x80000` |

## The two passes

| | restore | cold |
|---|---|---|
| started | 21:47:21 | 21:55:46 |
| finished | 21:52:23 | 22:08:21 |
| mean prompt_ms | 1325.6 | 5927.5 |
| mean generate_ms | 1524.8 | 1560.6 |
| VmRSS kB | 579892 | 581212 |
| temperature before | 53.5 degrees C | 56.9 degrees C |
| temperature after | 74.5 degrees C | 80.3 degrees C |
| `get_throttled` after | `0x0` | `0x80000` |
| log | `e02_variant_a_restore_pair.csv` | `e02_variant_a_cold_pair.csv` |

5927.5 divided by 1325.6 is 4.47. State restore saves 4.6 seconds on every decision.

`get_throttled` read `0x0` before the first pass. The board had been rebooted at 21:38 to
clear the sticky bit 19 left by the discarded single cold pass earlier the same evening.

`rows without a literal 0` is this binary's own count and is not the clause-one gate. That
gate is the agreement tool over the thousand-row run, where the 949 malformed rows of
Saturday 17 October 2026 were counted.

## Three reproducibility checks

These are better grounds for believing the pair than the size of the ratio is.

| What | Compared with | Difference |
|---|---|---|
| restore prompt 1325.6 ms | 1320.6 ms, Saturday 17 October 2026, on the dock | 0.38 percent |
| cold prompt 5927.5 ms | 5830.4 ms, the discarded pass at 21:33 the same evening | 1.67 percent |
| generation 1560.6 against 1524.8 ms | each other, within the pair | 2.35 percent |

The first says the supply and the day do not move the timing, only whether a run survives,
which retires the confound that blocked this measurement all weekend. The third is the
internal check: generation does not depend on whether the prompt was decoded or restored, so
the two passes differ in the way the experiment intends and not in some other way.

## Why it is not accepted

The acceptance asks for `get_throttled` reading `0x0` before and after and discards a run with
any bit set. `0x80000` is bit 19, the soft temperature limit since boot. Bits 16, 17 and 18
stayed clear across the whole thirty minutes: no under-voltage event, no frequency capping, no
throttling, on a supply that had been suspected of all three.

**The cost of bit 19 was measured live, which is new.** Bit 3 is the same limit's now half,
and it toggled through the cold pass. `measure_clock arm` moved with it:

| `get_throttled` | clock | below full |
|---|---|---|
| `0x80000`, bit 3 clear | 1800457088 and 1800404352 Hz | full |
| `0x80008`, bit 3 set | 1775460992 Hz | 1.4 percent |
| `0x80008`, bit 3 set | 1726417920 Hz | 4.1 percent |

Two samples of the reduction, and nothing sampled how long the limit was engaged, so the
time-averaged cost on these figures is **not known** and is not stated. What can be said is
that an intermittent reduction whose observed extremes are 1.4 and 4.1 percent cannot carry
4.47 down to 2.0.

## The decision this leaves, which is not mine to make

Either:

1. **Cool the board** until a pair returns with the register clean at both ends, and accept
   the figure then. The thermal numbers below say what cooling has to achieve.
2. **Change the acceptance** to separate bit 19 from bits 16, 17 and 18: require those three
   clear, and permit the soft temperature limit on a run that carries its own clock trace.

The second is defensible on the measurements in the previous section and would be
indefensible without them, which is the difference between changing a protocol and weakening
one. Until one is chosen the clause reads measured at 4.47 and not accepted, which is a
different statement from unmeasured.

## The thermal trace, for whoever designs the cooling

Spot readings only, with the sampler that should have existed noted below.

| Time | Temperature | Note |
|---|---|---|
| 21:45 approximately | 53.0 degrees C | idle, still falling, seven minutes from boot |
| 21:47:21 | 53.5 degrees C | restore pass starts |
| 21:52:23 | 74.5 degrees C | restore pass ends, register still `0x0` |
| 21:55:46 | 56.9 degrees C | cold pass starts after a three-minute wait |
| 21:57:35 | 75.4 degrees C | one minute forty-nine seconds in |
| 21:58:29 | 76.9 degrees C | |
| 22:04:26 | 81.8 degrees C | |
| 22:05:06 to 22:05:47 | 81.3 degrees C | three readings, no change |
| 22:06:18 | 82.7 degrees C | |
| 22:07:19 | 81.3 degrees C | |
| 22:08:21 | 80.3 degrees C | cold pass ends |
| 22:08:57 | 68.1 degrees C | load gone |
| 22:09 approximately | 67.2 degrees C | clock back to 1800457088 Hz |

Two things for a cooling design. The board reached 75 degrees C inside two minutes of the cold
pass and then sat between 80 and 83 degrees C for ten minutes, so the cooling has to carry a
sustained load and not a burst. And it fell 13 degrees in about forty seconds once the load
stopped, so the heat path is adequate at idle and the shortfall is under load.

**What was missing, and now exists.** `tools/throttle_log.sh` writes the time,
`get_throttled`, `measure_clock arm` and `measure_temp` to a CSV once a second. The readings
above are spot checks taken by hand and they cannot give a duty cycle. Four readings in this
file were first reported as settled values and then withdrawn when the next one contradicted
them: a plateau at 77 degrees C, a plateau at 81.3 degrees C, a clock cost of 1.4 percent, and
a run believed finished that was not.

**And the sampler's own first run quantified why.** Five seconds on an idle board, Sunday 18
October 2026 at 22:37, gave temperatures of 50.6, 52.1, 51.1, 51.1 and 50.6 degrees C: a spread
of 1.5 degrees with no load at all. Every one of the four withdrawn conclusions rested on a
difference of that size or on a window shorter than half a minute. The noise floor was always
large enough to produce them, and nothing in this project had measured it until the instrument
built to see past it measured itself.

## One condition no chapter records, now read

This board's full ARM clock is 1.800 GHz, and `arm_boost=1` in `/boot/firmware/config.txt` is
the line that asks for it. That option is the documented switch for the Raspberry Pi 4's boost
clock. What this board runs at without the line has not been measured and is not asserted
here.

The volume's measurement protocol pins the model file, the thread count and the engine tag and
says nothing about frequency, so a reader reproducing these figures without that line would not
be at 1.800 GHz and would have nothing in the chapter to explain the gap. Three values make the
condition reproducible, all read on Sunday 18 October 2026 and all in the header table above:
the revision code, the firmware version, and `arm_boost=1`.

**How this was nearly missed, which is the part worth keeping.** The first search of
`config.txt` looked for `arm_freq`, `over_voltage`, `force_turbo`, `temp_soft_limit` and
`temp_limit`. It returned nothing, and that empty result was reported as "no override, so
1.800 GHz is the firmware default". The conclusion was wrong because the search did not include
the key that was there. A grep that returns nothing rules out only what it looked for, and a
confident negative from an incomplete needle list is the same failure as a counter that reads
zero because it counted the wrong noun. The fix was to stop guessing keys and print every line
the firmware acts on:

    grep -vE "^\s*(#|$)" /boot/firmware/config.txt

**Checked and clean, so nobody need check it again.** There are two files named `config.txt` on
this card. `/boot/firmware/config.txt` is the live one, on the boot partition.
`/boot/config.txt` is 91 bytes on the root filesystem and holds a Raspberry Pi OS placeholder
saying not to edit it and that the file has moved, so it is harmless by design rather than a
setting being silently ignored.

**Also in the live file and worth knowing for other projects:** `dtparam=spi=on`,
`dtparam=i2c_arm=on`, `enable_uart=1` and `dtoverlay=w1-gpio`, which claims a GPIO of its own.
There are no CAN overlay lines, so the note recording that this card gave both SPI0 chip
selects describes a state it is no longer in.
