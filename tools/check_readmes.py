#!/usr/bin/env python3
"""Every published chapter is reachable, and the front page's links exist.

    python3 tools/check_readmes.py

Why this exists. On Sunday 8 November 2026 the front page sent a reader to
`chapters/01-bringup.md` and `chapters/02-decision-loop.md`, neither of which
exists: the published files are `01-bring-up-and-the-honest-tokens-per-second.md`
and `02-the-decision-loop.md`. It also said the chapters were "not in this tree
yet", which the publish step had already made false, and it never mentioned
`00-about-this-volume.md`, so that page was published and named by no page at
all. Three defects, one cause: a paragraph that was true when written and was
falsified by a correct commit.

Two rules, and they fail for different reasons.

A **dead link** is a link whose target is not there. It still reads as a link,
so somebody eventually clicks it and finds out.

An **orphan** is a published chapter that no page names. It reads as nothing,
which is why `00-about-this-volume.md` survived every check this repository
had. `CONTENTS.md` is generated from the numbered chapters, and the volume also
publishes front matter outside that range, so the gap is structural rather than
accidental. The same defect was found in five sibling volumes the same day.

The front page is hand-written and is the thing this checks. `chapters/` and
`CONTENTS.md` are written by the catalogue's publish step and are read here,
never edited.

Exit status is non-zero if anything is unreachable or unresolved, so CI can use
it directly.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    front_path = ROOT / "README.md"
    if not front_path.exists():
        print("REFUSED: no README.md, so there is no front page to check.")
        return 2
    front = front_path.read_text(encoding="utf-8")

    contents = ROOT / "CONTENTS.md"
    reachable = front + (
        contents.read_text(encoding="utf-8") if contents.exists() else "")

    problems = []

    chapters = sorted((ROOT / "chapters").glob("*.md"))
    for page in chapters:
        if page.name not in reachable:
            problems.append(
                "chapters/%s is published and named by neither README.md nor "
                "CONTENTS.md, so it is reachable from no page" % page.name)

    links = 0
    for target in re.findall(r"\]\(([^)\s]+)\)", front):
        if target.startswith(("http://", "https://", "#", "mailto:")):
            continue
        head = target.split("#", 1)[0]
        if not head:
            continue
        links += 1
        if not (ROOT / head).exists():
            problems.append("README.md links %s, which does not exist" % target)

    # A path written in backticks is not a link and no link checker sees it,
    # which is exactly how the two wrong chapter names above survived.
    named = 0
    for target in re.findall(r"`([A-Za-z0-9_][A-Za-z0-9_./-]*\.md)`", front):
        if "/" not in target or "NN" in target:
            continue
        named += 1
        if not (ROOT / target).exists():
            problems.append(
                "README.md names %s in prose, and it does not exist" % target)

    print("%d chapters, %d front-page links, %d paths named in prose"
          % (len(chapters), links, named))
    if problems:
        print("\n%d problem(s):" % len(problems))
        for problem in problems:
            print("  " + problem)
        print("\nFix the front page, not this script, unless the rule is wrong.")
        return 1
    print("every chapter is reachable and every front-page path resolves")
    return 0


if __name__ == "__main__":
    sys.exit(main())
