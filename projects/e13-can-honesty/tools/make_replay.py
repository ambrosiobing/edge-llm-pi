#!/usr/bin/env python3
"""Generate the e13 replay file and its oracle. Python is allowed here as a generator.

    python projects/e13-can-honesty/tools/make_replay.py

Writes two files and prints the numbers the chapter's agreement table quotes.

projects/e13-can-honesty/replay/heartbeats_1000.csv, header t_s,tag,v0,v1,v2,v3,v4,v5:
one row per CAN frame, in time order, as the service sees them. tag is the interface the
frame arrived on, v0 the identifier with its flags, v1 the length, v2 and v3 the first two
data bytes, v4 and v5 the transmit and receive error counters an error frame carries in
data[6] and data[7]. A file is one fixed ordering, so the interface names here are the
canonical ones (can0 the MCP2518FD, can1 the MCP2515, vcan0 the virtual node); on the
bench the two hardware names swap between boots and the service binds them by driver.

Three nodes send a heartbeat every period and a fourth never sends anything. Dropouts are
injected on all three senders, each lasting four to nine periods, which is long enough for
the rule's three period timeout to be crossed and the recovery to be seen. A dropout on
the first hardware node also produces the error frames the other controller reports while
it transmits into a broken pair: a transmit warning, then transmit passive, then bus-off
with its heartbeat for that one period missing, then the restart. The restart lands inside
the next period on purpose, so the second node never reaches the three period timeout and
the chapter can say that the recovery interval was chosen rather than inherited.

tests/oracle/e13.expected.csv, header t_s,tag,rule,model: four rows per period, one per
node, carrying the rule's verdict and, for the one node the service asks about that
period, the stub's answer. The rule here is the same three lines as src/rules.rs in the
same order, and the stub answer is derived from grammar.gbnf by the same algorithm as
edgellm::engine::first_expansion in the shared Rust crate and edgellm_stub.c in the C
core, so no literal in this file is hand written. The one simplification: a parenthesised
group is walked through rather than expanded as its own first alternative, which this
project's grammar never needs because it has no groups, and which would show up at once
as a row that differs from the service's own answer.

The oracle file and the replay file are one pair: regenerate both together, and run the
project's oracle test afterwards.

The walk is seeded, so two runs write the same bytes.
"""
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
REPO = PROJECT.parent.parent
REPLAY = PROJECT / "replay" / "heartbeats_1000.csv"
GRAMMAR = PROJECT / "grammar.gbnf"
ORACLE = REPO / "tests" / "oracle" / "e13.expected.csv"

PERIODS = 1000
PERIOD_S = 1.0
ABSENT_AFTER = 3.0
STATE_OPERATIONAL = 0x05

# The kernel's error frame encoding, from the can-utils copy of include/linux/can.h and
# include/linux/can/error.h, read Wednesday 14 October 2026.
CAN_ERR_FLAG = 0x20000000
CAN_ERR_CRTL = 0x00000004
CAN_ERR_BUSOFF = 0x00000040
CAN_ERR_RESTARTED = 0x00000100
CAN_ERR_CRTL_TX_WARNING = 0x08
CAN_ERR_CRTL_TX_PASSIVE = 0x20

# name, heartbeat id, interface, source class, offset inside the period, dropout chance
NODES = [
    ("joint-a", 0x701, "can0", "hardware", 0.02, 0.012),
    ("joint-b", 0x702, "can1", "hardware", 0.04, 0.010),
    ("joint-c", 0x703, "vcan0", "vcan", 0.06, 0.006),
    ("joint-d", 0x704, "", "absent", 0.00, 0.000),
]


# ---------------------------------------------------------------- the grammar's stub answer

def grammar_rules(text):
    """name -> body, where a body runs to the next line that defines a rule."""
    rules = {}
    name, body = None, ""
    for line in text.splitlines():
        bare = line.split("#", 1)[0]
        head = bare.split("::=", 1)
        starts = (len(head) == 2 and head[0].strip()
                  and all(c.isalnum() or c in "_-" for c in head[0].strip()))
        if starts:
            if name:
                rules[name] = body
            name, body = head[0].strip(), head[1]
        elif name:
            body += " " + bare
    if name:
        rules[name] = body
    return rules


def expand(rules, body, depth=0):
    """The first alternative of this body, as edgellm_stub.c documents it: literals append
    their text, a rule reference expands that rule's first alternative, a character class
    and every repetition mark append nothing, and '|' ends the walk."""
    out, i, n = [], 0, len(body)
    while i < n:
        c = body[i]
        if c == "|":
            break
        if c.isspace():
            i += 1
        elif c == '"':
            i += 1
            while i < n and body[i] != '"':
                if body[i] == "\\" and i + 1 < n:
                    i += 1
                    out.append({"n": "\n", "t": "\t", "r": "\r"}.get(body[i], body[i]))
                else:
                    out.append(body[i])
                i += 1
            i += 1
        elif c == "[":
            while i < n and body[i] != "]":
                i += 2 if body[i] == "\\" else 1
            i += 1
        elif c == "{":
            while i < n and body[i] != "}":
                i += 1
            i += 1
        elif c in "*+?()":
            i += 1
        elif c.isalnum() or c in "_-":
            start = i
            while i < n and (body[i].isalnum() or body[i] in "_-"):
                i += 1
            ident = body[start:i]
            if depth < 8 and ident in rules:
                out.append(expand(rules, rules[ident], depth + 1))
        else:
            i += 1
    return "".join(out)


# ---------------------------------------------------------------- the rule, in Python

def presence(source, silent_s):
    """The three lines of src/rules.rs, in the same order."""
    if source == "vcan":
        return "MODELLED"
    if silent_s >= ABSENT_AFTER * PERIOD_S:
        return "ABSENT"
    return "PRESENT"


def csv_cell(text):
    if any(ch in text for ch in ',"\n'):
        return '"' + text.replace('"', '""') + '"'
    return text


# ---------------------------------------------------------------- the timeline

def build_timeline(seed=2026):
    """down[i] is the set of periods in which node i sends nothing; errors[p] is the list
    of error frames to emit in period p on the second hardware node's interface."""
    rng = random.Random(seed)
    down = [set() for _ in NODES]
    errors = {}
    out_until = [0] * len(NODES)
    dropouts = [0] * len(NODES)
    for p in range(PERIODS):
        for i, (_, _, _, source, _, chance) in enumerate(NODES):
            if source == "absent":
                continue
            if p >= out_until[i] and rng.random() < chance:
                length = rng.randint(4, 9)
                out_until[i] = p + length
                dropouts[i] += 1
                if i == 0:
                    for k, (cls, data1, tx, rx) in enumerate([
                            (CAN_ERR_CRTL, CAN_ERR_CRTL_TX_WARNING, 96, 0),
                            (CAN_ERR_CRTL, CAN_ERR_CRTL_TX_PASSIVE, 128, 0),
                            (CAN_ERR_BUSOFF, 0x00, 255, 0),
                            (CAN_ERR_RESTARTED, 0x00, 0, 0)]):
                        if p + k < PERIODS:
                            errors.setdefault(p + k, []).append((cls, data1, tx, rx))
                    if p + 2 < PERIODS:
                        down[1].add(p + 2)       # bus-off: it stops transmitting
            if p < out_until[i]:
                down[i].add(p)
    return down, errors, dropouts


def main():
    stub_text = expand(grammar_rules(GRAMMAR.read_text(encoding="ascii")),
                       grammar_rules(GRAMMAR.read_text(encoding="ascii"))["root"])
    down, errors, dropouts = build_timeline()
    REPLAY.parent.mkdir(parents=True, exist_ok=True)
    ORACLE.parent.mkdir(parents=True, exist_ok=True)

    frames = []
    for p in range(PERIODS):
        t0 = p * PERIOD_S
        for i, (_, can_id, iface, source, offset, _) in enumerate(NODES):
            if source == "absent" or p in down[i]:
                continue
            frames.append((t0 + offset, iface, can_id, 1, STATE_OPERATIONAL, 0, 0, 0))
        for cls, data1, tx, rx in errors.get(p, []):
            frames.append((t0 + 0.50, NODES[1][2], CAN_ERR_FLAG | cls, 8, 0, data1, tx, rx))
    frames.sort(key=lambda row: row[0])

    with REPLAY.open("w", newline="\n", encoding="ascii") as fh:
        fh.write("t_s,tag,v0,v1,v2,v3,v4,v5\n")
        for t_s, iface, can_id, length, d0, d1, tx, rx in frames:
            fh.write("%.2f,%s,%d,%d,%d,%d,%d,%d\n" % (t_s, iface, can_id, length, d0, d1,
                                                      tx, rx))

    last_heard = [None] * len(NODES)
    last_verdict = [None] * len(NODES)
    counts = {"PRESENT": 0, "MODELLED": 0, "ABSENT": 0}
    asked_counts = {"PRESENT": 0, "MODELLED": 0, "ABSENT": 0}
    asked_node = [0] * len(NODES)
    pos = 0
    with ORACLE.open("w", newline="\n", encoding="ascii") as fh:
        fh.write("t_s,tag,rule,model\n")
        for p in range(PERIODS):
            end = (p + 1) * PERIOD_S
            while pos < len(frames) and frames[pos][0] < end:
                t_s, iface, can_id, _, _, _, _, _ = frames[pos]
                if not can_id & CAN_ERR_FLAG:
                    for i, (_, node_id, node_iface, _, _, _) in enumerate(NODES):
                        if node_id == can_id and node_iface == iface:
                            last_heard[i] = t_s
                pos += 1
            verdicts = []
            for i, (_, _, _, source, _, _) in enumerate(NODES):
                silent = float("inf") if last_heard[i] is None else end - last_heard[i]
                verdicts.append(presence(source, silent))
            pick = None
            for i in range(len(NODES)):
                if last_verdict[i] is not None and last_verdict[i] != verdicts[i]:
                    pick = i
                    break
            if pick is None:
                pick = p % len(NODES)
            asked_counts[verdicts[pick]] += 1
            asked_node[pick] += 1
            for i, (name, _, _, _, _, _) in enumerate(NODES):
                counts[verdicts[i]] += 1
                model = csv_cell(stub_text) if i == pick else ""
                fh.write("%.6f,%s,%s,%s\n" % (end, name, verdicts[i], model))
            last_verdict = list(verdicts)

    total_rows = PERIODS * len(NODES)
    print("wrote %s and %s" % (REPLAY.relative_to(REPO).as_posix(),
                               ORACLE.relative_to(REPO).as_posix()))
    print("frames: %d over %d periods" % (len(frames), PERIODS))
    print("dropouts injected: " + ", ".join(
        "%s %d" % (NODES[i][0], dropouts[i]) for i in range(len(NODES))))
    print("rule rows: %d" % total_rows)
    for key in ("PRESENT", "MODELLED", "ABSENT"):
        print("  %-9s %5d rows  %5.1f percent" % (key, counts[key],
                                                  100.0 * counts[key] / total_rows))
    print("model answers: %d, one per period" % PERIODS)
    for key in ("PRESENT", "MODELLED", "ABSENT"):
        print("  asked rows the rule calls %-9s %4d" % (key, asked_counts[key]))
    print("asked per node: " + ", ".join(
        "%s %d" % (NODES[i][0], asked_node[i]) for i in range(len(NODES))))
    print("stub answer from grammar.gbnf: %s" % stub_text)
    print("stub agreement (the PRESENT fraction of the asked rows): %d of %d, %.1f percent"
          % (asked_counts["PRESENT"], PERIODS, 100.0 * asked_counts["PRESENT"] / PERIODS))


if __name__ == "__main__":
    main()
