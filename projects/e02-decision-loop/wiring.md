# e02 wiring: nothing on the header

Project e02 is the decision loop on a replayed CSV. No part is wired, no HAT is seated, and
nothing is read from the 40-pin header. The board is the Raspberry Pi 4 Model B with the card
`eplepi` (Raspberry Pi OS, Debian 13 trixie, user `bing`), a USB keyboard and either the
Renkforce USB/TTL serial console at 115200 or SSH. This is the one project in the volume
whose wiring figure shows an empty header, on purpose: the loop must run end to end before
any sensor is attached, and from e03 on the same binary runs with `--live` and a device.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | none |

## The one-HAT rule for this Pi

The Pi 4B header is free during e02. If the MCC 118 (card A) or the WS-28164 (card B) is
seated from a previous sitting, it may stay seated: e02 touches neither SPI0 nor any GPIO
line. It does not matter which card is in the board for e02 as long as the model file is on
it; the chapter names `eplepi`.

## Pre-flight, on the Pi 4B over SSH or the serial console

Memory first. The model file must fit under 60 percent of `MemAvailable` or the run is
SD-card paging, not a decision latency.

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    vcgencmd measure_temp
    ls -l ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf

`get_throttled` must read `0x0` before and after the 1000-decision run.

## The run

    cd ~/edge-llm-pi
    build/projects/e02-decision-loop/edgellm-e02 --project e02 \
      --replay projects/e02-decision-loop/replay/synthetic_1000.csv \
      --grammar projects/e02-decision-loop/grammar.gbnf \
      --prompt projects/e02-decision-loop/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --state /tmp/e02.state \
      --log projects/e02-decision-loop/evidence/e02_run.csv
    vcgencmd get_throttled

## Acceptance

    build/tests/oracle_check projects/e02-decision-loop/evidence/e02_run.csv \
      tests/oracle/e02.expected.csv
    python tools/agreement.py projects/e02-decision-loop/evidence/e02_run.csv

The oracle check compares the rule column byte for byte and will report every row where
the real model's column differs from the stub's; what passes the chapter is the agreement
tool's count of rows whose model column is not one of NORMAL, WATCH, ALERT, which must be
zero. The second acceptance number is the ratio of the `--cold` mean `prompt_ms` to the
state-restore mean `prompt_ms` from `e02_variant_a` on the same board and thread count,
which must be at least 2.0; throttle bits `0x0` before and after both.

## config.txt

No overlay and no line is added for e02.
