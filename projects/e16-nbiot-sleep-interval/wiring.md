# e16 wiring: the SIM7020E on the Pi 3B, and the PPK2 on the module rail only

The board is the Raspberry Pi 3 Model B Rev 1.2 (1 GB) with the card `eplepi`
(Raspberry Pi OS, Debian 13 trixie, user `bing`). One HAT on this header, the Waveshare
SIM7020E NB-IoT HAT, and nothing else on this Pi for the whole project.

**The correction this project carries.** The nRF PPK2 measures from 500 nA to about 1 A
and sources 0.8 V to 5.0 V (Nordic's Power Profiler Kit II page, verified Wednesday
14 October 2026). A Raspberry Pi under load draws more than that, so the PPK2 cannot
power or meter a Raspberry Pi, and nothing in this project is a Pi figure. The Pi keeps
its own USB supply. The PPK2 is in ampere-meter mode, in series with the **module's**
5 V feed, and it meters the SIM7020E HAT alone.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 2 and 4 | 5 V | HAT 5V | masked before the HAT is seated, see below; the module's 5 V arrives from above through the PPK2 |
| 6 | GND | HAT GND | common with the bench supply and the PPK2 ground |
| 8 | GPIO14, TXD | module RXD | the PL011, after `dtoverlay=disable-bt`; 3.3 V logic |
| 10 | GPIO15, RXD | module TXD | the PL011; 3.3 V logic |
| 7 | GPIO4 | PWRKEY | the HAT's own PWR jumper, controllable from BCM 4 by default (Waveshare wiki, verified Wednesday 14 October 2026). Never drive it from anything else |
| 1 and 17 | 3V3 | VCCIO reference | the VCCIO jumper sits on 3.3 V, which is the default on this HAT (Waveshare wiki, verified Wednesday 14 October 2026) |
| all others | | pass-through | nothing else is on this header |

## Isolating the module rail before the PPK2 goes in

The HAT carries a full 40-pin pass-through header, so every pin is reachable from
above. The module's 5 V must come from the bench supply through the instrument and from
nowhere else, exactly as the kit labs' PPK2 bench says: a device under test with two
supplies turns every number in the capture into an unknown fraction of the truth.

1. Power everything down. Fit the NB antenna, which is not the 4G antenna.
2. Put a strip of insulating tape over header pins 2 and 4 before the HAT is seated, so
   the Pi's 5 V rail never reaches the HAT. Whether that is the whole of the HAT's 5 V
   path is **to verify** against the HAT's schematic before the first capture; if it is
   not, the route is the one the Linux volume's project 16 takes, which does not seat
   the HAT at all and runs six jumpers instead.
3. Seat the HAT. Check with the supply still off that the HAT is dark.
4. Bench 5 V (the Joy-it SBC-POW-BB rails) into the PPK2's supply side, the PPK2's
   device side to the HAT's 5V pass-through pin, grounds common across the bench
   supply, the PPK2, the HAT and the Pi. The silkscreen names of the two terminals in
   ampere-meter mode are **to verify**: Nordic's ampere-meter page returned 404 on
   Wednesday 14 October 2026, and the functional requirement, the instrument in series
   with the only supply the module has, does not depend on the label.
5. PPK2 USB to the host that runs `tools/ppk2_capture.py`.
6. Only then apply power, to the Pi first and the bench rail second.

## config.txt and the serial port

On a Pi 3 the PL011 belongs to the Bluetooth controller until the overlay moves it, and
the mini UART's baud rate follows the core clock, so the AT dialogue is silence or
rubbish at 115200 without this line.

    # /boot/firmware/config.txt
    enable_uart=1
    dtoverlay=disable-bt

    sudo systemctl disable --now hciuart serial-getty@ttyAMA0
    ls -l /dev/serial0

## Pre-flight, on the Pi 3B over SSH

The Pi 3B+ console is dead on this bench and the Pi 3B's is not used here either: the
UART carries the module. SSH is the route.

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    vcgencmd measure_temp
    ls -l ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf
    ls -l /dev/ttyAMA0

`get_throttled` must read `0x0` before and after the decision run. The model file must
fit under 60 percent of `MemAvailable` or the decision latency is the speed of the card.

## AT bring-up, and what happens without a SIM

The plan needs no port and no module and is what goes into the lab book first:

    build/projects/e16-nbiot-sleep-interval/edgellm-e16 --at-plan

With the HAT powered and a SIM fitted, the same binary walks the plan on the port and
prints the window the network granted:

    build/projects/e16-nbiot-sleep-interval/edgellm-e16 --at /dev/ttyAMA0:115200

A build without `-DEDGELLM_IO_SERIAL=ON` refuses `--at` and names the option. Without a
SIM the sequence stops at `AT+CPIN?`, which is still a pass for the bring-up: it proves
the overlay, the seating, the jumper positions and the port. There is then no attach, no
PSM cycle and no capture, the granted window stays 0 in every scenario, and the rule
answers I60 everywhere. The chapter says so rather than publishing a profile taken
without a SIM, which measures an attach that never completes.

## The capture and its acceptance

    python tools/ppk2_capture.py --seconds 1200 --rate 100 \
        --out projects/e16-nbiot-sleep-interval/evidence/psm_cycle.csv
    python projects/e16-nbiot-sleep-interval/tools/profile_summarise.py \
        projects/e16-nbiot-sleep-interval/evidence/psm_cycle.csv \
        --capacity 2000 --target 365 --granted 3600

The acceptance condition is the spread of the charge per wake over five cycles: at most
10 percent, printed whether it passes or not. The summariser's own recovery is checked
without the instrument with `--self-check`.

## The decision run

    build/projects/e16-nbiot-sleep-interval/edgellm-e16 --project e16 \
      --replay projects/e16-nbiot-sleep-interval/replay/profiles_100.csv \
      --grammar projects/e16-nbiot-sleep-interval/grammar.gbnf \
      --prompt projects/e16-nbiot-sleep-interval/prompt.txt \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
      --n-predict 64 --temperature 0.3 --threads 4 --state /tmp/e16.state \
      --log projects/e16-nbiot-sleep-interval/evidence/e16_run.csv
    vcgencmd get_throttled

## Acceptance

    build/tests/oracle_check projects/e16-nbiot-sleep-interval/evidence/e16_run.csv \
      tests/oracle/e16.expected.csv
    python tools/agreement.py projects/e16-nbiot-sleep-interval/evidence/e16_run.csv
    ctest --test-dir build -R e16_battery_test --output-on-failure

## The one-HAT rule for this Pi

The SIM7020E owns this header. The 3.5 inch LCD, the Explorer700 and the MCC 118 each
want pins this HAT covers, and the ADXL345 and the VL53L4CD of the other Pi 3B projects
want pins 3 and 5. Power off before unseating anything, and take the tape off pins 2
and 4 when the HAT comes off, or the next project on this board has no 5 V.
