#!/usr/bin/env python3
"""Refuse the authoring half of the volume.

The book's LaTeX, PDF and HTML stay in the catalogue; only the Markdown chapters and
the code are public. This check fails when any file git would commit (tracked, or
untracked and not ignored) has one of the authoring extensions or sits under a build/
directory. It runs from .githooks/pre-commit and from CI.
"""
import os
import subprocess
import sys

EXTENSIONS = (".tex", ".pdf", ".html", ".aux", ".log", ".toc")


def file_list(root):
    out = subprocess.run(
        ["git", "-C", root, "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        capture_output=True, check=True)
    return [p for p in out.stdout.decode("utf-8", "replace").split("\0") if p]


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                           os.path.join(os.path.dirname(__file__), ".."))
    bad = []
    for rel in file_list(root):
        parts = rel.replace("\\", "/").split("/")
        if "build" in parts[:-1]:
            bad.append(rel + " (under build/)")
        elif rel.lower().endswith(EXTENSIONS):
            bad.append(rel)
    for item in bad:
        print("authoring file would be committed: " + item)
    print("no_authoring_files: %d refused" % len(bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
