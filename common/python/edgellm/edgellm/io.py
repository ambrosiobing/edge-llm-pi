"""The one boundary between a project's loop and the hardware: the mirror of io.h.

A project opens exactly one event source: a replayed CSV during development and in CI,
or a live device on the bench. The loop is identical in both cases. The replay reads
the same CSV form as common/c/src/io_replay.c (header t_s,tag,v0,v1,...; blank lines
and lines starting with '#' skipped) so one replay file serves a C and a Python
project alike. The serial source reads the same form, one event per line, from a
sidecar over pyserial; the serial link underneath it is what a project's actuate()
writes a command to. pyserial is imported lazily so the replay path needs nothing.
"""
import time

IO_MAX_VALUES = 16


class Event:
    """One event: a monotonic timestamp, a tag naming the source, up to 16 values."""

    __slots__ = ("t_s", "tag", "values")

    def __init__(self, t_s, tag, values=()):
        self.t_s = float(t_s)
        self.tag = str(tag)[:31]
        self.values = [float(v) for v in values][:IO_MAX_VALUES]

    @property
    def n(self):
        return len(self.values)

    def v(self, index, default=0.0):
        """The index-th value, or the default when the event is shorter."""
        return self.values[index] if index < len(self.values) else default

    def __repr__(self):
        return "Event(%r, %r, %r)" % (self.t_s, self.tag, self.values)


def parse_line(line, line_no=0):
    """One CSV line into an Event, or None for a blank or '#' line; ValueError else."""
    line = line.strip()
    if not line or line.startswith("#"):
        return None
    fields = line.split(",")
    if len(fields) < 2:
        raise ValueError("replay: line %d: no tag column" % line_no)
    try:
        t_s = float(fields[0])
    except ValueError:
        raise ValueError("replay: line %d: t_s is not a number" % line_no) from None
    values = []
    for f in fields[2:2 + IO_MAX_VALUES]:
        f = f.strip()
        if f == "":
            break
        try:
            values.append(float(f))
        except ValueError:
            raise ValueError("replay: line %d: %r is not a number" % (line_no, f)) from None
    return Event(t_s, fields[1].strip(), values)


class ReplaySource:
    """A CSV stands in for the hardware. Iteration ends at the end of the file."""

    backend = "replay"

    def __init__(self, path):
        self.path = str(path)
        self._fh = open(self.path, "r", encoding="ascii", newline="")
        self.line_no = 1
        header = self._fh.readline()
        if not header.startswith("t_s,tag"):
            self._fh.close()
            raise ValueError("replay: %s: header must start with t_s,tag" % self.path)
        self.actuations = []

    def __iter__(self):
        return self

    def __next__(self):
        while True:
            line = self._fh.readline()
            if line == "":
                raise StopIteration
            self.line_no += 1
            ev = parse_line(line, self.line_no)
            if ev is not None:
                return ev

    def actuate(self, what, value):
        """Records the call and returns 0: a rule's action is accepted here as on the bench."""
        self.actuations.append((what, value))
        return 0

    def close(self):
        self._fh.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


class SerialLink:
    """A line-oriented serial port over pyserial: write a command, read a reply line.

    The port is /dev/serial/by-id/... on the bench (e15's wiring.md names it) or any
    path; baud 115200 8N1 by default, which is what the e15 firmware uses. A reply is
    read with a timeout and returned without its line ending; "" means no reply.
    """

    def __init__(self, port, baud=115200, timeout_s=1.0):
        try:
            import serial
        except ImportError:
            raise RuntimeError("SerialLink needs pyserial: pip install -r requirements.txt")
        self.port = port
        self._ser = serial.Serial(port, baud, timeout=timeout_s)
        self.sent = 0
        self.received = 0

    def send_line(self, text):
        data = (text + "\n").encode("ascii")
        self._ser.write(data)
        self.sent += 1

    def read_line(self):
        raw = self._ser.readline()
        if raw:
            self.received += 1
        return raw.decode("ascii", "replace").rstrip("\r\n")

    def command(self, text):
        """Send one line and return the one reply line (or "" on timeout)."""
        self.send_line(text)
        return self.read_line()

    def close(self):
        self._ser.close()


class SerialSource:
    """Events from a sidecar, one CSV line each, in the replay form without the header.

    The sidecar writes "t_s,tag,v0,v1,..." lines; t_s may be the sidecar's own clock
    or empty, in which case the host's monotonic clock is used. actuate() writes
    "<what>=<value>" as one line, which is the command form a sidecar firmware can
    parse with one strchr.
    """

    backend = "serial"

    def __init__(self, port, baud=115200, timeout_s=5.0):
        self.link = SerialLink(port, baud, timeout_s)
        self.port = port
        self.line_no = 0
        self.actuations = []

    def __iter__(self):
        return self

    def __next__(self):
        while True:
            line = self.link.read_line()
            if line == "":
                continue
            self.line_no += 1
            if line.startswith(","):
                line = "%.6f%s" % (time.monotonic(), line)
            ev = parse_line(line, self.line_no)
            if ev is not None:
                return ev

    def actuate(self, what, value):
        self.actuations.append((what, value))
        self.link.send_line("%s=%s" % (what, value))
        return 0

    def close(self):
        self.link.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def open_source(replay=None, backend=None, spec=None):
    """The one source the loop opens: --replay <csv>, or --live serial <port[:baud]>."""
    if replay is not None:
        return ReplaySource(replay)
    if backend == "serial":
        port, _, baud = (spec or "").partition(":")
        if not port:
            raise ValueError("--live serial needs <port>[:baud]")
        return SerialSource(port, int(baud) if baud else 115200)
    raise ValueError("backend %r is not compiled into this package; replay and serial are"
                     % backend)
