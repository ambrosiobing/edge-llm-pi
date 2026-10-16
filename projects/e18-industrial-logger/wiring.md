# e18 wiring: the Waveshare WS-28164 on the Pi 4 Model B, card B

Project e18 reads a 64 byte register block over RS485, decides NOMINAL or DRIFT with the
span rule of `rust/src/rules.rs`, writes the verdict on the RS232 channel and asks the
model for the sentence beside it. The board is the Raspberry Pi 4 Model B with the
spare Verbatim 32 GB card, card B of the HAT allocation, the same card e13 and e14 use.
The HAT is the Waveshare WS-28164 isolated RS232, RS485, CAN, CAN FD board, which
Waveshare specify for the Pi 4B and Pi 5 only.

**What is real here and what is not.** The HAT's transceivers are real, and so is the
echo between its two RS485 channels, which is the one electrical test this project can
run. The register stream is replayed from `replay/registers_600.csv`, which
`tools/make_replay.py` writes and which is not a capture. **There is no peer device.**
No Modbus slave, no second logger, nothing that answers a read holding registers
request; the one serial cable on the bench is the Renkforce USB/TTL cable, which is TTL
and not RS232, so it cannot be the RS232 end either. `--live` is written and its frame
decoder is on the oracle path, and it has never been fed by a device.

## The three channels on this HAT

| Channel | Part | Bus | Linux device | Interrupt |
|---|---|---|---|---|
| RS485 A | SC16IS752 channel A, with its RS485 transceiver | SPI1 chip select 0 | `/dev/ttySC0` | GPIO25 |
| RS485 B | SC16IS752 channel B, with its RS485 transceiver | the same SPI1 chip select | `/dev/ttySC1` | GPIO25 |
| RS232 | SP3232EEN on the Pi's own UART | GPIO14 and GPIO15 | `/dev/ttyAMA0` | none |
| CAN FD | MCP2518FD with MCP2562FD | SPI0 chip select 1 | `can0` or `can1` | GPIO24 |
| classic CAN | MCP2515 with SN65HVD230 | SPI0 chip select 0 | `can0` or `can1` | GPIO23 |

The two CAN channels belong to e13 and e14 and e18 touches neither; their overlay lines
stay in `config.txt` because the wiki's tested block carries them and because card B
runs all three projects. The isolated side of the board is alive: it was proven on
Tuesday 7 October 2026 by an acknowledged classic CAN frame crossing the barrier twice,
with two jumper wires between the CAN FD and CAN screw terminals.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 2 and 4 | 5 V | HAT supply, both pins | the HAT's own connector, no lead |
| 6, 9, 14, 20, 25, 30, 34, 39 | GND | HAT ground | the HAT's own connector |
| 12 | GPIO18 | SPI1 chip select 0 to the SC16IS752 | the HAT's own connector |
| 35 | GPIO19 | SPI1 MISO | the HAT's own connector |
| 38 | GPIO20 | SPI1 MOSI | the HAT's own connector |
| 40 | GPIO21 | SPI1 SCLK | the HAT's own connector |
| 22 | GPIO25 | SC16IS752 interrupt, `int_pin=25` | the HAT's own connector |
| 8 | GPIO14, TXD | SP3232EEN driver input, the RS232 transmit path | the HAT's own connector |
| 10 | GPIO15, RXD | SP3232EEN receiver output, the RS232 receive path | the HAT's own connector |
| 16 | GPIO23 | MCP2515 interrupt, e13 and e14 only | the HAT's own connector |
| 18 | GPIO24 | MCP2518FD interrupt, e13 and e14 only | the HAT's own connector |
| 24 and 26 | GPIO8, GPIO7 | SPI0 chip selects, the two CAN controllers | the HAT's own connector |

The HAT takes the whole header through its own connector; nothing above is a loose lead.
The two leads this project does add are between the HAT's own screw terminals:

| From | To | Signal | Wire colour |
|---|---|---|---|
| RS485 A terminal, A | RS485 B terminal, A | the non inverting half of the pair | not recorded, label both ends |
| RS485 A terminal, B | RS485 B terminal, B | the inverting half of the pair | not recorded, label both ends |

**Bare copper under the screw.** The terminal block is a friction fit for a Dupont lead
and nothing more: a lead pushed in and the screw turned down on its insulation holds
mechanically and conducts intermittently, which reads on the bus as short reads and
checksum failures that come and go. Strip about 6 mm, fold the strands back on
themselves so the screw bites on a double thickness, and tug each lead before counting
the connection as made. The echo test counts short reads and checksum failures
separately for exactly this reason: nine short reads and one bad checksum is a
diagnosis, "the echo failed" is not.

**Termination.** Both CAN channels have a 120 ohm termination selectable by a jumper
cap and the wiki does not name the jumper, so the reference designator is read off the
schematic rather than assumed. The RS485 pair on a two node bus of two terminals a few
centimetres apart does not need termination at these rates; whether this board fits one
on the RS485 side at all is **to verify** on the schematic before anything is blamed on
reflections.

## The one HAT rule for this Pi

The WS-28164 takes the whole 40 pin header. The MCC 118 DAQ HAT cannot be seated at the
same time and **its overlay lines must not be in this card's `config.txt`**: both claim
SPI0 chip selects, which is why the bench has a card A for the MCC 118 and a card B for
this HAT. Before anything else, confirm that card B carries no `mcc118` or `daqhat` line
and that card A is not the card in the board.

## config.txt on card B

The wiki's whole tested block, which is the configuration Waveshare publish as working.
e18 needs the first and the fourth line; the rest belong to e13 and e14 and stay, so
that card B is one tested configuration rather than three partial ones.

    dtparam=spi=on
    dtoverlay=i2c0
    dtoverlay=spi1-3cs
    dtoverlay=sc16is752-spi1,int_pin=25
    dtoverlay=mcp2515,spi0-0,oscillator=16000000,interrupt=23
    dtoverlay=mcp251xfd,spi0-1,interrupt=24
    enable_uart=1

**Half applying this block is the easy failure.** `dtoverlay=sc16is752-spi1` without
`dtoverlay=spi1-3cs` leaves SPI1 with no chip select for the part to answer on, and the
two `/dev/ttySC*` nodes never appear; `spi1-3cs` without `dtparam=spi=on` leaves the
controller disabled. The symptom in both cases is the same, two missing device nodes and
nothing in the kernel log that names a cause, so apply all four and read `dmesg` before
touching a lead.

## Free the console UART first

The RS232 channel of this HAT is the Pi's own UART through the SP3232EEN, and on
Raspberry Pi OS that UART is the serial console. Until it is freed, the kernel and a
login prompt are driving the line this project wants, and the symptom is boot messages
arriving at a peer that does not exist and a service that cannot open the port.

    sudo raspi-config nonint do_serial_hw 0
    sudo raspi-config nonint do_serial_cons 1
    sudo systemctl disable --now serial-getty@ttyAMA0.service
    sudo sed -i 's/console=serial0,115200 //' /boot/firmware/cmdline.txt
    grep -o 'console=[^ ]*' /boot/firmware/cmdline.txt
    sudo reboot

The `grep` must print `console=tty1` and nothing else. The exact `raspi-config nonint`
spellings and the `/boot/firmware` path on Debian 13 trixie are **to verify** on the
card; the `cmdline.txt` edit and the `serial-getty` unit are the two things that must
end up done, however they are reached. This also removes the Pi 4B's working serial
console, so the session that does it is over SSH and not over the Renkforce cable.

## Pre-flight, on the Pi 4B over SSH

The card is the hostname. Memory first, because the model file must fit under 60 percent
of what is available, then the bus, then the three devices.

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    ls -l ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf
    ls -l /dev/spidev*
    dmesg | grep -i -e sc16is -e spi1 -e ttySC
    ls -l /dev/ttySC0 /dev/ttySC1 /dev/ttyAMA0
    groups

`vcgencmd get_throttled` must read `0x0`; the Qwen file at 491 MB needs 818 MB
available. `/dev/spidev1.0` must exist, `dmesg` must name the `sc16is752` on `spi1.0`,
the three device nodes must be there, and `groups` must include `dialout` or every open
is refused with permission denied.

One loopback by hand before any program, with the two leads in place, which proves the
leads and the direction control and nothing else:

    stty -F /dev/ttySC1 115200 raw -echo
    cat /dev/ttySC1 | od -An -tx1 &
    printf 'e18 echo probe\r\n' > /dev/ttySC0
    kill %1

## Build

The two halves build separately, which is the chapter's point. In skyhorizon's WSL,
with the stub and no model:

    cd ~/src/edge-llm-pi
    cmake -S projects/e18-industrial-logger/c -B build-e18c -DEDGELLM_STUB_MODEL=ON
    cmake --build build-e18c
    ctest --test-dir build-e18c --output-on-failure
    cargo test --manifest-path projects/e18-industrial-logger/rust/Cargo.toml

On the board, with the real engine, `libllama` must already be built or installed:

    cd ~/edge-llm-pi
    cmake -S projects/e18-industrial-logger/c -B build-e18c \
      -DEDGELLM_LLAMA_INCLUDE_DIR=$HOME/llama.cpp/include \
      -DEDGELLM_LLAMA_LIBRARY=$HOME/llama.cpp/build/bin/libllama.so
    cmake --build build-e18c -j4
    cargo build --release --manifest-path projects/e18-industrial-logger/rust/Cargo.toml

## The acceptance runs

One, the echo, which needs no model and no daemon:

    cd ~/edge-llm-pi
    target/release/edgellm-e18 --echo 1000 \
      --rs485-a /dev/ttySC0 --rs485-b /dev/ttySC1 --baud 115200 \
      --grammar projects/e18-industrial-logger/grammar.gbnf \
      --prompt projects/e18-industrial-logger/prompt.txt

Zero errors and zero sequence gaps over all 1000 frames, or the test has failed and the
leads, the direction control and the overlay are read in that order.

Two, the agreement table, with the daemon running and the real model:

    sudo install -d -o $USER -g $USER /run/edgellm
    build-e18c/edgellm-e18-daemon --socket /run/edgellm/e18-daemon.sock \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --grammar projects/e18-industrial-logger/grammar.gbnf \
      --prompt projects/e18-industrial-logger/prompt.txt \
      --state /run/edgellm/e18.state --n-ctx 512 --threads 4 --n-predict 64 &
    vcgencmd get_throttled
    target/release/edgellm-e18 \
      --replay projects/e18-industrial-logger/replay/registers_600.csv \
      --grammar projects/e18-industrial-logger/grammar.gbnf \
      --prompt projects/e18-industrial-logger/prompt.txt \
      --engine socket --socket /run/edgellm/e18-daemon.sock \
      --rs232 /dev/ttyAMA0 \
      --log projects/e18-industrial-logger/evidence/e18_replay.csv
    vcgencmd get_throttled
    python tools/agreement.py projects/e18-industrial-logger/evidence/e18_replay.csv --md

Three, what the boundary costs per call, three figures on one prompt and one model:

    build-e18c/e18_abi_test \
      --grammar projects/e18-industrial-logger/grammar.gbnf \
      --prompt projects/e18-industrial-logger/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf --calls 200
    target/release/edgellm-e18 --bench 200 --engine socket \
      --socket /run/edgellm/e18-daemon.sock \
      --grammar projects/e18-industrial-logger/grammar.gbnf \
      --prompt projects/e18-industrial-logger/prompt.txt

The first prints the in process figure and the ABI figure; the second the socket figure.
The FFI figure from the service itself needs the `ffi` feature and a link path:

    RUSTFLAGS="-L $PWD/build-e18c" cargo run --release \
      --manifest-path projects/e18-industrial-logger/rust/Cargo.toml --features ffi -- \
      --bench 200 --engine ffi --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --grammar projects/e18-industrial-logger/grammar.gbnf \
      --prompt projects/e18-industrial-logger/prompt.txt

## The units

Two units, in `files/`. The daemon owns the model and no device; the service owns the
three devices and no model. Copy both to `/etc/systemd/system`, create the `edgellm`
user and group, add the service's user to `dialout`, and edit the paths for a Raspberry
Pi OS run; the Yocto recipes under `meta-edgellm/recipes-app/edgellm-e18/` install them
with the paths already right. Stopping the daemon under a running service is the
demonstration the chapter asks for: the log keeps a row per frame with the rule column
filled and the model column empty, and nothing on the RS232 line changes.
