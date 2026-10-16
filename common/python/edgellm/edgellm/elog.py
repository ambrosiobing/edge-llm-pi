"""The per-event log: exactly the columns and the quoting of common/c/src/elog.c.

    t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine

raw joins the values with ';' in %.6g; tag, rule, model and engine are quoted with
doubled inner quotes and newlines replaced by spaces; t_s is %.6f and the two timings
%.3f; a row without a model answer carries "",,,, in the model columns. The file is
flushed after every row so that a process ended with kill -9 leaves every decision it
took on disk. A log written here and one written by the C core are byte for byte the
same shape, which is what tests/oracle/eNN.expected.csv and tools/agreement.py rely on.
"""

COLUMNS = "t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine"


def quoted(text):
    """CSV quoting as write_quoted() in elog.c: doubled quotes, newlines to spaces."""
    out = ['"']
    for ch in text:
        if ch == '"':
            out.append('""')
        elif ch in "\r\n":
            out.append(" ")
        else:
            out.append(ch)
    out.append('"')
    return "".join(out)


def fmt_g6(value):
    """C's %.6g for the raw column (Python's %g is the same format)."""
    return "%.6g" % value


def format_row(ev, rule, model, engine):
    """One log line without its newline; model is a Result or None."""
    raw = ";".join(fmt_g6(v) for v in ev.values)
    head = "%.6f,%s,%s,%s," % (ev.t_s, quoted(ev.tag), raw, quoted(rule or ""))
    if model is not None:
        tail = "%s,%.3f,%.3f,%d," % (quoted(model.text), model.prompt_ms,
                                     model.generate_ms, model.n_generated)
    else:
        tail = '"",,,,'
    return head + tail + quoted(engine)


class EventLog:
    """elog_open, elog_row, elog_close."""

    def __init__(self, path, engine_version):
        self.path = str(path)
        self.engine = engine_version or ""
        self.rows = 0
        self._fh = open(self.path, "w", encoding="ascii", newline="\n")
        self._fh.write(COLUMNS + "\n")
        self._fh.flush()

    def row(self, ev, rule, model=None):
        """Writes and flushes one row. Returns 0, like elog_row()."""
        self._fh.write(format_row(ev, rule, model, self.engine) + "\n")
        self._fh.flush()
        self.rows += 1
        return 0

    def close(self):
        if self._fh is not None:
            self._fh.close()
            self._fh = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
