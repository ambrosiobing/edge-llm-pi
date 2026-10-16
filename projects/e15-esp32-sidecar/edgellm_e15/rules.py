"""rules.py: the control rule of project e15, the ESP32 sidecar pattern proposal.

The event is one budget prompt: v0 the energy budget in milliamps, v1 the minimum
on-time the application requires in percent, v2 to v5 the measured mean cost of the
four patterns in milliamps above the LED-off baseline (PPK2, from evidence/, or the
placeholder the replay generator names until the bench run exists). The rule picks
the cheapest pattern whose on-time meets the requirement; a tie goes to the lower
index. The budget is a number the model is told and the rule does not need: the
cheapest qualifying pattern is the one that fits the budget if any does.

Index 0 is PATTERN_A (steady, on 100 percent), 1 PATTERN_B (slow blink, 50 percent),
2 PATTERN_C (fast blink, 20 percent), 3 PATTERN_D (breathing, 50 percent on average).
The names are the grammar's literals letter for letter, see grammars/e15_pattern.gbnf;
the oracle test and the agreement table depend on that equality. The rule is what the
serial command sends to the ESP32; the model's proposal only reaches the log.
"""
from edgellm import register

NAMES = ("PATTERN_A", "PATTERN_B", "PATTERN_C", "PATTERN_D")
ON_PCT = (100.0, 50.0, 20.0, 50.0)   # the on-time of each pattern, fixed by the firmware
COMMANDS = ("A", "B", "C", "D")      # the one-letter command the firmware answers to


class RuleE15:
    project = "e15"
    names = NAMES

    def __call__(self, history, ev):
        if ev.n < 6:                                           # a short event is steady, no guess
            return 0
        best, best_cost = 0, None
        for i in range(4):
            if ON_PCT[i] < ev.v(1):                            # on-time below the requirement
                continue
            cost = ev.v(2 + i)
            if best_cost is None or cost < best_cost:          # strictly cheaper wins a tie
                best, best_cost = i, cost
        return best


rule_e15 = register(RuleE15())
