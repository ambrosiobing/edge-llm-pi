#!/usr/bin/env python3
"""Run the reference, run the harness, price one against the other, file the evidence.

    python3 tools/bench_runner.py --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \\
        --board pi4b-eplepi --evidence projects/e01-bringup/evidence \\
        --llama-bench build/bin/llama-bench \\
        --harness build/projects/e01-bringup/e01_bench_harness \\
        --prompt projects/e01-bringup/prompt.txt --hat "bare header, no HAT"

Protocol (the volume's measurement rule): `vcgencmd get_throttled` must read
throttled=0x0 before and after or the result is refused; the reference command is
`llama-bench -m <gguf> -p 64 -n 32 -t 1,2,3,4 -r 5 -o md`. The output goes into a dated
Markdown file in the evidence folder with the board, kernel, model, command, HAT state and
both throttle readings in its header, so the number can be attributed later. Use
--no-throttle-check on a host that has no vcgencmd (it is then written as not checked).

**--harness is what makes this a gate rather than a reading.** Given the harness binary and
a prompt file, the runner also runs the harness at each of --harness-threads, parses both
markdown tables, writes the division between them, and exits 1 if the harness disagrees with
the reference by more than --band at --gate-threads. Without it the runner files the
reference table alone and the comparison is a person's job.

Why it was added, on Friday 16 October 2026, straight after e01's first real run: the
protocol said the harness table was to be pasted into the evidence file by hand and the
division written beside it. That is four transcriptions of eight figures per board, each of
which can go wrong silently, and the arithmetic that decides pass or fail was being done in
someone's head. A gate nobody can mistype is worth more than a gate nobody can forget.

**--gate-threads defaults to 3, not to the core count, and that is deliberate.** On the
bench Pi 4B the reference's own four-thread generation figure varied by 12.5 percent of its
mean across five repeats, so a band of 10 percent drawn around it is crossed or not crossed
by chance; at three threads both instruments repeat to a hundredth of a token per second.
A gate is only as good as the stability of the number it is drawn around. Other thread
counts are measured and reported, and do not decide.

**--settle defaults to 60 seconds between stages, and that is a measured default.** On
Saturday 17 October 2026 this runner was refused three times on the bench Pi 4B with
`throttled=0x50000`, under-voltage and throttling flagged since boot, while the same three
stages run by hand with pauses between them passed. The HAT, the peripherals and the peak
thread count were each eliminated: bare header with nothing on USB still refused, and one-
and four-thread bursts from a fresh boot left the register clean. What differed was whether
the load was continuous. The hand runs peaked at 72.0 C and then fell to 71.5 and 68.1
between stages; back to back the board kept climbing into the band below the 80 C soft limit
where the draw is greatest and nothing yet tells the processor to stop. Pass --settle 0 to
reproduce the old behaviour, which is also how to test whether the pause is what helps.

**The register is read at every stage and the evidence file is written even when the run is
refused.** Both followed from the same night. Reading it only at the ends says the rail
sagged without saying when, and raising on a bad reading threw away eight minutes of
measurement that would have shown how far off it had gone. A refused run now stops at the
stage that dirtied the register, writes what it had, and says so at the top of the file.
"""
import argparse
import datetime
import os
import platform
import subprocess
import sys
import time

TESTS = ("pp64", "tg32")


CLEAN = "throttled=0x0"


def throttled(check):
    """The register, reported and not judged.

    It used to raise on anything but a clean reading, which meant a run refused at the end
    threw away the table it had just spent eight minutes measuring. A refused run is still a
    measurement of something, and on Saturday 17 October 2026 three of them in a row told us
    only that the rail had sagged at some unknown point. Reading and deciding are now
    separate, so the decision can be recorded next to the figures it rejected.
    """
    if not check:
        return "not checked"
    try:
        return subprocess.run(["vcgencmd", "get_throttled"], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        raise SystemExit("bench_runner: vcgencmd get_throttled failed; use "
                         "--no-throttle-check on a host that is not a Raspberry Pi")


def temperature():
    """Best effort. A host without vcgencmd still runs; it just records less."""
    try:
        return subprocess.run(["vcgencmd", "measure_temp"], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return "not read"


def decode(reading):
    """The sticky bits in words, because 0x50000 tells a reader nothing on its own."""
    if not reading.startswith("throttled=0x"):
        return reading
    try:
        bits = int(reading.split("0x", 1)[1], 16)
    except ValueError:
        return reading
    if bits == 0:
        return "clean"
    names = [(0, "under-voltage now"), (1, "arm frequency capped now"),
             (2, "throttled now"), (3, "soft temperature limit now"),
             (16, "under-voltage has occurred"), (17, "arm frequency capping has occurred"),
             (18, "throttling has occurred"), (19, "soft temperature limit has occurred")]
    set_bits = [text for bit, text in names if bits & (1 << bit)]
    return ", ".join(set_bits) if set_bits else "unknown bits"


def run_tool(label, cmd):
    """Run one measuring tool and return its stdout.

    stdout only: llama.cpp writes its loading and graph chatter to stderr, and the harness
    prints its table with std::printf, so the two separate cleanly and the table arrives
    without several thousand lines around it.
    """
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except OSError as exc:
        # OSError, not FileNotFoundError. A path that exists but cannot be executed raises
        # PermissionError instead, and the commonest way to reach that is to pass
        # build/llama.cpp/tools/llama-bench, which is the build tree's mirror of the source
        # FOLDER of that name rather than the binary; the binary is build/bin/llama-bench.
        # With only FileNotFoundError caught, that mistake printed a twenty-line traceback
        # instead of one line saying which path failed. Friday 16 October 2026.
        raise SystemExit("bench_runner: cannot run %s (%s)" % (cmd[0], exc))
    except subprocess.CalledProcessError as exc:
        raise SystemExit("bench_runner: %s exited %d\n%s" % (label, exc.returncode,
                                                             exc.stderr[-2000:]))
    # llama-bench prints the spread as U+00B1. The file is ASCII, because the repository
    # refuses a non-ASCII byte in anything a person edits, and errors="replace" turned
    # every one of those into a question mark: the first run's evidence file reads
    # "6.87 ? 0.00", which loses the meaning of the column. Transliterate, do not substitute.
    return result.stdout.replace("\u00b1", "+/-")


def parse_rows(text):
    """Markdown rows to {(threads, test): (value, spread)}.

    Both tools emit a seven-column row; the reference writes the test as pp64 and the
    harness as "pp 64", so the name is squeezed before it is used as a key.
    """
    rows = {}
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("|") or set(line) <= set("|- "):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) != 7 or cells[4] == "threads":
            continue
        try:
            threads = int(cells[4])
            parts = cells[6].split("+/-")
            value = float(parts[0])
            spread = float(parts[1]) if len(parts) > 1 else 0.0
        except ValueError:
            continue
        rows[(threads, cells[5].replace(" ", ""))] = (value, spread)
    return rows


def divide(reference, harness):
    """Rows of (threads, test, reference, harness, percent) where both measured the same."""
    out = []
    for threads, test in sorted(harness):
        if (threads, test) not in reference or test not in TESTS:
            continue
        ref = reference[(threads, test)][0]
        har = harness[(threads, test)][0]
        if ref == 0:
            continue
        out.append((threads, test, ref, har, (har - ref) / ref * 100.0))
    return out


def gate(reference, harness, gate_threads, band, ran=True):
    """(failed, sentence) for the one comparison that decides the run.

    Generation at gate_threads, because that is the quantity every later chapter's latency
    depends on and the one the harness exists to measure. A missing figure on either side
    is a failure and not a pass: a gate that cannot be applied has not been satisfied, and
    the opposite reading is how a suite shrinks to nothing and still reports success.
    """
    if not ran:
        return False, "not run; the harness was not given"
    key = (gate_threads, "tg32")
    if key not in reference or key not in harness:
        return True, ("REFUSED: no tg 32 figure at %d threads from both instruments, so "
                      "the gate could not be applied" % gate_threads)
    delta = (harness[key][0] - reference[key][0]) / reference[key][0] * 100.0
    ok = abs(delta) <= band
    return not ok, ("%s: the harness tg 32 at %d threads is %+.2f percent against the "
                    "reference, band %.1f percent"
                    % ("PASS" if ok else "FAIL", gate_threads, delta, band))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--model", required=True, help="GGUF file")
    parser.add_argument("--board", required=True, help="board and card, e.g. pi4b-cardA")
    parser.add_argument("--evidence", required=True, help="the project's evidence/ folder")
    parser.add_argument("--llama-bench", default="llama-bench", help="the reference binary")
    parser.add_argument("--threads", default="1,2,3,4")
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--no-throttle-check", action="store_true")
    parser.add_argument("--harness", help="the project's harness binary; enables the gate")
    parser.add_argument("--prompt", help="prompt file the harness reads; needs --harness")
    parser.add_argument("--harness-threads", default="3,4",
                        help="thread counts to run the harness at")
    parser.add_argument("--gate-threads", type=int, default=3,
                        help="the thread count whose tg comparison decides pass or fail")
    parser.add_argument("--band", type=float, default=10.0,
                        help="percent the harness may differ from the reference")
    parser.add_argument("--settle", type=int, default=60,
                        help="seconds of idle between stages; 0 runs them back to back")
    parser.add_argument("--hat", default="not recorded",
                        help="what was seated on the header, e.g. 'bare header, no HAT'")
    args = parser.parse_args()

    if args.harness and not args.prompt:
        raise SystemExit("bench_runner: --harness needs --prompt")

    check = not args.no_throttle_check
    before = throttled(check)
    if check and before != CLEAN:
        raise SystemExit("bench_runner: refused before any load, %s (%s). The sticky bits "
                         "only clear at boot, so reboot and read it again before running."
                         % (before, decode(before)))
    started = datetime.datetime.now()
    readings = [("before any load", before, temperature())]
    stopped_at = None

    cmd = [args.llama_bench, "-m", args.model, "-p", "64", "-n", "32", "-t", args.threads,
           "-r", str(args.repeat), "-o", "md"]
    reference_table = run_tool("llama-bench", cmd)
    after = throttled(check)
    readings.append(("after the reference", after, temperature()))
    if check and after != CLEAN:
        stopped_at = "the reference"

    # The register is read between stages, not only at the end, because three refusals in
    # one night said the rail had sagged without saying when. And the stages are separated
    # by --settle seconds, because the one difference between the runs that passed and the
    # runs that were refused on this bench was whether the load was continuous: the same
    # three stages run by hand with pauses peaked at 72.0 C and then fell to 71.5 and 68.1,
    # while back to back they kept climbing into the band below the soft limit where the
    # draw is greatest and nothing yet tells the processor to stop. Saturday 17 October 2026.
    harness_runs = []
    if args.harness and stopped_at is None:
        for t in [x.strip() for x in args.harness_threads.split(",") if x.strip()]:
            if args.settle > 0:
                time.sleep(args.settle)
                readings.append(("after settling %d s" % args.settle, throttled(check),
                                 temperature()))
            hcmd = [args.harness, "--model", args.model, "--prompt", args.prompt,
                    "-p", "64", "-n", "32", "-t", t, "-r", str(args.repeat)]
            harness_runs.append((hcmd, run_tool("the harness", hcmd)))
            after = throttled(check)
            readings.append(("after the harness at %s threads" % t, after, temperature()))
            if check and after != CLEAN:
                stopped_at = "the harness at %s threads" % t
                break

    reference = parse_rows(reference_table)
    harness = {}
    for _, text in harness_runs:
        harness.update(parse_rows(text))
    table = divide(reference, harness)

    failed, verdict = gate(reference, harness, args.gate_threads, args.band,
                           ran=bool(args.harness))
    if stopped_at is not None:
        # The register beats the gate. Figures measured on a rail that sagged are not
        # publishable however well they agree with each other, and the run stops at the
        # stage that dirtied it rather than carrying on to collect more of them.
        failed = True
        verdict = ("REFUSED at %s: the register read %s (%s). The stages after it were not "
                   "run. Everything above was measured before the register went dirty and "
                   "is kept so the failure can be read, not so it can be quoted."
                   % (stopped_at, readings[-1][1], decode(readings[-1][1])))

    os.makedirs(args.evidence, exist_ok=True)
    stem = os.path.splitext(os.path.basename(args.model))[0]
    name = "%s_llama-bench_%s_%s.md" % (started.strftime("%Y-%m-%d_%H%M"), args.board, stem)
    path = os.path.join(args.evidence, name)
    uname = platform.uname()
    with open(path, "w", encoding="ascii", errors="replace", newline="\n") as fh:
        fh.write("# llama-bench on %s, %s\n\n" % (args.board,
                                                  started.strftime("%A %d %B %Y, %H:%M")))
        fh.write("- model: %s\n" % os.path.basename(args.model))
        fh.write("- kernel: %s %s %s\n" % (uname.system, uname.release, uname.machine))
        fh.write("- command: %s\n" % " ".join(cmd))
        fh.write("- header: %s\n" % args.hat)
        fh.write("- settling pause between stages: %d s\n" % args.settle)
        fh.write("- instrument: upstream llama-bench; the volume's harness is priced "
                 "against this table and published only within %.1f percent of it\n\n"
                 % args.band)
        if stopped_at is not None:
            fh.write("**REFUSED at %s.** See the verdict below. The figures in this file "
                     "are kept so the failure can be read; none of them is publishable.\n\n"
                     % stopped_at)
        fh.write("## The register, read at every stage\n\n")
        fh.write("| moment | vcgencmd get_throttled | in words | temperature |\n")
        fh.write("|---|---|---|---|\n")
        for moment, reading, temp in readings:
            fh.write("| %s | %s | %s | %s |\n" % (moment, reading, decode(reading), temp))
        fh.write("\nRead between stages and not only at the ends, because a run refused at "
                 "the end says the rail sagged without saying when.\n\n")
        fh.write(reference_table)
        if not reference_table.endswith("\n"):
            fh.write("\n")
        for hcmd, text in harness_runs:
            fh.write("\n## The harness, same board, same sitting, same model file\n\n")
            fh.write("    %s\n\n" % " ".join(hcmd))
            fh.write(text)
            if not text.endswith("\n"):
                fh.write("\n")
        if table:
            fh.write("\n## The division\n\n")
            fh.write("| threads | quantity | reference | harness | harness minus reference |\n")
            fh.write("|---|---|---|---|---|\n")
            for threads, test, ref, har, pct in table:
                label = "pp 64" if test == "pp64" else "tg 32"
                fh.write("| %d | %s | %.2f | %.2f | %+.2f percent |\n"
                         % (threads, label, ref, har, pct))
            fh.write("\n**Verdict: %s.**\n\n" % verdict)
            fh.write("The gate is the tg 32 comparison at %d threads. Other thread counts "
                     "are measured and reported here and do not decide, because a band is "
                     "only meaningful around a figure that repeats.\n" % args.gate_threads)

    print(reference_table)
    for _, text in harness_runs:
        print(text)
    print("bench_runner: written %s" % path)
    print("bench_runner: %s" % verdict)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
