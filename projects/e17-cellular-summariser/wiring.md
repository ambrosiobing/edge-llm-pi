# e17 wiring: the SIM7070G HAT on the Raspberry Pi 3 Model B Plus

Project e17 reads one cellular modem over AT and summarises its status. The board is the
Raspberry Pi 3 Model B Plus with the card `eplepi` (Raspberry Pi OS, Debian 13 trixie, user
`bing`), and the part is the Waveshare SIM7070G Cat-M/NB-IoT/GPRS HAT seated on the 40-pin
header. One HAT per header: this HAT cannot share the Pi 3B+ with the Explorer700 of e08, and
the SIM7020E of e16 is a different module on a different board.

**Read the board before you power it.** The wiki states "Logic voltage: 5V/3.3V (switch via
0ohm resistor)" and does not say which way the factory fitted it (read Wednesday 14 October
2026). That is a resistor, not a jumper: it cannot be moved on this bench, which has no
soldering iron. If it sits on the 5 V pad, the Pi's GPIO14 and GPIO15 are outside their
absolute maximum the moment the HAT is powered. So the first action is to find the resistor
and its two pads, record which pad carries it, and photograph it. That record is the
precondition of every step below.

The wiki also says nothing about a UART selection jumper. Its siblings have one, and the kit
labs record the Waveshare **B** position as the one in which the Pi controls the modem (P01
for the SIM7600E-H, P02 for the SIM7020E). Read the silkscreen on this board: if a selection
block exists, it goes to the position that connects the module's UART to GPIO14 and GPIO15,
and if it does not, the HAT's USB cable is the only AT path and `--modem` points at the
`ttyUSB` node instead of `/dev/ttyAMA0`.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 2 and 4 | 5 V | HAT 5 V | through the header, no wire; the HAT's own supply |
| 6 | GND | HAT GND | through the header, no wire |
| 8 | GPIO14, TXD | module RXD | through the header; PL011 after `dtoverlay=disable-bt` |
| 10 | GPIO15, RXD | module TXD | through the header |
| 7 | GPIO4 | PWRKEY | the HAT's own assignment; the PWR jumper's other position takes it to the supply |
| 37 | GPIO26 | DTR | only with the DTR jumper in position A; the default is disconnected |
| all others | | nothing | pass-through; the HAT covers them, so nothing else is reachable |

Antennas before power: the LTE antenna on the main port, and the GNSS antenna if a fix is
wanted (e17 does not use GNSS; the kit lab P03 does). A module that transmits into an open
port stresses its own power amplifier for no result.

## The console

There is none on this board. The Pi 3B+ serial console is dead on this bench (with Bluetooth
disabled the `uart0_pins` node is empty, so GPIO14 is never muxed to the console UART), and
the HAT covers the header anyway. The route is SSH, and `dtoverlay=disable-bt` is what gives
the module the PL011 rather than the mini UART whose baud rate follows the core clock.

## config.txt and the port

    # /boot/firmware/config.txt
    enable_uart=1
    dtoverlay=disable-bt

Then remove `console=serial0,115200` from `cmdline.txt`, and

    sudo systemctl disable --now hciuart serial-getty@ttyAMA0
    ls -l /dev/serial0          # -> ttyAMA0
    sudo usermod -aG dialout bing

## Pre-flight, on the Pi 3B+ over SSH

    ls -l /dev/serial0 /dev/ttyAMA0
    ls /dev/ttyUSB*             # only if the HAT USB cable is connected
    picocom -b 115200 /dev/ttyAMA0

In that session, by hand, before any program touches the port:

    AT
    AT+CPIN?
    AT+CNMP=38          # LTE only: no 2G fallback
    AT+CMNB=1           # Cat-M, not NB-IoT: lock the radio access technology
    AT+CPSI?            # the evidence: it must mention CAT-M or eMTC
    AT+CSQ
    AT+CEREG?
    AT+COPS?
    AT+CPSMS?

`AT+CMNB=1` is the line the day depends on. Without it the module is free to hunt between
Cat-M and NB-IoT, every hunt moves the signal quality and the registration state, and the
summaries then describe the hunting rather than the link. Leave `picocom` before starting the
sampler: two writers on one port interleave their commands and read each other's answers.

## The run

The sampler, for 24 hours, writing one row a minute in the replay CSV shape:

    cd ~/edge-llm-pi
    build/projects/e17-cellular-summariser/edgellm-e17 \
      --modem /dev/ttyAMA0:115200 --sample-interval 60 --max-samples 1440 \
      --sample-log ~/e17_bench_day.csv

Then the loop over the file the sampler wrote, which is the same command as the modelled day
with one path changed:

    cd ~/edge-llm-pi
    build/projects/e17-cellular-summariser/edgellm-e17 --project e17 \
      --replay ~/e17_bench_day.csv \
      --grammar projects/e17-cellular-summariser/grammar.gbnf \
      --prompt projects/e17-cellular-summariser/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --n-predict 64 --temperature 0.3 --threads 4 --state /tmp/e17.state \
      --log projects/e17-cellular-summariser/evidence/e17_run.csv
    vcgencmd get_throttled

## Acceptance

    build/projects/e17-cellular-summariser/e17_at_parser_test
    build/tests/oracle_check projects/e17-cellular-summariser/evidence/e17_run.csv \
      tests/oracle/e17.expected.csv
    python projects/e17-cellular-summariser/tools/hallucination_check.py \
      projects/e17-cellular-summariser/evidence/e17_run.csv --operators A1,Magenta

The parser test must pass before any summary is read. The checker prints the state
agreement over the 96 answered samples, the count of operator names the report carried that
the modem did not report in that minute, and the count of numbers the report carried that
were not in that minute's inputs. All three are published whatever they are.

## Without a SIM

The chapter stops at AT bring-up, and that is still a pass: `AT`, `AT+CPIN?`, `AT+CMNB=1`
and `AT+CSQ` answer with no SIM fitted, the parser's eleven cases pass on the host, and the
acceptance runs on the modelled day. What cannot be taken is a bench day: with no SIM there
is no registration to lose and no operator to report, so `AT+CEREG?` stays at a
not-registered state and every sample is DOWN.
