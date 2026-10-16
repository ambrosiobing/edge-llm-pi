#!/usr/bin/env python3
"""The twenty-row scorecard, generated from the repository and never written by hand.

    python -m edgellm_e20.scorecard                 the table as text
    python -m edgellm_e20.scorecard --md            the same table as Markdown
    python -m edgellm_e20.scorecard --csv out.csv   the same table as CSV
    python -m edgellm_e20.scorecard --strict        exit 1 if any row carries a note

What a row says, and where every cell comes from:

    project      the identifier, from the spine below: twenty rows, always twenty
    language     from projects/<slug>/project.toml when that file exists, else from
    variant      the spine, which is the volume's own declaration; the origin cell
    board        says which of the two the row was built from
    n            the rows of the record named by the source cell
    agreement    the share of the answered rows whose model literal is the rule's,
                 counted as tools/agreement.py counts it: the denominator is the rows
                 that carried an answer, and N beside it is every row
    quieter      the model named a literal EARLIER in the name table than the rule's
    louder       the model named a literal LATER in the name table than the rule's
    no_literal   the model column held nothing the name table names: empty, free text,
                 or an object that did not close inside n_predict
    origin       bench, CI stub, or none; the four counts above partition N
    source       the file the numbers came from
    note         why a cell reads "not measured", or what differs between the spine
                 and a manifest

The order of the name table is what makes "quieter" and "louder" mean something: the
convention of grammars/README.md puts the quiet literal first, so a disagreement
towards index 0 is the model calling an event calmer than the rule did. The table is
read from the project's own grammar, projects/<slug>/grammar.gbnf or
grammars/<id>_*.gbnf. A project whose decision is a sentence and not an enum has no
such table; its row carries N and says in its note that a disagreement there has no
direction and that the chapter's own checker scores it.

A CI figure is not evidence. The ranking of rank.py therefore sorts on the bench
evidence alone: a project with a thousand stub rows and no bench run ranks with the
projects that have nothing at all, and the rule says so in its own reason.

Three ways this generator stops rather than printing a table with a hole in it: a
project.toml that does not parse or has no [project] id, language and variant; a
manifest or a folder naming an id that is not one of the twenty; and an oracle file
that carries no rule and model columns. A missing project.toml is not one of them: the
row is still printed, from the spine, with its reason. That is what the spine is for. A
scorecard that quietly had nineteen rows because one folder was malformed would be
worse than no scorecard at all.

The decision is extracted from the model column as tools/agreement.py extracts it (a
JSON object with a "decision" key, otherwise the bare text), extended with the name
table so that an object keyed on something else, as e03's "colour" and e07's "trend"
are, is read too. The tool stays the reference for one log; this walks nineteen.

Written Wednesday 14 October 2026.
"""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:                                  # Python 3.10 and older
    tomllib = None

# The spine: the twenty projects of the volume, in reading order, with the language,
# the catalogue variant letter and the board each chapter declares. These columns are
# the volume's own table; projects/<slug>/project.toml is the repository's answer to
# it, and a row says which of the two it was built from. A row is never dropped.
SPINE = (
    ("e01", "Bring-up and the honest tokens per second table", "C++", "A",
     "all four boards, no HAT", "instrument"),
    ("e02", "The decision loop: grammar, state reuse, control harness", "C", "C",
     "Pi 4B, no HAT", "decision"),
    ("e03", "LED status explainer", "C", "C",
     "Pi 3B, LEDs on BCM 17, 27, 22", "decision"),
    ("e04", "Pi thermal and throttle narrator", "C client", "D",
     "Pi 4B, no HAT", "decision"),
    ("e05", "ADXL345 event reporter", "Rust", "R7",
     "Pi 3B, ADXL345 on pins 3 and 5", "decision"),
    ("e06", "VL53L4CD too close explainer", "C", "C",
     "Pi 3B, VL53L4CD beside the ADXL345", "decision"),
    ("e07", "IKS4A1 climate and motion narrator", "C++", "B",
     "Pi 3B+, IKS4A1 on flying leads", "decision"),
    ("e08", "Explorer700 console with a model", "C", "O",
     "Pi 3B+, Explorer700", "decision"),
    ("e09", "LCD prompt and verdict display", "C++", "A",
     "Pi 3B, 3.5 inch LCD", "decision"),
    ("e10", "MCC 118 voltage event labeller", "C", "C",
     "Pi 4B card A, MCC 118", "decision"),
    ("e11", "Nucleo CDC sensor hub narrator", "C", "C",
     "Pi 4B, Nucleo over USB CDC", "decision"),
    ("e12", "STWIN.box vibration band reporter", "C++", "A",
     "Pi 4B, STWIN.box over USB-C", "decision"),
    ("e13", "CAN honesty agent", "Rust", "R1",
     "Pi 4B card B, WS-28164", "decision"),
    ("e14", "Joint monitor: CAN plus accelerometer", "Rust", "R2",
     "Pi 4B card B, WS-28164 plus ADXL345", "decision"),
    ("e15", "ESP32 sidecar with a bounded pattern proposal", "Python", "R8",
     "Pi 4B or 3B, ESP32 over USB", "decision"),
    ("e16", "NB-IoT sleep interval from measured microamps", "C", "C",
     "Pi 3B, SIM7020E, PPK2 on the module", "decision"),
    ("e17", "Cellular status summariser", "C", "C",
     "Pi 3B+, SIM7070G", "decision"),
    ("e18", "Mini industrial serial logger", "Rust service, C daemon", "R3 and R5",
     "Pi 4B card B, WS-28164 RS485 and RS232", "decision"),
    ("e19", "Event-driven cellular alert composer", "C", "C",
     "Pi 3B+, SIM7070G plus ADXL345", "decision"),
    ("e20", "What should I measure next, and the scorecard", "Python", "P and R8",
     "Pi 4B, no HAT, network to skyhorizon", "decision"),
)

IDS = tuple(row[0] for row in SPINE)
NOT_MEASURED = "not measured"
BENCH, STUB, NOTHING = "bench", "CI stub", "none"
CSV_COLUMNS = ("project", "language", "variant", "board", "n", "agreement", "quieter",
               "louder", "no_literal", "origin", "source", "note")
# The snapshot a replay run is scored against carries the seven cells the rule sorts
# on and the model is shown, and no prose: a note runs to two hundred characters and a
# variant to seventy, and the repository's own rule is ninety-nine columns in every
# text file, tools/line_check.py included. The language, the variant and the board of
# a snapshot row are read back from the spine, and the full twelve-column form is what
# --csv writes, outside the tree or under build/, which that tool passes over.
SNAPSHOT_COLUMNS = ("project", "n", "agreement", "quieter", "louder", "no_literal",
                    "origin")

# A rule line of a GBNF file, and an enum alternative inside one: the literal X
# written as "\"X\"" in the grammar.
RULE_LINE = re.compile(r"^[ \t]*([A-Za-z0-9_-]+)[ \t]*::=(.*)$")
ENUM_ALT = re.compile(r'^"\\"([A-Za-z0-9_]+)\\""$')


class ScorecardError(Exception):
    """A defect the generator refuses to work around. Printed, and the exit is 1."""


class Row:
    """One line of the scorecard. No cell is ever empty: a number or the words."""

    __slots__ = ("project", "title", "language", "variant", "board", "kind", "n",
                 "agreement", "quieter", "louder", "no_literal", "origin", "source",
                 "note")

    def __init__(self, project, title, language, variant, board, kind):
        self.project = project
        self.title = title
        self.language = language
        self.variant = variant
        self.board = board
        self.kind = kind
        self.n = 0
        self.agreement = None          # None is not measured, never 0.0 by default
        self.quieter = None
        self.louder = None
        self.no_literal = None
        self.origin = NOTHING
        self.source = "none"
        self.note = ""

    @property
    def measured(self):
        return self.agreement is not None

    @property
    def rank_n(self):
        """The evidence the ranking counts: bench rows only, never a CI figure."""
        return self.n if self.origin == BENCH else 0

    @property
    def rank_agreement(self):
        """Not measured sorts below nought percent: an unknown rate is a reason to
        measure, a known bad one is a reason to think."""
        return self.agreement if (self.measured and self.origin == BENCH) else -1.0

    def cells(self):
        """The twelve cells as text, with every unmeasured one spelled out."""
        return {
            "project": self.project,
            "language": self.language,
            "variant": self.variant,
            "board": self.board,
            "n": str(self.n),
            "agreement": ("%.1f percent" % self.agreement) if self.measured
                         else NOT_MEASURED,
            "quieter": str(self.quieter) if self.quieter is not None else NOT_MEASURED,
            "louder": str(self.louder) if self.louder is not None else NOT_MEASURED,
            "no_literal": (str(self.no_literal) if self.no_literal is not None
                           else NOT_MEASURED),
            "origin": self.origin,
            "source": self.source,
            "note": self.note or "none",
        }

    def model_line(self):
        """The row as the model reads it: the three keys the rule sorts on, the record
        behind them, and nothing the rule does not use."""
        if self.origin == BENCH and self.measured:
            return ("%s evidence %d agreement %.1f percent quieter %d louder %d no "
                    "literal %d" % (self.project, self.n, self.agreement, self.quieter,
                                    self.louder, self.no_literal))
        if self.origin == BENCH:
            return ("%s evidence %d agreement not measured (no row carried an answer)"
                    % (self.project, self.n))
        if self.origin == STUB:
            rate = ("%.1f percent" % self.agreement) if self.measured else "no rate"
            return ("%s evidence 0 agreement not measured (a CI stub run of %d rows at "
                    "%s, which is not evidence)" % (self.project, self.n, rate))
        return "%s evidence 0 agreement not measured (no record at all)" % self.project

    def __repr__(self):
        return "Row(%s, n=%d, agreement=%r, origin=%r)" % (
            self.project, self.n, self.agreement, self.origin)


def repo_root(start=None):
    """The repository root: three levels above this file, or an explicit path."""
    if start is not None:
        return Path(start).resolve()
    return Path(__file__).resolve().parents[3]


# ------------------------------------------------------------------ the name table

def read_name_table(text):
    """The ordered enum literals of a GBNF file: the first rule other than root whose
    alternatives are all quoted JSON string literals, in the order the grammar writes
    them, which is the order of that project's own name table. Returns () for a
    sentence grammar, which legitimately has no enum rule."""
    bodies = []
    for raw in text.splitlines():
        line = raw.split("#", 1)[0] if raw.lstrip().startswith("#") else raw
        match = RULE_LINE.match(line)
        if match:
            bodies.append([match.group(1), match.group(2)])
        elif bodies and line.strip():
            bodies[-1][1] += " " + line.strip()
    for name, body in bodies:
        if name == "root":
            continue
        parts = [part.strip() for part in body.split("|")]
        if len(parts) < 2:
            continue
        literals = []
        for part in parts:
            match = ENUM_ALT.match(part)
            if match is None:
                literals = []
                break
            literals.append(match.group(1))
        if literals:
            return tuple(literals)
    return ()


def name_table_for(root, project, slug=None):
    """The project's name table, from its own grammar first, then grammars/."""
    candidates = []
    if slug is not None:
        candidates.append(root / "projects" / slug / "grammar.gbnf")
    candidates += sorted((root / "grammars").glob(project + "_*.gbnf"))
    for path in candidates:
        if not path.exists():
            continue
        try:
            text = path.read_text(encoding="ascii")
        except (OSError, UnicodeDecodeError) as exc:
            raise ScorecardError("%s cannot be read as ASCII: %s" % (path, exc)) from None
        table = read_name_table(text)
        if table:
            return table, path
    return (), None


# ------------------------------------------------------------------ the records

def model_decision(text, table=()):
    """The model's literal, as tools/agreement.py reads it, plus the name table.

    A JSON object with a "decision" key gives that value; an object keyed on anything
    else gives the first of its values that the table names; a JSON array gives the
    same from its first object, which is e20's own answer shape; anything that is not
    JSON gives the bare text. "" means the model answered nothing at all."""
    text = (text or "").strip()
    if not text:
        return ""
    try:
        value = json.loads(text)
    except ValueError:
        return text
    if isinstance(value, dict):
        if "decision" in value:
            return str(value["decision"])
        for item in value.values():
            if isinstance(item, str) and item in table:
                return item
        return str(next(iter(value.values()), "")) if value else ""
    if isinstance(value, list) and value:
        first = value[0]
        if isinstance(first, dict):
            for item in first.values():
                if isinstance(item, str) and item in table:
                    return item
            return str(next(iter(first.values()), "")) if first else ""
        return str(first)
    return text


def score_log(path, table):
    """One log file counted. None when the file is not a log at all, which is how a
    second artefact in evidence/ is passed over instead of being read as a run.

    N is every row of the file. The three classes and the agreements partition the rows
    that carried an answer, which is the denominator tools/agreement.py divides by, so
    a rate here and a rate from the tool on the same log are the same number. A row
    whose model column is empty is a row where the model was not asked, which is how an
    event-driven project logs the events its rule handled alone; those rows are counted
    apart and named in the note, never as a disagreement."""
    with Path(path).open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or "rule" not in reader.fieldnames \
                or "model" not in reader.fieldnames:
            return None
        rows = list(reader)
    counted = {"n": len(rows), "enum": bool(table), "agree": None, "quieter": None,
               "louder": None, "no_literal": None, "answered": None, "unasked": None}
    if not table:
        return counted
    agree = quieter = louder = no_literal = unasked = 0
    for row in rows:
        rule = (row["rule"] or "").strip()
        model = model_decision(row["model"], table)
        if not model:
            unasked += 1
        elif model not in table or rule not in table:
            no_literal += 1
        elif model == rule:
            agree += 1
        elif table.index(model) < table.index(rule):
            quieter += 1
        else:
            louder += 1
    counted.update(agree=agree, quieter=quieter, louder=louder, no_literal=no_literal,
                   answered=len(rows) - unasked, unasked=unasked)
    return counted


def evidence_files(project_dir):
    """Every CSV in the project's evidence/ folder, by name. Nothing is guessed: each
    is offered to score_log, which passes over the ones that are not logs."""
    evidence = Path(project_dir) / "evidence"
    if not evidence.is_dir():
        return []
    return sorted(path for path in evidence.glob("*.csv") if path.is_file())


# ------------------------------------------------------------------ the manifests

def project_dirs(root):
    """Every projects/eNN-*/ folder, by id. A folder whose id is not one of the twenty
    is a defect, not something to pass over."""
    found = {}
    base = Path(root) / "projects"
    if not base.is_dir():
        raise ScorecardError("%s does not exist, so no project can be read" % base)
    for path in sorted(base.iterdir()):
        if not path.is_dir():
            continue
        head = path.name.split("-", 1)[0]
        if not re.fullmatch(r"e\d\d", head):
            raise ScorecardError(
                "projects/%s does not begin with a two-digit project id" % path.name)
        if head not in IDS:
            raise ScorecardError("projects/%s names %s, which is not one of the twenty"
                                 % (path.name, head))
        if head in found:
            raise ScorecardError("two folders claim %s: %s and %s"
                                 % (head, found[head].name, path.name))
        found[head] = path
    return found


def read_manifest(path):
    """projects/<slug>/project.toml as its [project] section, or a loud failure."""
    if tomllib is None:
        raise ScorecardError("reading %s needs tomllib, which is Python 3.11 or newer"
                             % path)
    try:
        with Path(path).open("rb") as handle:
            data = tomllib.load(handle)
    except tomllib.TOMLDecodeError as exc:
        raise ScorecardError("%s does not parse: %s" % (path, exc)) from None
    except OSError as exc:
        raise ScorecardError("%s cannot be read: %s" % (path, exc)) from None
    section = data.get("project")
    if not isinstance(section, dict):
        raise ScorecardError("%s has no [project] section" % path)
    for key in ("id", "language", "variant"):
        if not section.get(key):
            raise ScorecardError("%s has no [project] %s" % (path, key))
    if section["id"] not in IDS:
        raise ScorecardError("%s declares id %r, which is not one of the twenty"
                             % (path, section["id"]))
    return section


def first_token(text):
    """The first word of a language name, so that "C client" and "C" are one language
    and "Rust service, C daemon" is Rust."""
    for part in re.split(r"[ ,]+", str(text).strip()):
        if part:
            return part.lower()
    return ""


# ------------------------------------------------------------------ the scorecard

def build(root=None):
    """The twenty rows, in reading order. Raises ScorecardError on a defect."""
    root = repo_root(root)
    folders = project_dirs(root)
    rows = []
    for project, title, language, variant, board, kind in SPINE:
        row = Row(project, title, language, variant, board, kind)
        folder = folders.get(project)
        notes = []
        if folder is None:
            row.note = ("no projects/%s-*/ folder yet, so this row is the volume's "
                        "declaration and not the repository's" % project)
            rows.append(row)
            continue
        manifest = folder / "project.toml"
        if not manifest.exists() or manifest.stat().st_size == 0:
            notes.append("projects/%s/project.toml is missing or empty, so the "
                         "language, the variant and the board are the volume's "
                         "declaration" % folder.name)
        else:
            section = read_manifest(manifest)
            if section["id"] != project:
                raise ScorecardError(
                    "projects/%s/project.toml declares id %r but the folder is %s"
                    % (folder.name, section["id"], project))
            if first_token(section["language"]) != first_token(language):
                notes.append("language differs: the volume declares %s and the manifest "
                             "says %s" % (language, section["language"]))
            manifest_variant = str(section["variant"]).strip()
            if manifest_variant != variant and \
                    not manifest_variant.startswith(variant.split()[0]) and \
                    not variant.startswith(manifest_variant):
                notes.append("variant differs: the volume declares %s and the manifest "
                             "says %s" % (variant, manifest_variant))
            row.language = str(section["language"])
            row.variant = manifest_variant
            row.board = str(section.get("board") or board)

        table, _ = name_table_for(root, project, folder.name)
        counted = origin = used = None
        for path in evidence_files(folder):
            result = score_log(path, table)
            if result is None:
                notes.append("%s carries no rule and model columns and was passed over"
                             % path.relative_to(root).as_posix())
                continue
            counted, origin, used = result, BENCH, path
        if counted is None:
            oracle = root / "tests" / "oracle" / (project + ".expected.csv")
            if oracle.exists():
                result = score_log(oracle, table)
                if result is None:
                    raise ScorecardError(
                        "%s exists but carries no rule and model columns" % oracle)
                counted, origin, used = result, STUB, oracle

        if counted is None:
            notes.append("no evidence log and no oracle file: neither the bench run nor "
                         "a replay run has been taken")
            row.note = "; ".join(notes)
            rows.append(row)
            continue

        row.n = counted["n"]
        row.origin = origin
        row.source = used.relative_to(root).as_posix()
        if origin == STUB:
            notes.append("the stub in CI, which is not a measurement: the bench run has "
                         "not been taken")
        if not counted["enum"]:
            notes.append("the decision is a sentence and not an enum, so a disagreement "
                         "there has no direction and this scorecard does not classify "
                         "one; the chapter's own checker scores it")
        else:
            row.quieter = counted["quieter"]
            row.louder = counted["louder"]
            row.no_literal = counted["no_literal"]
            if counted["unasked"]:
                notes.append("the model was not asked on %d of the %d rows, where the "
                             "rule acted alone" % (counted["unasked"], counted["n"]))
            if counted["answered"]:
                row.agreement = 100.0 * counted["agree"] / counted["answered"]
            else:
                notes.append("no row carried an answer, so there is no rate: either the "
                             "stub expands this grammar to nothing or the model was "
                             "never asked")
        row.note = "; ".join(notes)
        rows.append(row)

    if len(rows) != len(SPINE):
        raise ScorecardError("the scorecard has %d rows and must have %d"
                             % (len(rows), len(SPINE)))
    return rows


def rows_from_csv(path):
    """A generated scorecard read back, which is what a replay run is given.

    The snapshot in projects/e20-scorecard/replay/ is a dated record of what the
    repository held when tools/make_replay.py ran, so the replay and the oracle do not
    move when a nineteenth chapter lands its software. The cells are parsed back: "not
    measured" becomes None again and "82.3 percent" becomes 82.3."""
    known = {row[0]: (row[1], row[2], row[3], row[4], row[5]) for row in SPINE}
    rows = []
    with Path(path).open(newline="", encoding="utf-8") as handle:
        for raw in csv.DictReader(handle):
            project = (raw.get("project") or "").strip()
            if project not in known:
                raise ScorecardError("%s names %r, which is not one of the twenty"
                                     % (path, project))
            title, language, variant, board, kind = known[project]
            row = Row(project, title, raw.get("language") or language,
                      raw.get("variant") or variant, raw.get("board") or board, kind)
            row.n = int(raw.get("n") or 0)
            for name in ("agreement", "quieter", "louder", "no_literal"):
                cell = (raw.get(name) or "").strip()
                if cell == NOT_MEASURED or not cell:
                    continue
                if name == "agreement":
                    row.agreement = float(cell.split()[0])
                else:
                    setattr(row, name, int(cell))
            row.origin = (raw.get("origin") or NOTHING).strip() or NOTHING
            row.source = raw.get("source") or "none"
            note = (raw.get("note") or "").strip()
            row.note = "" if note == "none" else note
            rows.append(row)
    if len(rows) != len(SPINE):
        raise ScorecardError("%s has %d rows and must have %d"
                             % (path, len(rows), len(SPINE)))
    return rows


def blank_cells(rows):
    """Every cell that is empty, which must be none of them."""
    bad = []
    for row in rows:
        for key, value in row.cells().items():
            if not str(value).strip():
                bad.append("%s %s" % (row.project, key))
    return bad


# ------------------------------------------------------------------ the output

def as_csv(rows, handle, columns=CSV_COLUMNS):
    writer = csv.writer(handle, lineterminator="\n")
    writer.writerow(columns)
    for row in rows:
        cells = row.cells()
        writer.writerow([cells[name] for name in columns])


def as_snapshot_csv(rows, handle):
    """The dated snapshot a replay run is scored against: cells, no prose."""
    as_csv(rows, handle, SNAPSHOT_COLUMNS)


def as_text(rows):
    """The table a person reads in a terminal, and what the chapter quotes."""
    head = "%-4s %-13s %6s %13s %8s %7s %7s  %s" % (
        "No.", "lang, var", "N", "agreement", "quieter", "louder", "noLit", "origin")
    out = [head, "-" * 95]
    for row in rows:
        cells = row.cells()
        out.append("%-4s %-13s %6s %13s %8s %7s %7s  %s"
                   % (row.project, (row.language + ", " + row.variant)[:13], cells["n"],
                      cells["agreement"], cells["quieter"], cells["louder"],
                      cells["no_literal"], row.origin))
    bench = sum(1 for row in rows if row.origin == BENCH)
    stub = sum(1 for row in rows if row.origin == STUB)
    rated = sum(1 for row in rows if row.measured)
    out.append("-" * 95)
    out.append("%d of %d rows from a bench run, %d from a CI stub run, %d from nothing "
               "at all" % (bench, len(rows), stub, len(rows) - bench - stub))
    out.append("%d rows carry an agreement rate; %d read not measured"
               % (rated, len(rows) - rated))
    return "\n".join(out)


def as_markdown(rows):
    out = ["| No. | Language, variant | Board | N | Agreement | quieter / louder / no "
           "literal | Origin and source |",
           "|---|---|---|---|---|---|---|"]
    for row in rows:
        cells = row.cells()
        classes = "%s / %s / %s" % (cells["quieter"], cells["louder"],
                                    cells["no_literal"])
        out.append("| %s | %s, %s | %s | %s | %s | %s | %s, %s |"
                   % (row.project, row.language, row.variant, row.board, cells["n"],
                      cells["agreement"], classes, row.origin, row.source))
    return "\n".join(out)


def notes_of(rows):
    return [(row.project, row.note) for row in rows if row.note]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", help="the repository root; found from this file else")
    parser.add_argument("--csv", help="write the table to this path as CSV")
    parser.add_argument("--md", action="store_true", help="print Markdown")
    parser.add_argument("--strict", action="store_true",
                        help="exit 1 if any row carries a note")
    args = parser.parse_args(argv)
    try:
        rows = build(args.repo)
    except ScorecardError as exc:
        print("scorecard: %s" % exc, file=sys.stderr)
        return 1
    empty = blank_cells(rows)
    if empty:
        print("scorecard: empty cells: %s" % ", ".join(empty), file=sys.stderr)
        return 1
    print(as_markdown(rows) if args.md else as_text(rows))
    if args.csv:
        with open(args.csv, "w", newline="", encoding="ascii") as handle:
            as_csv(rows, handle)
        print("csv written: %s" % args.csv)
    notes = notes_of(rows)
    if notes:
        print("\nnotes, one line per row that has one:")
        for project, note in notes:
            print("  %s: %s" % (project, note))
    return 1 if (args.strict and notes) else 0


if __name__ == "__main__":
    sys.exit(main())
