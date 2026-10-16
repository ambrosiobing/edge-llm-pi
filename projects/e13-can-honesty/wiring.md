# e13 wiring: the WS-28164 as its own two node CAN bus

Board: Raspberry Pi 4 Model B with **card B**, the spare Verbatim 32 GB. HAT: the Waveshare
WS-28164 isolated RS232, RS485, CAN, CAN FD board, seated on the 40 pin header. The bus is
the HAT talking to itself: two jumper wires between the CAN FD screw terminal and the CAN
screw terminal, proven as a working two node classic CAN bus on Tuesday 7 October 2026 with
frames in both directions, zero errors and the isolated side alive.

Two controllers are on the HAT, so the Pi supplies none:

| Channel | Controller | Transceiver | Chip select | Interrupt | Crystal | Termination |
|---|---|---|---|---|---|---|
| CAN FD | MCP2518FD | MCP2562FD | SPI0 CE1 | GPIO24 | 40 MHz (Y2) | 120 ohm cap fitted |
| classic CAN | MCP2515 | SN65HVD230 | SPI0 CE0 | GPIO23 | 16 MHz | 120 ohm cap fitted |

**It runs classic CAN, not CAN FD.** The MCP2515 cannot do flexible data frames, so the
pair can only agree on classic frames. Nothing measured on this bus says anything about
CAN FD, and a 2 Mbit data phase test here would be measuring one controller against
itself. The FD data phase waits for an FD rated transceiver on the Nucleo end.

## Pin table

The HAT uses the header; no flying leads. The rows below are the lines it claims.

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 19 | GPIO10, MOSI | SPI0 MOSI, both controllers | HAT, no wire |
| 21 | GPIO9, MISO | SPI0 MISO, both controllers | HAT, no wire |
| 23 | GPIO11, SCLK | SPI0 SCLK, both controllers | HAT, no wire |
| 24 | GPIO8, CE0 | MCP2515 chip select | HAT, no wire |
| 26 | GPIO7, CE1 | MCP2518FD chip select | HAT, no wire |
| 16 | GPIO23 | MCP2515 interrupt | HAT, no wire |
| 18 | GPIO24 | MCP2518FD interrupt | HAT, no wire |
| 1, 2, 4, 6, 9 | 3V3, 5V, GND | HAT supply and ground | HAT, no wire |
| CAN FD terminal H | n/a | CAN terminal H | jumper wire 1, colour not recorded |
| CAN FD terminal L | n/a | CAN terminal L | jumper wire 2, colour not recorded |

**The screw terminal is a friction fit for a Dupont lead.** A bare lead pushed into the
block holds well enough to pass frames and badly enough to fail intermittently later.
Clamp the bare copper under the screw before any measurement is taken, and say in the
evidence file that it was clamped.

## The one HAT rule for this Pi

This HAT and the MCC 118 DAQ HAT cannot share one `config.txt`: both claim SPI0 chip
selects. That is why there are two cards. Card A carries the MCC 118 overlay and is project
e10's; card B carries the lines below and is e13, e14 and e18's. **Check which card is in
the board before anything else**, because an e13 run on card A finds nothing on the bus and
looks like a wiring fault.

## config.txt, card B

    dtparam=spi=on
    dtoverlay=mcp2515,spi0-0,oscillator=16000000,interrupt=23
    dtoverlay=mcp251xfd,spi0-1,interrupt=24

The wiki's tested block also carries `dtoverlay=i2c0`, `dtoverlay=spi1-3cs` and
`dtoverlay=sc16is752-spi1,int_pin=25` for the serial channels; e18 needs those, e13 does
not, and leaving them out means a failure on this bus has one place to be.

## Pre-flight, on the Pi 4 Model B with card B

**Read which controller the kernel called what, every session.** The names swap between
boots, because the two drivers probe in whatever order they probe. Never assume `can0`.

    dmesg | grep mcp25
    ls -l /sys/class/net/can0/device/driver /sys/class/net/can1/device/driver
    ip -details link show

The second command is what the service itself reads: `mcp251xfd` is the CAN FD controller
and `mcp251x` is the MCP2515, and `edgellm-e13 --live` binds each node to the name the
kernel gave its driver this boot. The driver names are read from the mainline driver
modules and are to verify against the first `dmesg` on the bench.

Then bring both links up at 500 kbit with automatic recovery, and create the virtual
interface the third node lives on:

    sudo ip link set can0 up type can bitrate 500000 restart-ms 100
    sudo ip link set can1 up type can bitrate 500000 restart-ms 100
    sudo modprobe vcan
    sudo ip link add dev vcan0 type vcan
    sudo ip link set vcan0 up
    vcgencmd get_throttled

`get_throttled` must read `0x0` before and after the run. `restart-ms 100` is the recovery
policy of this project, chosen and not inherited: a controller that has taken itself off
the bus comes back a tenth of a second later, which is inside one heartbeat period, so a
node that went bus off never crosses the three period timeout and never reads ABSENT for
an error it recovered from.

## The third node, modelled by declaration

    cangen vcan0 -I 703 -L 1 -D 05 -g 1000 -n 1000

One heartbeat per second on the virtual interface, identifier 0x703, one byte of node
state. It has no transceiver and no wire, so the service reports it MODELLED whatever its
counters say, and the figure draws it dashed. `cangen` is from can-utils.

## The run

    cd ~/edge-llm-pi
    ./target/release/edgellm-e13 --live --periods 1000 \
      --grammar projects/e13-can-honesty/grammar.gbnf \
      --prompt projects/e13-can-honesty/prompt.txt \
      --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
      --state /tmp/e13.state \
      --log projects/e13-can-honesty/evidence/e13_live.csv
    vcgencmd get_throttled

## Acceptance

    ip -details -statistics link show can0
    ip -details -statistics link show can1
    python tools/agreement.py projects/e13-can-honesty/evidence/e13_live.csv --md

The bus off acceptance is read from `ip -details -statistics link show`: the state line,
the `berr-counter` pair, and the `restarts` count in the statistics block, read before the
wire is pulled, while it is out, and after it is back. Every recorded measurement names the
controller, never only the interface, because the interface name is this boot's and the
controller is the part.
