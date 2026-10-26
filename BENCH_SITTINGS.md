# Bench sittings: which projects share one wiring

Eighteen of the twenty projects have never run on hardware. They were planned one chapter
at a time, so each `wiring.md` describes its own project alone and several of them say the
header is bare for three projects without ever giving the union. This file gives the
unions, so that a sitting covers every project a single wiring can reach instead of one.

A sitting is one wiring, one `config.txt`, one reboot and one pre-flight. Two projects
belong in the same sitting when no pin, no overlay and no driver claim collides. Two
projects that re-pin the same part belong in different sittings even though they use the
same parts, which is the case that was misread until Monday 26 October 2026 and is
recorded in `PITFALLS.md`.

| sitting | host and wiring | projects | reachable today |
|---|---|---|---|
| A | Pi 3B, bare header: ADXL345, VL53L4CD, three LED modules | e03, e05 | yes; **e06 needs the ULD first**, see below |
| B | Pi 3B, the 3.5 inch panel on eleven jumpers, ADXL345 and LEDs re-pinned | e09 | yes |
| C | Pi 4B, no HAT | e04 | needs a second card |
| D | Pi 4B card B, WS-28164 | e13, e14, e18 | yes |
| E | Pi 3B+, bare header, IKS4A1 on flying leads | e07 | yes |
| F | Pi 3B+, Explorer700 | e08 | yes |
| G | Pi 4B card A, MCC 118 and the Nucleo DAC | e10 | yes |
| H | Pi 4B, USB only: Nucleo CDC, STWIN.box, ESP32 and the PPK2 | e11, e12, e15 | yes |
| I | Pi 3B+, SIM7070G | e17, e19 | no, needs a SIM card |
| J | Pi 3B, SIM7020E with the PPK2 | e16 | no, needs a SIM card |
| K | Pi 4B, network to skyhorizon, after the other nineteen have logs | e20 | last by construction |

Sitting A is written out below because it is the largest reduction, three projects for one
wiring. The others are a union of their own `wiring.md` files and have not been checked
pin by pin yet.

## Why e09 is not in sitting A

e09 uses the same ADXL345 and the same three LED modules as e05 and e03, so it looks like
an addition to sitting A and is not one. The panel hangs on eleven jumper wires, and those
eleven wires take header pins 1, 2, 6, 11, 18, 19, 21, 22, 23, 24 and 26. Two of them
collide with sitting A, so e09 moves both: the sensor's VCC from pin 1 to pin 17, and the
green LED from GPIO17 on pin 11 to GPIO4 on pin 7, because pin 11 is the panel's touch
interrupt and the `ads7846` driver does not probe without it. e09 also needs
`dtparam=spi=on`, its own compiled overlay and a different `config.txt`.

So e09 is a re-wiring of sitting A's parts, not an extension of it, and the two cannot be
one evening unless the sensor and LEDs are moved in the middle of it.

A seated panel is not an option in either sitting. Its 26-pin socket mates with header
pins 1 to 26 and its carrier runs over pins 27 to 40 without contacts, so a seated panel
leaves nothing reachable at all. That fact is in `PITFALLS.md`.

## Sitting A: what must exist before the board is touched

Wire all three parts, because the wiring is shared and the leads are the slow half. But
only two of the three can run on the day unless a download has happened first.

- **e06 needs the VL53L4CD ULD on the board**, unpacked at a folder holding its api source
  and header and its `platform.h`, with `-DEDGELLM_VL53L4CD_ULD_DIR` pointing at it.
  Without that flag `projects/e06-vl53l4cd-too-close/CMakeLists.txt` configures the project
  replay-only and says so, so the live sampler simply is not in the binary. The ULD is not
  in this repository on purpose: the ST download page was unreachable from this bench on
  Wednesday 14 October 2026 and the two reachable copies carry two different licence texts,
  BSD 3-Clause in the Arduino mirror and ST's SLA0103 in the X-CUBE tree. **Read the
  STSW-IMG026 page before copying either.** Until then e06's sensor can be wired and
  scanned with `i2cdetect` and `i2ctransfer`, which is worth doing in the same sitting, but
  its 200 approaches wait.
- **The llama.cpp submodule has to be checked out on the board**, since a fresh clone does
  not carry it.
- **The model file has to be on the card.** SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880
  bytes, SHA256 `2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2`, from
  `bartowski/SmolLM2-360M-Instruct-GGUF`. e01 and e02 ran Qwen2.5-0.5B on the Pi 4B, so
  this one may not be there.
- **e05's binary is a first compile anywhere.** It is the only project in sitting A that
  is Rust, it builds the engine through the `engine` feature, and that feature is compiled
  by no job in continuous integration, for the reason `tools/cargo_all.py` states. Its
  cross build in skyhorizon's WSL is written out in the chapter's steps 4 and 5 and has
  never been run.

The long pole is neither the wiring nor the runs: it is building llama.cpp on a 1 GB board
at `-j2`. Start that before the leads go on, not after.

## Sitting A: the union pin table

Board: Raspberry Pi 3 Model B Rev 1.2, 1 GB, card `eplepi` (Raspberry Pi OS, Debian 13
trixie, user `bing`), reached over SSH. Model: SmolLM2-360M-Instruct Q4_K_M, 0.27 GB,
which needs 450 MB of `MemAvailable` on this board.

| Header pin | BCM | Part pin | Projects | Leads on this pin |
|---|---|---|---|---|
| 1 | 3V3 | ADXL345 VCC, and VL53L4CD VIN | e05, e06 | **2** |
| 3 | GPIO2, SDA1 | ADXL345 SDA, and VL53L4CD SDA | e05, e06 | **2** |
| 5 | GPIO3, SCL1 | ADXL345 SCL, and VL53L4CD SCL | e05, e06 | **2** |
| 6 | GND | ADXL345 SDO, strapped low, which selects 0x53 | e05 | 1 |
| 9 | GND | ADXL345 GND, and VL53L4CD GND | e05, e06 | **2** |
| 11 | GPIO17 | green LED module, S | e03 | 1 |
| 13 | GPIO27 | yellow LED module, S | e03 | 1 |
| 14 | GND | green LED module, G | e03 | 1 |
| 15 | GPIO22 | red LED module, S | e03 | 1 |
| 16 | GPIO23 | ADXL345 INT1, wired and not read by any of the three | e05 | 1 |
| 17 | 3V3 | ADXL345 CS, strapped high, which selects I2C and not SPI | e05 | 1 |
| 20 | GND | yellow LED module, G | e03 | 1 |
| 25 | GND | red LED module, G | e03 | 1 |

Eleven signals on thirteen pins, and no collision: e03's lines are 11, 13, 15 with grounds
at 14, 20 and 25, while the two sensors use 1, 3, 5, 6, 9, 16 and 17. Pins 2, 4, 7, 8, 10,
12, 18, 19, 21, 22, 23, 24, 26 and all of 27 to 40 stay free.

**Four pins take two leads each**, which is the fan-out e06's own file names: one jumper
fits one pin, so the 3V3, GND, SDA and SCL leads of the VL53L4CD join the ADXL345's on a
breadboard rail or a split jumper, and the join is recorded in the evidence file with the
date.

**One deviation from e03's own pin table, and why.** e03 puts the ground of all three LED
modules on pin 25, which is three leads on one pin and a third splice for no reason. The
table above spreads them to pins 14, 20 and 25, one each, as e09 already does. The ground
pin is electrically free and e03's file says it was not recorded on Saturday 3 October 2026
and that pin 25 was chosen only because no other project claimed it. Record the deviation
in the evidence file.

**The SEN0032 header is not soldered**, so every one of its seven contacts is friction. A
part that answers nothing is a contact question before it is anything else, and the two
straps, CS to 3V3 and SDO to GND, are what hold the address at 0x53 rather than drifting
between 0x53 and 0x1d.

## Sitting A: one `config.txt`

Three projects, one set of lines. e03 needs none at all, since `gpiochip0` exists from the
kernel at boot, and e06 needs no overlay because the VL53L4CD is reached from user space
through `/dev/i2c-1`. Only e05's driver needs an overlay.

    dtparam=i2c_arm=on
    dtparam=i2c_arm_baudrate=100000
    dtoverlay=<the ADXL345 entry, spelling to confirm on the card>

The bus stays at 100 kHz: the ADXL345 on its current leads answered nothing at 400 kHz on
Friday 9 October 2026, and the VL53L4CD is content at 100 kHz.

**The overlay's name is an open question and must be read off the card, not from this
file.** Two files in this repository spell it two ways: `projects/e05-adxl345-events/wiring.md`
writes `dtoverlay=adxl345-i2c` and `projects/e09-lcd-verdict-display/wiring.md` writes
`dtoverlay=i2c-sensor,adxl345`, citing `boot/overlays/README` read on Wednesday 14 October
2026. One of them is wrong and the card settles it. Read the listing before editing
`config.txt`, then reboot.

## Sitting A: pre-flight, in one pass

Both sensors and all three lines are proved before any model is loaded. On the Pi 3B over
SSH, from the home directory:

```bash
cd ~ && ls /boot/firmware/overlays | grep -i -E "adxl|i2c-sensor"
```

```bash
cd ~ && free -m && grep MemAvailable /proc/meminfo && vcgencmd get_throttled
```

```bash
cd ~ && i2cdetect -y 1
```

```bash
cd ~ && vcgencmd measure_temp
```

```bash
cd ~ && ls /sys/bus/iio/devices/ && cat /sys/bus/iio/devices/iio:device0/name
```

```bash
cd ~ && cat /sys/bus/iio/devices/iio:device0/in_accel_scale
```

```bash
cd ~ && for a in x y z; do cat /sys/bus/iio/devices/iio:device0/in_accel_${a}_raw; done
```

```bash
cd ~ && i2ctransfer -y 1 w2@0x29 0x01 0x0f r2
```

```bash
cd ~ && gpiodetect && gpioinfo -c gpiochip0 17 27 22 && groups
```

What each one has to say, and the sitting stops if it does not:

- the overlay listing names exactly one ADXL345 entry, and that spelling is what goes in
  `config.txt`
- `MemAvailable` is at least 450 MB and `get_throttled` reads `0x0`
- `i2cdetect` shows **both** `29` and `53`, with `UU` at 53 once the driver is bound, which
  is right and not a fault
- the IIO `name` file reads `adxl345`, and the three raw values give about 256 counts on
  the axis pointing up, which is 1 g at full resolution; a value that does not change while
  the part is moved is a contact and not a sensor
- the transfer returns the VL53L4CD model id from register 0x010F; **the value it should
  return is still to verify**, because UM2931 was not reachable on Wednesday 14 October 2026
- `gpiodetect` lists `gpiochip0`, the three lines read `unused`, and `groups` includes `gpio`

## Sitting A: the three runs

Build once, then run the three in this order: e03 first because it is the cheapest proof
that the board, the model and the loop work at all, then e05's hundred hand-made events,
then e06's two hundred approaches, which is the longest.

e03 and e06 are CMake targets and build on the board. e05 is Rust, and a 1 GB Pi 3B is a
slow place to compile it, so the better route is the cross build in skyhorizon's WSL with
the binary copied over, as `projects/e05-adxl345-events/wiring.md` describes. That is the
one step in this sitting that is not on the Pi, and it has to happen before the sitting
rather than during it.

The run and acceptance commands are each project's own and are not copied here, because a
copy is a second place to drift. Take them from, in order:

1. `projects/e03-led-explainer/wiring.md`, sections "Build on the board", "The run", "Acceptance"
2. `projects/e05-adxl345-events/wiring.md`, sections "The run", "Acceptance"
3. `projects/e06-vl53l4cd-too-close/wiring.md`, sections "The run", "Acceptance"

Two things that belong to the sitting rather than to any one project:

- **`vcgencmd get_throttled` reads `0x0` before and after every one of the three runs.** A
  1 GB board running an engine is the case where it does not, and a run with a bit set is
  not published.
- **Pass `--n-predict` explicitly on every command.** The core's default budget is 8, which
  truncated 949 objects out of 1000 in e02 on Saturday 17 October 2026. Three projects have
  since had the remedy reach their systemd unit and miss the bench command. e03 and e05 use
  64, e06 uses 40.

## What one sitting buys

Three of the eighteen unmeasured projects, from one wiring, one reboot and one pre-flight.
Each needs its own run and its own labelling, so the saving is the setup and not the
measurement: e05's hundred events are still made and labelled by hand, and e06's two
hundred approaches are still two hundred approaches.

None of the three needs a second day. The two-day repeat is e01's clause, for the tokens
per second table, and it is already satisfied by the records of Friday 16 and Saturday 17
October 2026.
