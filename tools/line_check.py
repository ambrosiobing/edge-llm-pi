#!/usr/bin/env python3
"""Every hand-written text file ASCII, every line at most 99 columns, no tab in source.

Walks the tree, skipping .git, build, third_party and the caches, and skipping the
binary kinds. Exit 1 with file:line per problem.

**What is deliberately not checked, and why.** The rule exists for two reasons: code
printed in the book has to fit 99 columns on the page, and a file a person edits should
not need a wide window. Neither applies to what the catalogue's publish step writes into
this repository, so the generated artefacts are skipped outright:

    chapters/*.md             the book, written from the LaTeX sources
    projects/*/README.md      each project's copy of its chapter
    CONTENTS.md, PITFALLS.md  the generated indexes
    figures/*.svg             the figures, written by dvisvgm

Their line lengths come from chapter prose and from vector path data, and the Markdown
is deliberately UTF-8 rather than ASCII, because the converter turns quotation marks into
typographic ones for a browser. Checking them would be checking the renderer's output
against a rule written for a person's editor. Added Friday 16 October 2026, after the
first publish put 42 generated Markdown files and 100 figures into the tree and the check
reported 12,674 problems, every one of them in a file nobody types in.
"""
import os
import sys

SKIP_DIRS = {".git", "build", "third_party", ".venv", "target", "__pycache__",
             ".pytest_cache", ".mypy_cache", ".ruff_cache"}
BINARY_EXT = (".gguf", ".png", ".jpg", ".bin", ".state")
NO_TABS_EXT = (".c", ".h", ".cpp", ".hpp", ".py", ".md", ".cmake", ".txt", ".yml", ".gbnf")
GENERATED_FILES = {"contents.md", "pitfalls.md"}
LIMIT = 99


def is_generated(rel):
    """True for a file the publish step writes, which no person edits by hand."""
    parts = rel.replace("\\", "/").split("/")
    if parts[0] == "chapters" and rel.lower().endswith(".md"):
        return True
    if parts[0] == "figures" and rel.lower().endswith(".svg"):
        return True
    if len(parts) == 3 and parts[0] == "projects" and parts[2] == "README.md":
        return True
    return len(parts) == 1 and parts[0].lower() in GENERATED_FILES


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                           os.path.join(os.path.dirname(__file__), ".."))
    problems = []
    files = 0
    generated = 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name.lower().endswith(BINARY_EXT):
                continue
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root)
            if is_generated(rel):
                generated += 1
                continue
            files += 1
            with open(path, "rb") as fh:
                data = fh.read()
            for number, raw in enumerate(data.split(b"\n"), 1):
                line = raw.rstrip(b"\r")
                if any(b > 127 for b in line):
                    problems.append("%s:%d: non-ASCII byte" % (rel, number))
                # A system prompt is one paragraph the model reads, and a manifest's command
                # strings cannot be wrapped without changing them; both keep the ASCII rule only.
                # An evidence file is the same case and a stronger one: a measuring tool wrote
                # it, its header records the exact command line that produced the numbers, and
                # reflowing that line to fit a style rule would corrupt the record the chapter
                # cites. The ASCII rule still applies, so a file that cannot be read in a plain
                # editor is still refused. Added Friday 16 October 2026 with e01's first run.
                in_evidence = "/evidence/" in rel.replace("\\", "/")
                table_row = name.lower().endswith(".md") and line.lstrip().startswith(b"|")
                exempt = table_row or in_evidence or name in ("prompt.txt", "project.toml")
                if len(line) > LIMIT and not exempt:
                    problems.append("%s:%d: %d columns" % (rel, number, len(line)))
                if b"\t" in line and name.lower().endswith(NO_TABS_EXT) and \
                        name != "CMakeLists.txt":
                    problems.append("%s:%d: tab" % (rel, number))
    for item in problems:
        print(item)
    print("line_check: %d files, %d problems (%d generated files skipped, see the docstring)"
          % (files, len(problems), generated))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
