#!/bin/sh
# Samples the Raspberry Pi firmware's throttle register, ARM clock and SoC temperature into a
# CSV for the duration of a measurement run, so a figure arrives with its own conditions
# attached instead of with spot checks taken by hand.
#
# Usage, from the project folder whose evidence/ should hold the trace:
#     sh ../../tools/throttle_log.sh evidence/eNN_throttle_2026-10-18.csv &
#     ... run the passes ...
#     kill %1
#
# Third argument is a sample cap, after which it stops by itself; 0 or absent means it runs
# until killed. A detached sampler beside a detached run needs one, because if the run dies
# early nothing kills the sampler and it writes to the card until the card is full. For the
# eighty-five minute agreement run the sheet passes 6000, a little over the 5112 seconds the
# run needs:
#     nohup sh tools/throttle_log.sh evidence/e02_throttle_1000.csv 1 6000 &
#
# Invoked through sh, not as ./throttle_log.sh, because this file is committed 100644 like
# tools/install-hooks.sh beside it. A tracked execute bit is a mode the index has to carry and
# that a Windows checkout cannot set, so the repository's convention is to name the interpreter
# instead. Calling it directly gives "Permission denied" and nothing else.
#
# Columns: unix_s, iso_time, throttled, arm_hz, temp_c
#
# WHY THIS EXISTS. get_throttled's sticky bits (16 to 19) say a limit occurred and never for
# how long, and its live bits (0 to 3) change faster than a person can type. e02's cold pass of
# Sunday 18 October 2026 toggled bit 3 throughout its twelve and a half minutes while
# measure_clock arm moved between 1726417920 and 1800457088 Hz, and four readings taken by hand
# during it were each reported as a settled value and then withdrawn when the next contradicted
# them: a plateau at 77 degrees C, a plateau at 81.3 degrees C, a clock cost of 1.4 percent, and
# a run believed finished that was still going. A column cannot be withdrawn, and a duty cycle
# cannot be had any other way.
#
# WHAT IT DOES NOT TELL YOU. Its own cost is unmeasured. Each sample spawns three vcgencmd
# calls, and on a board already at its thermal ceiling that is not obviously free. The
# instrument is the first suspect, so run it alone on an idle board for a minute and read the
# temperature column before trusting a trace taken alongside a run.
#
# FIRST RUN, Sunday 18 October 2026 at 22:37 on the Raspberry Pi 4 Model B, card eplepi: five
# samples in five seconds, every column populated, and the trap reported its count. The loop,
# the trap, the interval and the timestamp format are verified; the refusal to overwrite an
# existing file is not, and neither is the cost named above.
#
# That five-second trace also measured the noise this file exists to see past. On an idle board
# the temperature column read 50.6, 52.1, 51.1, 51.1 and 50.6 degrees C in five consecutive
# seconds, a spread of 1.5 degrees with nothing running, and the clock column alternated between
# 1800457088 and 1800404352 Hz, a jitter of 52736 Hz or three thousandths of a percent. A
# difference of one degree between two spot readings is therefore not a trend and never was,
# which is what four withdrawn conclusions cost to learn by hand the same evening.
#
# Smoke-test it on any new board, or after changing it, before relying on a trace:
#     sh ../../tools/throttle_log.sh /tmp/smoke.csv & sleep 5; kill %1; cat /tmp/smoke.csv
# Five header-plus-sample lines with no empty fields means it works there too.
set -u

out=${1:-}
interval=${2:-1}
max=${3:-0}

if [ -z "$out" ]; then
    echo "usage: $0 <output.csv> [interval_seconds] [max_samples]" >&2
    exit 2
fi
case "$max" in
    ''|*[!0-9]*)
        echo "$0: max_samples must be a whole number, got '$max'" >&2
        exit 2
        ;;
esac
if ! command -v vcgencmd >/dev/null 2>&1; then
    echo "$0: vcgencmd not found, so this is not a Raspberry Pi and there is nothing to read" >&2
    exit 2
fi
if [ -e "$out" ]; then
    echo "$0: $out exists; refusing to overwrite a trace that may be someone's evidence" >&2
    exit 2
fi

printf 'unix_s,iso_time,throttled,arm_hz,temp_c\n' > "$out" || exit 1

# On exit, say how many samples landed and where, because a trace nobody can find is no better
# than no trace. The subtraction removes the header row.
report() {
    lines=$(grep -c "" "$out" 2>/dev/null || echo 1)
    echo "$0: $((lines - 1)) samples in $out" >&2
    exit 0
}
trap report INT TERM

# A failed vcgencmd writes an empty field and the loop continues, because a gap in a trace is
# itself worth recording and is more useful than a trace that stops at the first hiccup.
count=0
while :; do
    thr=$(vcgencmd get_throttled 2>/dev/null | cut -d= -f2) || thr=
    hz=$(vcgencmd measure_clock arm 2>/dev/null | cut -d= -f2) || hz=
    deg=$(vcgencmd measure_temp 2>/dev/null | cut -d= -f2 | tr -d "'C") || deg=
    printf '%s,%s,%s,%s,%s\n' \
        "$(date +%s)" "$(date +%Y-%m-%dT%H:%M:%S%z)" "$thr" "$hz" "$deg" >> "$out"
    count=$((count + 1))
    if [ "$max" -gt 0 ] && [ "$count" -ge "$max" ]; then
        echo "$0: $count samples in $out, which is the cap" >&2
        exit 0
    fi
    sleep "$interval"
done
