# e04 wiring: nothing on the header, two processes on the board

Project e04 is the thermal and throttle narrator: a C client of llama-server over HTTP on
the loopback interface of the Raspberry Pi 4 Model B with the card `eplepi` (Raspberry Pi
OS, Debian 13 trixie, user `bing`). No part is wired, no HAT is seated, and nothing is read
from the 40-pin header. The two inputs are the kernel's thermal zone and the firmware's
throttle word; the two processes are `llama-server`, which holds the model, and
`edgellm-e04`, which holds the loop. What is absent is any current reading: the PPK2 cannot
meter a Raspberry Pi, so the self-heating of narrating is measured as temperature, never as
power.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | none |

## The one-HAT rule for this Pi

The header is free during e04. A HAT left seated from another sitting (the MCC 118 on
card A, the WS-28164 on card B) may stay: e04 touches no bus and no GPIO line. The
temperature trace is only comparable between runs with the same HAT state, so the evidence
file names it.

## Pre-flight, on the Pi 4B over SSH or the serial console

    cat /sys/class/thermal/thermal_zone0/temp
    vcgencmd get_throttled
    vcgencmd measure_clock arm
    vcgencmd measure_temp
    ls -l /dev/vcio
    id -nG | tr ' ' '\n' | grep -x video
    ls -l ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf
    free -m

`thermal_zone0/temp` prints millidegrees (`46260` is 46.26 C). `get_throttled` prints a hex
word: bit 0 under-voltage, bit 1 ARM frequency capped, bit 2 currently throttled, bit 3 soft
temperature limit active, bits 16 to 19 the same four as "has occurred" since boot. The rule
reads bits 2 and 3 and the temperature; the "has occurred" bits never decide. `vcgencmd`
opens `/dev/vcio`, owned by root and the video group on Raspberry Pi OS; the user `bing` must
be in that group, and the unit adds it with `SupplementaryGroups=video`. The model file must
fit under 60 percent of `MemAvailable`; at 0.27 GB it does on every Pi 4B variant.

## The server

In one terminal, or as the unit `files/edgellm-llama-server.service`:

    cd ~/llama.cpp
    build/bin/llama-server -m ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
      --host 127.0.0.1 --port 8080 -c 512 -t 4

`~/llama.cpp` is the clone project e01 makes at the pinned tag, and the chapter adds the
server target to that existing build rather than compiling the engine twice. It does not
exist on this bench yet: e01's run of Friday 16 October 2026 used the repository's submodule
build for its reference instead of the standalone clone its own steps prescribe, which is
recorded as a deviation in `projects/e01-bringup/evidence`. Either create the clone as e01
step 3 says, or point these commands at `~/edge-llm-pi` and its `build/bin/`, which carries
the same binaries from the same tag. The choice belongs with e01's, not with this chapter.

and from a second terminal, before the client, `curl -s http://127.0.0.1:8080/health` must
print `{"status":"ok"}`.

## The run

Replay first (the same loop, the file standing in for the thermal zone), then live:

    cd ~/edge-llm-pi
    build/projects/e04-thermal-narrator/edgellm-e04 --project e04 \
      --replay projects/e04-thermal-narrator/replay/benchmark_180.csv \
      --grammar projects/e04-thermal-narrator/grammar.gbnf \
      --prompt projects/e04-thermal-narrator/prompt.txt \
      --server http://127.0.0.1:8080 \
      --log projects/e04-thermal-narrator/evidence/e04_replay.csv \
      --rtt projects/e04-thermal-narrator/evidence/e04_replay_rtt.csv

    cd ~/edge-llm-pi
    build/projects/e04-thermal-narrator/edgellm-e04 --project e04 \
      --live sysfs /sys/class/thermal/thermal_zone0/temp:60 \
      --grammar projects/e04-thermal-narrator/grammar.gbnf \
      --prompt projects/e04-thermal-narrator/prompt.txt \
      --server http://127.0.0.1:8080 \
      --log projects/e04-thermal-narrator/evidence/e04_live.csv \
      --rtt projects/e04-thermal-narrator/evidence/e04_live_rtt.csv

The live run is stopped with Ctrl-C after the e01 benchmark it narrates has ended; every row
is flushed as it is written. The narrator-idle trace for the self-heating comparison is the
same binary with `--rule-only` and no `--server`, over the same benchmark on another sitting:

    cd ~/edge-llm-pi
    build/projects/e04-thermal-narrator/edgellm-e04 --project e04 \
      --live sysfs /sys/class/thermal/thermal_zone0/temp:60 \
      --grammar projects/e04-thermal-narrator/grammar.gbnf \
      --prompt projects/e04-thermal-narrator/prompt.txt --rule-only \
      --log projects/e04-thermal-narrator/evidence/e04_idle.csv

## Acceptance

    build/tests/oracle_check projects/e04-thermal-narrator/evidence/e04_replay.csv \
      tests/oracle/e04.expected.csv
    python tools/agreement.py projects/e04-thermal-narrator/evidence/e04_live.csv

The oracle check compares the rule column byte for byte and reports every row where the
real model's column differs from the mock's, which is expected on the bench. What passes the
chapter is three numbers: the agreement over the live log at or above 95 percent once the
rows where the rule's decision differs from the previous row's are set aside (the client
prints both counts on exit, `agree`, `disagree` and `of which ... on a row where the rule
changed`); the mean of the `rtt_ms` column of the `_rtt.csv` file minus the mean of the two
server columns, which is the HTTP cost and is placed beside e02's in-process figure on the
same board; and the difference between the `e04_live.csv` and `e04_idle.csv` temperature
columns over the same benchmark, which is the self-heating of narrating. `get_throttled`
is read before and after, and a run where the idle trace itself shows bit 2 or bit 3 is
discarded.

## config.txt

No overlay and no line is added for e04.
