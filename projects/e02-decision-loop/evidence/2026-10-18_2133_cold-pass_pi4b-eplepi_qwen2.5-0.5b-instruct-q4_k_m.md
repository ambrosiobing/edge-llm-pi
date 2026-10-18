# e02 cold pass, Sunday 18 October 2026, and why it is discarded

Written by hand, like the file of Saturday 17 October 2026 beside it, because e02 has no
runner as e01 does. Every figure carries the command that produced it.

| | |
|---|---|
| Date | Sunday 18 October 2026, finished 21:33:35 local |
| Board | Raspberry Pi 4 Model B, 8 GB, card `eplepi`, user `bing` |
| Supply | ISY IAC-5065, 65 W USB-C charger, 5 V rail labelled 5.0 V at 3.0 A, 15.0 W |
| Model | `qwen2.5-0.5b-instruct-q4_k_m.gguf`, Q4_K_M, 491 MB |
| Threads | 2 |
| Rows | 100, the first hundred of `replay/synthetic_1000.csv` |
| Verdict | **discarded**, by this chapter's own acceptance protocol |

**This is the first supply this chapter has ever named.** The four restarts of Friday 16 and
Saturday 17 October 2026 were on a different one, an ISY IAD-1025-1 USB-C dock. The dock's
exact topology on those two days, meaning which plug entered the Pi's own USB-C socket, is
not confirmed and is not guessed here.

## The commands

    cd ~/edge-llm-pi/projects/e02-decision-loop
    head -n 101 replay/synthetic_1000.csv > /tmp/e02_first_100.csv
    vcgencmd get_throttled
    ../../build/projects/e02-decision-loop/e02_variant_a \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --replay /tmp/e02_first_100.csv --threads 2 --cold \
      --log evidence/e02_variant_a_cold.csv
    vcgencmd get_throttled; uptime

`head -n 101` and not `-n 100`, because the file carries a header row; it printed 101 lines.
`/tmp` is cleared at boot on Raspberry Pi OS, and the board had been rebooted, so the first
attempt of the evening failed at once with `e02_variant_a: cannot open the replay or the log`
and the `head` was repeated. That failure cost seconds and is not one of the four restarts.

## What it printed

    variant A (C++ on llama.h): rows 100, mode cold
    mean prompt_ms 5830.4, mean generate_ms 1545.2, rows without a literal 0
    VmRSS kB 581208
    throttled=0x80000
     21:33:35 up 20 min,  2 users,  load average: 2.10, 1.94, 1.21

`get_throttled` read `0x0` before the run, taken about five minutes from boot, and `0x80000`
after it.

`rows without a literal 0` is this binary's own count and is not the clause-one gate. That
gate is the agreement tool over the thousand-row run, which is where the 949 malformed rows
of Saturday 17 October 2026 were counted.

## Why it is discarded

`0x80000` is bit 19 alone: the soft temperature limit has occurred since boot. It is **not**
bit 16, under-voltage since boot, and **not** bit 18, throttling since boot. The sticky bits
clear only at boot, and the board had booted about five minutes before the run with the
register at `0x0`, so the bit was set during this run and by this load.

This chapter's acceptance asks for `get_throttled` reading `0x0` before and after both passes
and discards a run with any bit set. The rule exists so that a figure taken under a reduced
clock is not published, and it applies to a run that would have passed exactly as it applies
to one that would not. So the figure above is a record, not evidence, and e02's second clause
stays unmeasured.

## What it would have given, and how far that can be trusted

Against the restore half of 1320.6 ms from Saturday 17 October 2026, 5830.4 ms is a ratio of
**4.4**, where the acceptance asks for at least 2.0. Three reasons that is a cross-check and
not the number:

- The two halves are from two days and two supplies, the restore on the dock and the cold on
  the charger. The acceptance asks for the same conditions on the same board.
- The register was dirty at the end of the cold half, so its clock was not constant.
- The restore half itself carries a known flaw recorded in the file of Saturday 17 October
  2026: a wasted decode of the final token on every row, left in place deliberately so that
  both passes run identical code.

How much the thermal event can have moved the cold figure is bounded by the one column that
state restore does not touch. Generation does not depend on whether the prompt was decoded or
restored, and it came out at 1545.2 ms here against 1506.3 ms on Saturday, a difference of
2.6 percent. The distortion is percents; the margin over the acceptance is a factor of two.
The conclusion is not in doubt. The evidence for it is, which is the whole reason this file
says discarded at the top rather than 4.4.

## What the run does establish

- **The board held.** A hundred rows at two threads, about thirteen minutes of continuous
  load including the model load, with no restart: `uptime` read 20 min continuous at the end.
  The attempt at the same pass on the dock, on Saturday 17 October 2026, stopped after three
  minutes of about seven.
- **No under-voltage event under that load.** Bit 16 is clear after a load that had taken the
  board down the day before.
- One observation is not a mechanism. The dock is now the leading explanation for the four
  restarts of Friday 16 and Saturday 17 October 2026. It is not the established cause, and
  this file does not say it is.
- **Resident memory on the cold path:** VmRSS 581208 kB, about 568 MiB, against a 491 MB model
  file at `n_ctx` 512.

## What the next sitting needs

1. Reboot. Bit 19 is sticky until the board boots, so no run started now can show `0x0` at
   its start, whatever else it does.
2. Read `vcgencmd measure_temp` at idle after the boot. If the board is already warm when
   idle on a bare case, the next thing to fix is cooling and not the run.
3. Both passes back to back on this one supply, restore first and cold second, with
   `get_throttled` before the first and after the second, and each pass to its own `--log`.
4. If bit 19 sets again on a cooled board, the pair cannot be taken on a bare board and the
   chapter should say that rather than retry a fourth time.
