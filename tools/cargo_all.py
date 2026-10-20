#!/usr/bin/env python3
"""cargo test over every Rust manifest in the tree, with the known failures named.

    python tools/cargo_all.py            run them all
    python tools/cargo_all.py --list     print what would run, and run nothing
    python tools/cargo_all.py --msrv 1.75  cargo +1.75 check them, and require that
                                          floor to be what each manifest declares

Every Cargo.toml under projects/ and under common/rust/, which is every one in the tree.

Why this is a script and not four lines in the workflow. Until Saturday 17 October 2026 the
workflow globbed `projects/*/Cargo.toml`, which is one level deep, and e18 keeps its halves
in `rust/` and `c/`. So nothing ever compiled e18, and because nothing compiled it, its
dependency on the shared crate could name four cargo features that crate does not declare
and no run said so. The glob is now a walk, and the walk lives here so that it can be read
and run on a laptop rather than only by the runner.

KNOWN is the other half of that lesson, and it is empty as of Sunday 18 October 2026. It
held one entry for one day. e18's Rust half did not compile: it had been written against an
interface the shared crate never had, eight errors, of which two were decisions rather than
repairs. Both were made that day and the entry came out the same evening, not because
someone remembered to tidy it but because **a KNOWN entry that starts passing fails this
script**, and it did, naming the two places to edit. That is the whole reason the mechanism
is shaped this way: a list of excuses nobody has to maintain is the thing that rots. Add an
entry only with the reason and the date, and expect the script to take it away from you.
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

KNOWN = {}


# Where Rust lives in this tree. common/rust is here because nothing ran the shared crate's
# own tests until Saturday 17 October 2026: the workflow named no path under common/, and
# this script's first version walked projects/ only. Four projects depend on that crate and
# roughly eighty of its test functions had never been run by any job, which is the same gap
# as the one-level glob, one directory over.
WALK = ("projects", "common/rust")


def manifests():
    found = []
    for top in WALK:
        base = ROOT / top
        if not base.is_dir():
            print("cargo_all: %s is not a directory, which cannot be right" % top)
            continue
        for p in sorted(base.rglob("Cargo.toml")):
            rel = p.relative_to(ROOT).as_posix()
            if "/target/" in "/" + rel:
                continue
            if len(rel.split("/")) > 4:
                continue
            found.append(rel)
    return found


def declared_msrv(rel):
    """The rust-version of one manifest, read off a live line, or None."""
    for line in (ROOT / rel).read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if s.startswith("#"):
            continue
        if s.startswith("rust-version"):
            return s.split("=", 1)[1].strip().strip('"')
    return None


# Feature sets the --msrv pass checks in addition to each manifest's defaults, with the
# reason, because until Tuesday 20 October 2026 no job named a cargo feature at all and both
# runs in this file used defaults. The default build is the stub one, which is what the
# oracle tests exercise; "live" is the bench build, the socketcan reader that runs on the Pi
# with the CAN HAT, and it had never been compiled by anything. e13 and e14 are the two
# projects whose chapters are the signature video pieces.
#
# WHAT IS DELIBERATELY NOT HERE, so that this list is not mistaken for full coverage.
# "engine" is absent from every entry: it pulls llama-cpp-2, which builds its own copy of
# llama.cpp from source at minutes per crate, and the workflow's engine job already pays
# that cost once for the C projects' different copy. "ffi", e18's, is absent for the reason
# its own manifest gives: it links libedgellm_abi.so, so the C half must be built first and
# the linker handed its path. Both remain uncompiled by any job and that is a known gap, not
# an oversight. edgellm's own "serial" feature needs no entry because e18 depends on the
# crate with it enabled, so checking e18 compiles that module.
EXTRA_FEATURES = {
    "projects/e13-can-honesty/Cargo.toml": ["live"],
    "projects/e14-joint-monitor/Cargo.toml": ["live"],
}

# Feature checks that are allowed to fail, keyed by "<manifest> --features <list>", with the
# reason and the date, and with the same rule KNOWN has: AN ENTRY THAT STARTS PASSING FAILS
# THIS SCRIPT, so a list of excuses cannot quietly become permanent.
#
# Both entries are one finding, made on Tuesday 20 October 2026 by the first run of this
# check. It is not in this repository's code. socketcan 4.0.0, which e13 and e14 both name,
# publishes a manifest in edition 2024 and declares rust-version 1.89. Cargo 1.75 cannot
# parse an edition-2024 manifest at all, so it stops before compiling anything:
#
#     feature `edition2024` is required
#     The package requires the Cargo feature called `edition2024`, but that feature is not
#     stabilized in this version of Cargo (1.75.0)
#
# The layer pins poky at scarthgap, whose rust is 1.75.0, so as things stand e13 and e14
# cannot be cross-built into an image at all, and no image has been built yet to say so.
# This is the dependency half of the floor problem, named as a risk in 6de23ff's message
# before it was observed: our own code compiles at 1.75, and a dependency's does not.
#
# The options, with their costs, and the choice is Joseph's rather than this file's:
#
#   socketcan 3.6.2 is edition 2021 and declares rust-version 1.75, which is exactly what
#     scarthgap supplies. Cheapest for the build, unknown for the code, because e13 and e14
#     are written against the 4.0 API and a major bump usually moves something.
#   a newer rust for Yocto, through a newer poky branch or meta-rust-bin, which changes the
#     release the whole bench shares with embedded-linux-bench and is not e13's decision.
#   do not image these two, and deploy them on Raspberry Pi OS only, which the volume
#     already supports as the first of its two deployment routes; the chapters would say so.
#   call the kernel's CAN socket interface through libc and drop the crate.
KNOWN_FEATURE_FAILURES = {
    "projects/e13-can-honesty/Cargo.toml --features live":
        "socketcan 4.0.0 is an edition-2024 manifest declaring rust-version 1.89, which "
        "cargo 1.75.0 cannot parse; see the note above KNOWN_FEATURE_FAILURES",
    "projects/e14-joint-monitor/Cargo.toml --features live":
        "the same socketcan 4.0.0 manifest, through this project's own dependency on it",
}


def as_version(text):
    """(1, 75, 0) from "1.75" or "1.75.0", or None when it is not a version at all.

    Both spellings are legal in a manifest and rustup names its toolchains with all three
    components, so a string comparison here would report a disagreement that does not
    exist. Compared numerically for that reason.
    """
    try:
        parts = [int(x) for x in text.split(".")]
    except ValueError:
        return None
    while len(parts) < 3:
        parts.append(0)
    return tuple(parts[:3])


def msrv_check(found, want):
    """cargo +<want> check every manifest, and require each to declare <want>.

    WHY A SECOND TOOLCHAIN, and it is the whole reason this option exists. The default run
    above uses whatever `cargo` is on the machine, which on skyhorizon's WSL is 1.99.0 and
    on the runner is the stable of the day. The layer pins poky at scarthgap, whose rust is
    1.75.0, and e05, e13, e14 and e18 inherit cargo, so scarthgap's compiler is what
    actually cross-builds them. A green `cargo test` on stable says nothing about that.

    It said nothing about it on Tuesday 20 October 2026, when the five crates were checked
    under 1.75.0 for the first time: e18's control rule multiplied two f64 constants inside
    a const fn body, which 1.75 refuses with E0658, and every job had been green for days.

    The declaration is checked in the same pass and in both directions. A manifest that
    raises its floor above the toolchain this gate installs is a build that would be refused
    on the image, and a manifest below it is the worse case, since cargo then permits a
    toolchain the code does not compile under, which is what e18's "1.74" did.
    """
    problems = 0
    for rel in found:
        got = declared_msrv(rel)
        if got is None:
            print("FAIL %s declares no rust-version, so nothing pins its toolchain" % rel)
            problems += 1
        elif as_version(got) != as_version(want):
            print("FAIL %s declares rust-version %s, this gate checks %s" % (rel, got, want))
            problems += 1
    for k in [k for k in EXTRA_FEATURES if k not in found]:
        print("FAIL EXTRA_FEATURES names %s, which is not in the tree" % k)
        problems += 1
    planned = set()
    for rel in found:
        for f in EXTRA_FEATURES.get(rel, []):
            planned.add("%s --features %s" % (rel, f))
    for k in [k for k in KNOWN_FEATURE_FAILURES if k not in planned]:
        print("FAIL KNOWN_FEATURE_FAILURES names %s, which this pass does not check" % k)
        problems += 1

    runs = 0
    allowed = 0
    for rel in found:
        for feats in [None] + [[f] for f in EXTRA_FEATURES.get(rel, [])]:
            label = "defaults" if feats is None else "--features " + ",".join(feats)
            key = rel if feats is None else "%s --features %s" % (rel, ",".join(feats))
            print("\n== %s  at %s, %s" % (rel, want, label))
            cmd = ["cargo", "+" + want, "check", "--manifest-path", rel]
            if feats is not None:
                cmd += ["--features", ",".join(feats)]
            r = subprocess.run(cmd, cwd=str(ROOT))
            runs += 1
            if key in KNOWN_FEATURE_FAILURES:
                if r.returncode == 0:
                    print("FAIL %s now compiles under %s and is listed as a known failure.\n"
                          "     Remove it from KNOWN_FEATURE_FAILURES in tools/cargo_all.py."
                          % (key, want))
                    problems += 1
                else:
                    allowed += 1
                    print("known failure, allowed: %s" % KNOWN_FEATURE_FAILURES[key])
            elif r.returncode != 0:
                print("FAIL %s (%s) does not compile under %s, which is what the image uses"
                      % (rel, label, want))
                problems += 1
    print("\ncargo_all --msrv %s: %d manifests, %d checks, %d known failures allowed, "
          "%d problems" % (want, len(found), runs, allowed, problems))
    return 1 if problems else 0


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="print the manifests and exit")
    ap.add_argument("--msrv", metavar="VERSION",
                    help="cargo +VERSION check every manifest and require each to declare "
                         "VERSION; this is the toolchain the Yocto layer supplies")
    args = ap.parse_args(argv)

    found = manifests()
    if not found:
        print("cargo_all: no Cargo.toml under projects/, which cannot be right")
        return 1

    if args.msrv:
        return msrv_check(found, args.msrv)

    if args.list:
        for rel in found:
            mark = "  (known failure)" if rel in KNOWN else ""
            print("%s%s" % (rel, mark))
        print("cargo_all: %d manifests, %d known failures" % (len(found), len(KNOWN)))
        stale = [k for k in KNOWN if k not in found]
        for k in stale:
            print("STALE: KNOWN names %s, which is not in the tree" % k)
        return 1 if stale else 0

    problems = 0
    for rel in found:
        print("\n== %s" % rel)
        r = subprocess.run(["cargo", "test", "--manifest-path", rel], cwd=str(ROOT))
        if rel in KNOWN:
            if r.returncode == 0:
                print("FAIL %s passed and is listed as a known failure.\n"
                      "     Remove it from KNOWN in tools/cargo_all.py and from the\n"
                      "     blocked_on note in its project.toml." % rel)
                problems += 1
            else:
                print("known failure, allowed: %s" % KNOWN[rel])
        elif r.returncode != 0:
            print("FAIL %s" % rel)
            problems += 1

    print("\ncargo_all: %d manifests, %d known failures, %d problems"
          % (len(found), len(KNOWN), problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
