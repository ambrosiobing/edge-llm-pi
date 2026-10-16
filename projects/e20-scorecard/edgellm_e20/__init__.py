"""e20: the twenty-row scorecard, the ranking rule and the remote client.

    scorecard.py   the twenty rows, generated from projects/*/project.toml, the
                   evidence logs and tests/oracle/*.csv; never written by hand
    rank.py        the control rule: N ascending, then agreement ascending, then the
                   identifier, so the least evidence and the worst rate come first
    remote.py      the three engines, the remote llama-server first; this is the one
                   project in the volume whose inference is not on the Pi
    main.py        the loop: rank, present, ask, log

Nothing here imports a submodule at package import, so that the scorecard and the
ranking can be run on a host where the shared package is not installed.

Written Wednesday 14 October 2026.
"""
__version__ = "0.1.0"
