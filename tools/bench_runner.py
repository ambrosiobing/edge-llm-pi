#!/usr/bin/env python3
"""Run llama-bench the volume's way and file the result as evidence.

    python tools/bench_runner.py --model models/SmolLM2-360M-Instruct-Q4_K_M.gguf \\
        --board pi3b-eplepi --evidence projects/e01-bringup/evidence

Protocol (the volume's measurement rule): `vcgencmd get_throttled` must read
throttled=0x0 before and after the run or the result is refused; the command is
`llama-bench -m <gguf> -p 64 -n 32 -t 1,2,3,4 -r 5 -o md`. The output goes into a dated
Markdown file in the evidence folder with the board, kernel, model, command and both
throttle readings in its header, so the number can be attributed later. Use
--no-throttle-check on a host that has no vcgencmd (it is then written as not checked).
"""
import argparse
import datetime
import os
import platform
import subprocess
import sys


def throttled(check):
    if not check:
        return "not checked"
    try:
        out = subprocess.run(["vcgencmd", "get_throttled"], capture_output=True, text=True,
                             check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        raise SystemExit("bench_runner: vcgencmd get_throttled failed; use "
                         "--no-throttle-check on a host that is not a Raspberry Pi")
    if out != "throttled=0x0":
        raise SystemExit("bench_runner: refused, %s (must be throttled=0x0)" % out)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--model", required=True, help="GGUF file")
    parser.add_argument("--board", required=True, help="board and card, e.g. pi4b-cardA")
    parser.add_argument("--evidence", required=True, help="the project's evidence/ folder")
    parser.add_argument("--llama-bench", default="llama-bench", help="binary to run")
    parser.add_argument("--threads", default="1,2,3,4")
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--no-throttle-check", action="store_true")
    args = parser.parse_args()

    check = not args.no_throttle_check
    before = throttled(check)
    cmd = [args.llama_bench, "-m", args.model, "-p", "64", "-n", "32", "-t", args.threads,
           "-r", str(args.repeat), "-o", "md"]
    started = datetime.datetime.now()
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        raise SystemExit("bench_runner: %s not found" % args.llama_bench)
    except subprocess.CalledProcessError as exc:
        raise SystemExit("bench_runner: llama-bench exited %d\n%s" % (exc.returncode,
                                                                       exc.stderr))
    after = throttled(check)

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
        fh.write("- throttled before: %s\n" % before)
        fh.write("- throttled after: %s\n" % after)
        fh.write("- instrument: upstream llama-bench; the volume's harness is priced "
                 "against this table and published only within 10 percent of it\n\n")
        fh.write(result.stdout)
        if not result.stdout.endswith("\n"):
            fh.write("\n")
    print(result.stdout)
    print("bench_runner: written %s" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
