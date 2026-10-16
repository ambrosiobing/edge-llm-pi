#!/usr/bin/env python3
"""Every grammar exists twice; this refuses to let the two copies drift apart.

    python3 tools/grammar_pairs.py

Each project keeps its grammar at projects/<slug>/grammar.gbnf, and the same text lives
under grammars/<name>.gbnf where the volume's chapter cites it and where the collection can
be read as a whole. Nineteen of the twenty projects have both.

**Why that is a hazard rather than a convenience.** The three readers disagree about which
copy they mean. The oracle test registered in projects/<slug>/CMakeLists.txt passes
GRAMMAR grammar.gbnf, the project folder's copy. The manifest project.toml names the
grammars/ copy in its [project] grammar field. The chapter prints one of them and the lint
compares the printed block against a file. While the two agree, all three readers are
talking about the same thing and nobody notices there are two. The moment someone edits one,
the test keeps passing against the old rules while the book shows the new ones, and no gate
in this repository says anything, because none of them compares the pair.

The manifest is treated as naming the canonical copy, since it is the file CI and the tools
already read. A project whose manifest has no grammar field is skipped: e01 is the
instrument and decides nothing, so it has no enum and no grammar.

Added Friday 16 October 2026, found while reading e02 ahead of its sitting rather than
during one. Compare bytes, not parsed content: a grammar that differs only in whitespace is
still two files that someone has edited once and not twice.
"""
import os
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def pairs(root=ROOT):
    """(project directory name, manifest path, canonical path, folder copy path)."""
    found = []
    projects = os.path.join(root, "projects")
    for slug in sorted(os.listdir(projects)):
        manifest = os.path.join(projects, slug, "project.toml")
        if not os.path.isfile(manifest):
            continue
        with open(manifest, "rb") as fh:
            data = tomllib.load(fh)
        named = data.get("project", {}).get("grammar")
        # A project that decides nothing says so in prose rather than leaving the field
        # out: e01's manifest reads "none: this project decides nothing, it is the
        # instrument". That is a sentence and not a path, so the field is taken as naming a
        # grammar only when it actually names a .gbnf file. Checked for the suffix rather
        # than for the word "none", because the next such project will phrase it its own way.
        if not named or not named.endswith(".gbnf"):
            continue
        found.append((slug, manifest, os.path.join(root, named),
                      os.path.join(projects, slug, "grammar.gbnf")))
    return found


def check(root=ROOT):
    problems = []
    seen = 0
    for slug, manifest, canonical, copy in pairs(root):
        seen += 1
        if not os.path.isfile(canonical):
            problems.append("%s: %s names a grammar that does not exist: %s"
                            % (slug, os.path.relpath(manifest, root),
                               os.path.relpath(canonical, root)))
            continue
        if not os.path.isfile(copy):
            problems.append("%s: no grammar.gbnf in the project folder, but the oracle "
                            "test asks for one" % slug)
            continue
        with open(canonical, "rb") as fh:
            a = fh.read()
        with open(copy, "rb") as fh:
            b = fh.read()
        if a.replace(b"\r\n", b"\n") != b.replace(b"\r\n", b"\n"):
            problems.append("%s: %s and %s have drifted apart; the oracle test reads the "
                            "second and the chapter cites the first"
                            % (slug, os.path.relpath(canonical, root),
                               os.path.relpath(copy, root)))
    return seen, problems


def main():
    seen, problems = check()
    for item in problems:
        print(item)
    print("grammar_pairs: %d grammars paired, %d problems" % (seen, len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
