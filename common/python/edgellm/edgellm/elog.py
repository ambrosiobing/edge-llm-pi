"""The per-event log: exactly the columns and the quoting of common/c/src/elog.c.

    t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine

raw joins the values with ';' in %.6g; tag, rule, model and engine are quoted with
doubled inner quotes and newlines replaced by spaces; t_s is %.6f and the two timings
%.3f; a row without a model answer carries "",,,, in the model columns. A log written here
and one written by the C core are byte for byte the same shape, which is what
tests/oracle/eNN.expected.csv and tools/agreement.py rely on.

THE SAME SHAPE MEANS THE SAME DURABILITY AND THE SAME EXCLUSIVITY, which this file did not
have until Tuesday 20 October 2026. It flushed after every row and nothing more, and the
reasoning for both corrections is written out at length in common/c/src/elog.c rather than
repeated here. In short:

  fsync, not only flush. A flush moves bytes out of this process into the kernel, which is
  enough for kill -9 and not enough for a board that restarts, because the page may never
  have reached the card. On Saturday 17 October 2026 that cost about five finished rows and
  left a log whose recorded size counted them. Measured on the bench Raspberry Pi 4 Model B
  writing to ext4 on the SD card, the sync is 6.594 ms a row, which is under 0.3 percent of
  a decision in every project in this volume.

  One writer per log. The file is opened without truncating, an exclusive lock is taken,
  and only then is it emptied. That order is the guard: opening with "w" empties the first
  writer's file before this process finds out it may not have it. A log left by a run that
  has finished is still replaceable, because 21 of this volume's systemd units carry
  Restart= with a fixed --log path and have to reopen their own log.
"""
import os

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
        # fcntl is imported here and not at the top so that importing edgellm stays
        # possible off POSIX; edgellm/__init__.py pulls EventLog in at import time, and
        # the row formatting above is useful to a test on any machine. Opening a log is
        # the part that is POSIX only, exactly as common/c/src/elog.c is.
        import fcntl

        self.path = str(path)
        self.engine = engine_version or ""
        self.rows = 0
        fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_CLOEXEC, 0o644)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            os.close(fd)
            raise RuntimeError(
                "%s is being written by another process, so this one will not start: "
                "do not run two on one log" % self.path)
        # The lock is held, so emptying the file now can only discard this caller's own
        # earlier run and never a live one's rows.
        os.ftruncate(fd, 0)
        self._fh = os.fdopen(fd, "w", encoding="ascii", newline="\n")
        self._fh.write(COLUMNS + "\n")
        self._flush_and_sync()

    def _flush_and_sync(self):
        """Out of this process and onto the device. See the header for why both."""
        self._fh.flush()
        os.fsync(self._fh.fileno())

    def row(self, ev, rule, model=None):
        """Writes one row and puts it on the device. Returns 0, like elog_row()."""
        self._fh.write(format_row(ev, rule, model, self.engine) + "\n")
        self._flush_and_sync()
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
