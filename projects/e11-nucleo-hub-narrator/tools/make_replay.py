#!/usr/bin/env python3
"""Generate the e11 replay file and its oracle. Python is allowed here as a generator.

    python projects/e11-nucleo-hub-narrator/tools/make_replay.py

Writes projects/e11-nucleo-hub-narrator/replay/hub_60s.csv with the header
t_s,tag,v0,v1,v2,v3,v4: sixty seconds of the sensor hub's SAMPLE frames at the hub's
stated 104 Hz, one frame per low-g data-ready of the ISM6HG256X, in the shape the frame
layer (src/cbor_frames.c) hands to the loop: v0 the sequence counter, v1 to v3 the low-g
axes in g with gravity on z, v4 the high-g channel's peak-hold in g, the largest
magnitude since the previous frame. Quiet stretches carry sensor noise around 1 g. Sixty
low-g events (2.4 to 12 g, the same shock on both channels) and forty high-g impacts (18
to 90 g on the high-g channel, the low-g channel saturated at its 16 g full scale, as the
kit lab P08 describes) are spread over the minute, each a short decaying burst. Two
deliberate sequence gaps remove frames 1000 to 1002 and frame 2200: the counter and the
timestamps both jump, exactly as a lost frame looks on the wire, so the loop's gap check
has something to find in CI.

Also writes tests/oracle/e11.expected.csv with the header t_s,tag,rule,model: rule is
the decision of rules_e11.c applied in Python with the same two thresholds, in the same
order, on the same values; model is the CI stub's answer for the rows where e11_main.c
asks the model (a rising edge of the rule's index, or the quiet tick every ten seconds
on its own clock) and the empty string for every other row, where the loop logs the rule
alone. The
stub's answer is the grammar's first expansion, {"decision":"QUIET","line":"."},
CSV-quoted the way common/c/src/elog.c writes it.

The walk is seeded, so running the script twice writes the same bytes. The printed
summary gives the counts the chapter quotes: the rule's three totals, the number of
rows the model is asked on, the agreement the stub reaches in CI (the QUIET fraction
of the asked rows), the gaps, the frame rate the timestamps imply, and the histogram
of event peaks the data figure draws.
"""
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "hub_60s.csv"
ORACLE = REPO / "tests" / "oracle" / "e11.expected.csv"

RATE_HZ = 104
SECONDS = 60
N_FRAMES = RATE_HZ * SECONDS
TAG = "replay"
SEED = 2026
N_LOWG = 60
N_HIGHG = 40
LOWG_FULL_SCALE = 16.0
GAPS = {1000: 3, 2200: 1}           # first frame dropped: how many frames are lost
QUIET_ASK_S = 10.0                  # E11_QUIET_ASK_S in src/e11_main.c
STUB_TEXT = '"{""decision"":""QUIET"",""line"":"".""}"'
EMPTY = '""'                        # the model column when the rule ran alone
NAMES = ("QUIET", "LOW_G_EVENT", "HIGH_G_EVENT")


def rule(v):
    """The same two thresholds as rules_e11.c, in the same order, on the same values."""
    if len(v) < 5:
        return 0
    if v[4] > 16.0:
        return 2
    m2 = v[1] * v[1] + v[2] * v[2] + v[3] * v[3]
    if m2 > 4.0:
        return 1
    return 0


def q4(x):
    """The value as the replay file carries it and the C side parses it."""
    return float(f"{x:.4f}")


def frames(rng):
    """Every frame of the minute before the gaps are cut: (t_s, seq, ax, ay, az, peak)."""
    ax = [0.0] * N_FRAMES
    ay = [0.0] * N_FRAMES
    az = [0.0] * N_FRAMES
    pk = [0.0] * N_FRAMES
    for i in range(N_FRAMES):
        ax[i] = rng.gauss(0.0, 0.004)
        ay[i] = rng.gauss(0.0, 0.004)
        az[i] = 1.0 + rng.gauss(0.0, 0.006)
        pk[i] = 1.0 + abs(rng.gauss(0.0, 0.02))
    kinds = ["low"] * N_LOWG + ["high"] * N_HIGHG
    rng.shuffle(kinds)
    events = []
    for k, kind in enumerate(kinds):
        start_s = 1.0 + k * 0.57 + rng.uniform(0.0, 0.2)
        amp = rng.uniform(2.4, 12.0) if kind == "low" else rng.uniform(18.0, 90.0)
        length = rng.randint(3, 6)
        theta = rng.uniform(0.0, math.pi)
        phi = rng.uniform(0.0, 2.0 * math.pi)
        ux = math.sin(theta) * math.cos(phi)
        uy = math.sin(theta) * math.sin(phi)
        uz = math.cos(theta)
        first = int(round(start_s * RATE_HZ))
        for j in range(length):
            i = first + j
            if i >= N_FRAMES:
                break
            a = amp * (0.45 ** j)
            x, y, z = ux * a, uy * a, 1.0 + uz * a
            ax[i] = max(-LOWG_FULL_SCALE, min(LOWG_FULL_SCALE, x))
            ay[i] = max(-LOWG_FULL_SCALE, min(LOWG_FULL_SCALE, y))
            az[i] = max(-LOWG_FULL_SCALE, min(LOWG_FULL_SCALE, z))
            pk[i] = max(pk[i], a * (1.0 + rng.gauss(0.0, 0.03)))
        events.append((kind, amp))
    rows = []
    for i in range(N_FRAMES):
        t_s = float(f"{i / RATE_HZ:.6f}")
        rows.append((t_s, i, q4(ax[i]), q4(ay[i]), q4(az[i]), q4(pk[i])))
    return rows, events


def cut_gaps(rows):
    drop = set()
    for first, lost in GAPS.items():
        drop.update(range(first, first + lost))
    return [r for r in rows if r[1] not in drop]


def main():
    rng = random.Random(SEED)
    rows, events = frames(rng)
    rows = cut_gaps(rows)
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)
    counts = {n: 0 for n in NAMES}
    asked = 0
    asked_quiet = 0
    prev_idx = 0
    last_tick = None
    prev_seq = None
    gaps = 0
    missing = 0
    with REPLAY.open("w", newline="\n", encoding="ascii") as f_rep, \
            ORACLE.open("w", newline="\n", encoding="ascii") as f_orc:
        f_rep.write("t_s,tag,v0,v1,v2,v3,v4\n")
        f_orc.write("t_s,tag,rule,model\n")
        for t_s, seq, x, y, z, peak in rows:
            v = (float(seq), x, y, z, peak)
            idx = rule(v)
            counts[NAMES[idx]] += 1
            if prev_seq is not None and seq != prev_seq + 1:
                gaps += 1
                missing += seq - prev_seq - 1
            prev_seq = seq
            if last_tick is None or t_s - last_tick >= QUIET_ASK_S:
                last_tick = t_s                 # the quiet tick, on its own clock
                ask = True
            else:
                ask = idx > prev_idx            # a rising edge: something began
            prev_idx = idx
            if ask:
                asked += 1
                if idx == 0:
                    asked_quiet += 1
            f_rep.write(f"{t_s:.6f},{TAG},{seq},{x:.4f},{y:.4f},{z:.4f},{peak:.4f}\n")
            f_orc.write(f"{t_s:.6f},{TAG},{NAMES[idx]},{STUB_TEXT if ask else EMPTY}\n")
    n = len(rows)
    span = rows[-1][0] - rows[0][0]
    print(f"wrote {REPLAY.relative_to(REPO).as_posix()} and {ORACLE.relative_to(REPO).as_posix()}")
    print(f"frames {n} of {N_FRAMES} generated; sequence gaps {gaps}, frames missing {missing}")
    for k, c in counts.items():
        print(f"{k:13s} {c:5d} frames  {100.0 * c / n:5.1f} percent")
    print(f"model asked on {asked} rows; stub agreement (the QUIET fraction of those): "
          f"{asked_quiet} of {asked}, {100.0 * asked_quiet / asked:.1f} percent")
    print(f"frame rate from the timestamps: {(n - 1) / span:.2f} Hz over {span:.2f} s "
          f"against the hub's stated {RATE_HZ} Hz")
    bins = [0] * 12
    for kind, amp in events:
        bins[min(11, int(amp // 8))] += 1
    print("event peaks per 8 g bin, 0 to 96 g:", " ".join(str(b) for b in bins))
    print(f"events: {sum(1 for k, _ in events if k == 'low')} low-g, "
          f"{sum(1 for k, _ in events if k == 'high')} high-g")


if __name__ == "__main__":
    main()
