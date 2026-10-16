# e20 wiring: nothing on the header, and the model is not on the board

Project e20 reads files and the network. No part is wired, no HAT is seated, and nothing is
read from or driven on the 40-pin header. The board is the Raspberry Pi 4 Model B with the
card `eplepi` (Raspberry Pi OS, Debian 13 trixie, user `bing`), reached over SSH or the
Renkforce USB/TTL console at 115200, and one path matters: the bench LAN to skyhorizon, the
Windows 11 laptop whose WSL side serves the model.

## This is the one project in the volume whose inference is not local

The volume's rule is that the Pi runs the model. This chapter breaks it on purpose and says
why. The work is reading nineteen logs, nineteen manifests and nineteen oracle files and
ranking them, which is host-shaped file processing; and the prose the scorecard wants, three
reasons that quote numbers from a table, is what the model ladder's upper rung is for.
Qwen2.5-1.5B-Instruct at Q4_K_M is 1.12 GB, which needs about 1.87 GB of `MemAvailable`
under the volume's rung rule of 60 percent. Neither Pi 3 has it: both are 1 GB boards. The
Pi 4 Model B may or may not have it, and nobody on this bench knows yet, because that
board's memory has not been read; e01 reads it with `free -m`, and until that reading exists
the 1.5B rung on the Pi 4B is "read it" and not "yes".

So the model is served by `llama-server` in skyhorizon's WSL and this project is a client of
it over the LAN. Two consequences are stated rather than hidden. The decision latency now
includes a network round trip and is not comparable with any figure in the other nineteen
chapters. And there is no `llama_state_save_file` over HTTP: what stands in for the saved
system-prompt state of e02 is the server's own prefix cache, asked for with `cache_prompt`,
and the system prompt goes out with every request.

The stated fallback, when the host does not answer, is Qwen2.5-0.5B on the board itself
through the shared Python package's engine module. A run that falls back says so in the
log's `engine` column and in the chapter's figures, so a remote figure and a local figure
can never be read as one number.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| none | none | none | none; nothing is connected to the header for this project |

## The one-HAT rule for this Pi

The Pi 4B header is free during e20. If the MCC 118 (card A) or the WS-28164 (card B) is
seated from a previous sitting it may stay seated: e20 touches neither SPI0 nor any GPIO
line. Either card may be in the board, as long as the repository checkout is on it; the
chapter names `eplepi`.

## Pre-flight, on skyhorizon's WSL side: the server

The model file first, then the server, bound to all interfaces and not to the loopback, or
the Pi cannot reach it. The context is 1024 and not the volume's usual 512, because one
event here is the whole twenty-row scorecard, about 400 tokens of user text, plus up to 192
generated.

    ls -l ~/models/qwen2.5-1.5b-instruct-q4_k_m.gguf
    free -m
    llama-server -m ~/models/qwen2.5-1.5b-instruct-q4_k_m.gguf \
      --host 0.0.0.0 --port 8080 -c 1024 -t 8
    curl -s http://127.0.0.1:8080/health

Then, from the Pi, the same route over the LAN. The address of skyhorizon on the bench LAN
is to verify at the sitting (`hostname -I` on the WSL side, and the Windows host's own
address if WSL port forwarding is in the way, which is the usual reason a Pi sees nothing):

    ping -c 2 skyhorizon.lan
    curl -s http://skyhorizon.lan:8080/health

## Pre-flight, on the Pi 4B

No model file is needed on the board for the remote path. The throttle reading is taken
anyway, because the client runs for several minutes and a board that throttles while it runs
makes the round-trip figures useless.

    cd ~/edge-llm-pi
    vcgencmd get_throttled
    vcgencmd measure_temp
    python -m pip install -e common/python/edgellm
    python -m pip install -e projects/e20-scorecard

## The run

Generate the replay, the snapshot and the oracle first, on the authoring laptop or here; a
replay run is scored against the snapshot beside it and not against the live tree.

    cd ~/edge-llm-pi
    python projects/e20-scorecard/tools/make_replay.py
    python -m edgellm_e20.main --server http://skyhorizon.lan:8080 \
      --log projects/e20-scorecard/evidence/e20_run.csv \
      --proposals projects/e20-scorecard/evidence/e20_proposals.csv \
      --require 0.80
    vcgencmd get_throttled

The scorecard of the repository as it stands, which is what the chapter prints, is a
separate command and reads no network at all:

    cd ~/edge-llm-pi
    python -m edgellm_e20.scorecard --repo . --md

## Acceptance

    python -m pytest projects/e20-scorecard
    python tools/agreement.py projects/e20-scorecard/evidence/e20_run.csv

The acceptance condition is the share of the fifty presentations on which the model's first
identifier is the rule's first identifier: at least 40 of 50, fixed before the run, and
`--require 0.80` makes the run itself exit non-zero below it. The fifty differ only in the
order the twenty rows are shown in, so the share is also a measurement of how much the
answer depends on the presentation, which for the rule is not at all.

The second check is the scorecard itself: twenty rows, no empty cell, every cell that is not
a number reading "not measured" with its reason in the note column. `make_replay.py` refuses
to write anything if a cell is empty, and `tests/test_scorecard.py` checks the same thing
against the repository as it stands.

## config.txt

No overlay and no line is added for e20.
