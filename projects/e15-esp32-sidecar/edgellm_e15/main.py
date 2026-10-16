"""main.py: project e15 on the shared loop, common/python/edgellm/edgellm/loop.py.

The loop opens the event source (the replayed budget file; there is no live budget
source, the budget is a number), looks the rule up with find("e15"), runs it on every
event, calls actuate() with the rule's index, asks this file for the user text, asks
the engine for one grammar-constrained answer with the saved system-prompt state
restored, and writes the log row with the rule column beside the model column. This
file owns the prompt and the serial link to the ESP32.

The ESP32 is real and the budgets are modelled, so this file takes one option of its
own, --serial <port>, strips it from argv before main() sees it, opens the link over
pyserial at 115200 and sends the rule's one-letter command through it; the reply line
"pattern X" is checked against the command and a mismatch counts as an actuation
error. Without --serial (CI, the host) the replay source records the actuation. The
model has no path to the serial link: the index comes from rules.py through the loop.
"""
import sys

from edgellm import SerialLink, main as loop_main

from .rules import COMMANDS, rule_e15


class E15Project:
    def __init__(self, link=None):
        self.link = link
        self.sent = 0
        self.mismatches = 0

    def actuate(self, source, rule, index, ev):
        """The rule's pattern goes to the ESP32 as one letter; the reply must echo it."""
        if self.link is None:
            return source.actuate("pattern", index)
        reply = self.link.command(COMMANDS[index])
        self.sent += 1
        if reply != "pattern " + COMMANDS[index]:
            self.mismatches += 1
            return -1
        return 0

    def format_prompt(self, ev, history):
        """The costs, the budget and the requirement; the on-times are in prompt.txt."""
        if ev.n < 6:
            return None                                       # log the rule alone
        return ("cost mA: A %.2f, B %.2f, C %.2f, D %.2f. Budget %.1f mA. "
                "Minimum on-time %.0f percent. Decide."
                % (ev.v(2), ev.v(3), ev.v(4), ev.v(5), ev.v(0), ev.v(1)))


def split_serial(argv):
    """Takes --serial <port[:baud]> out of argv; returns (rest, port, baud)."""
    rest, port, baud = [], None, 115200
    i = 0
    while i < len(argv):
        if argv[i] == "--serial" and i + 1 < len(argv):
            port, _, b = argv[i + 1].partition(":")
            baud = int(b) if b else 115200
            i += 2
            continue
        rest.append(argv[i])
        i += 1
    return rest, port, baud


def main(argv=None):
    argv, port, baud = split_serial(sys.argv[1:] if argv is None else argv)
    link = SerialLink(port, baud) if port else None
    project = E15Project(link)
    try:
        rc = loop_main(argv, project)
    finally:
        if link is not None:
            link.command("O")                                 # the LED off before leaving
            link.close()
    if project.mismatches:
        print("e15: %d of %d commands were not echoed by the sidecar"
              % (project.mismatches, project.sent), file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())
