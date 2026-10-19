# e10 wiring: the MCC 118 on the Pi 4B, the Nucleo DAC on CH0

Project e10 runs on the Raspberry Pi 4 Model B with card A (`eplepi`, Raspberry Pi OS
trixie, user `bing`) and the MCC 118 DAQ HAT seated on the 40-pin header: 8 single-ended
inputs, 12 bits, 100 kS/s aggregate, SPI0 CE0, live on this card since Friday 9 October
2026 (serial DBA2019, firmware 1.03). The stimulus is the NUCLEO-H7A3ZI-Q's DAC on PA4,
0 to 3.3 V, wired to CH0 with a ground wire between the two boards; the firmware note
is `firmware/stimulus_dac.md`. Nothing on any input ever exceeds 10.1 V, and the Nucleo
cannot produce more than 3.3 V, so the stimulus is inside the range by construction.

## Pin table

The HAT covers all forty pins. The rows below are the pins it uses, from the kit labs
volume's P05 record of this HAT; the power rails it draws from are not recorded there.

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 19 | GPIO10 | SPI0 MOSI, through the header | none, the HAT is seated |
| 21 | GPIO9 | SPI0 MISO | none |
| 23 | GPIO11 | SPI0 SCLK | none |
| 24 | GPIO8 | SPI0 CE0, the board's chip select | none |
| 27, 28 | ID_SD, ID_SC | the HAT ID EEPROM, read by `daqhats_read_eeproms` | none |
| 32, 33, 37 | GPIO12, GPIO13, GPIO26 | address jumpers A0, A1, A2; all off is address 0 | none |
| terminal CH0 | none | Nucleo DAC1 OUT1, PA4 (CN7 pin 17, to verify in UM2408) | not recorded; read it off the wire |
| terminal GND beside CH0 | none | any Nucleo GND pin | not recorded; connect this one first |
| terminals CH1 to CH7 | none | nothing; an unconnected input floats and is never quoted | none |

Which power rail the HAT takes from the header is unread; the HAT's user guide and
electrical specification were not reachable from this bench on Wednesday 14 October
2026 (the specification URL redirected to a shop page) and are listed to verify.

## The one-HAT rule for this Pi, and the two cards

The MCC 118 is the only board on the header while e10 runs. Card A carries this HAT;
card B carries the WS-28164 CAN HAT of e13, e14 and e18. The two cannot share one
`config.txt`: the CAN HAT's two overlays claim both SPI0 chip selects, and with them
present `mcc118_open` fails while `hat_list` still reports the board, because
`hat_list` reads the EEPROM copies under `/etc/mcc/hats` and never touches the bus. On
card A those overlay lines are commented out with a dated backup (Friday 9 October
2026). The check that means something is the device node, not the listing:

    ls -l /dev/spidev0.0
    grep -n -E '^dtoverlay=(mcp251xfd|mcp2515)' /boot/firmware/config.txt

The first must exist; the second must print nothing on card A.

## Power order

1. Pi first, with the HAT seated, then wait for the login prompt.
2. Nucleo after, over its own USB. An input driven while the HAT is unpowered is
   outside what the chapter knows about the input stage, so it is never done.
3. Reflash the Nucleo immediately before every capture: the board is shared with the
   firmware volume and the kit labs, and a stale flash reads as a reproduction. Copy
   the stimulus console banner, which prints its build date and time, into the
   evidence folder beside the log.

## Pre-flight, on the Pi 4B over SSH

The EEPROM read precedes the first open, every time the host card has been reflashed.

    cd ~/daqhats; sudo ./install.sh
    sudo daqhats_read_eeproms
    daqhats_list_boards
    ls -l /dev/spidev0.0
    free -m; vcgencmd get_throttled

`daqhats_list_boards` must name one MCC 118 at address 0; `get_throttled` must read
`0x0` before and after the run. The library is built from the daqhats repository by its
`install.sh` (C library and header under `/usr/local`, the EEPROM read, SPI enabled
through `raspi-config` if it was off); the Python wrapper from piwheels (1.4.1.0) beside
the 1.5.0.1 library from source does not matter to this project, which links the C
library only.

## The bench build

    cd ~/edge-llm-pi; cmake -S . -B build -DGGML_NATIVE=ON -DEDGELLM_DAQHATS=ON
    cmake --build build -j4 --target edgellm-e10

## The run

One commanded amplitude per log, named in the file name, so that every capture is
attributable to a stimulus. The Nucleo is commanded over its console (see
`firmware/stimulus_dac.md`); here a 2.000 V step sequence:

    cd ~/edge-llm-pi
    build/projects/e10-mcc118-labeller/edgellm-e10 --project e10 --live mcc118 0:0:2 \
      --n-predict 16 \
      --grammar projects/e10-mcc118-labeller/grammar.gbnf \
      --prompt projects/e10-mcc118-labeller/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --state /tmp/e10.state \
      --log projects/e10-mcc118-labeller/evidence/e10_live_step_2.000V.csv --max-events 3000
    vcgencmd get_throttled

**`--n-predict 16`, and why it is on the bench command and not only in the unit.** The core's
default budget is 8. That number is what truncated 949 objects out of 1000 in e02 on Saturday
17 October 2026, and 64dfa5c raised this project's budget to 16 in its systemd unit for exactly
that reason. The unit is the Yocto deployment; the block above is what a person pastes on the
bench, and until Monday 19 October 2026 it passed no budget at all and therefore ran at 8. The
same gap was found and closed in e02's sheet the same day, which makes three projects where the
remedy reached the unit and missed the command.

It would have been caught rather than published, because `tools/agreement.py` returns non-zero
on any row whose object did not close and lists them. What it would have cost is the sitting:
this run is not a thing to repeat casually with the hardware mounted. Sixteen is generous on
purpose, since an unused token is free, the loop stops at the end-of-generation token and the
root ends at the closing brace. The `n_generated` column of the first real log says how many
were actually spent, and that column settles it rather than this paragraph.

`--live mcc118 <address>:<channel>[:<window_ms>]`; `--max-events 3000` is six seconds of
2 ms windows. The child process prints the HAT's serial, firmware, the single-shot
reading and the granted scan rate on stderr before the first window.

## Acceptance

    python tools/agreement.py projects/e10-mcc118-labeller/evidence/e10_live_step_2.000V.csv
    python projects/e10-mcc118-labeller/tools/make_replay.py --check \
      projects/e10-mcc118-labeller/evidence/e10_live_step_2.000V.csv 2.000

The first prints N, the agreement rate and the confusion table over the windows; a row
whose model column holds none of NOISE, RAMP, STEP fails the chapter. The second takes
the STEP rows, estimates each window's amplitude as the last averaged point minus the
first, and compares the median with the commanded 2.000 V: more than 2 percent off and
the setup fails, not the model. Repeat for the six amplitudes 0.5 to 3.0 V and the
ramp sequence, 300 stimuli in all.

## config.txt

No overlay is added. `dtparam=spi=on` must be present (the daqhats `install.sh` turns
SPI on through `raspi-config` when it is off), and no `dtoverlay=mcp251xfd` or
`dtoverlay=mcp2515` line may be active on card A.
