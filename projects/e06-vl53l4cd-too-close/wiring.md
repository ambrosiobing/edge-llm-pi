# e06 wiring: the VL53L4CD alone on a bare Pi 3 header

Project e06 reads the Adafruit VL53L4CD single-zone time-of-flight sensor over i2c-dev. The
chapter allocates the Raspberry Pi 3 Model B Rev 1.2 (1 GB); the wiring below was made and
verified on the **Raspberry Pi 3 Model B Plus Rev 1.3** on Thursday 29 October 2026, which is
the same memory class and the same bus. No HAT is seated.

**THE SENSOR IS ALONE ON THE BUS, AND THIS PARAGRAPH USED TO SAY OTHERWISE.** It read that
the VL53L4CD "sits on the same bus as the DFRobot SEN0032 ADXL345 of e05, which stays wired",
and that the bus stays at 100 kHz "because the ADXL345 on its current leads answers nothing at
400 kHz". The ADXL345 was retired from the plan on Saturday 24 October 2026 and e05's
substitution away from it was directed on Tuesday 27 October 2026, so the first claim is gone
and the second has lost its reason. The bus still runs at the default **100 kHz**: no
`dtparam=i2c_arm_baudrate` line is set, which was confirmed on the card rather than assumed.

Leaving it alone was deliberate rather than incidental. An X-NUCLEO-IKS5A1 had been on this
bus earlier the same day and would not have collided, answering at 0x19, 0x1e, 0x5c, 0x6a and
0x6b against this part's 0x29. It came off anyway, because **on a bare bus a single address
is proof of the four leads, where among six other devices a mis-wired sensor is
indistinguishable from a correctly wired silent one.**

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | VL53L4CD VIN | **blue** |
| 3 | GPIO2, SDA1 | VL53L4CD SDA | **violet** |
| 5 | GPIO3, SCL1 | VL53L4CD SCL | **red** |
| 9 | GND | VL53L4CD GND | **green** |
| none | | VL53L4CD XSHUT and GPIO1 | unconnected: no shutdown, no interrupt |

Colours recorded from the bench on Thursday 29 October 2026, with the photograph
`evidence/2026-10-29_1626_wiring_vl53l4cd_rpi3bplus.jpg`. They had read "not recorded" since
the chapter was written.

**TWO OF THEM RUN AGAINST CONVENTION AND THAT IS WORTH READING TWICE: red is SCL, not the
supply, and blue is VIN.** Anyone wiring this from habit rather than from the table will put
red on VIN and get nothing, which looks exactly like a dead part.

**The four leads were originally four colours with two the same**, and were changed before
this table was written precisely so the column could disambiguate them. That is the remedy
e03's own wiring file recommends after two identical yellow leads there put the wrong colour
on GPIO17 and GPIO27. Changing them also invalidated the bus check taken before the change,
so `i2cdetect` and a live ranging run were both repeated on the leads recorded here.

**A fifth, black lead appears in the photograph and does not exist.** It is a shadow. Four
jumper housings sit in the breadboard, four leads are listed above, and one device answers on
the bus.

The breakout's VIN accepts 3 to 5 V and its logic is level shifted; GPIO1 is a 2.8 V output
and stays unconnected here.

The breakout carries its own 10 kOhm pull-ups on SDA and SCL; the Pi's pins 3 and 5 carry
fitted 1.8 kOhm pull-ups. In parallel that is about 1.5 kOhm, 2.2 mA sink at 3.3 V, inside
the 3 mA an I2C driver must sink, so no resistor is removed and none is added.

## The one-HAT rule for this Pi

The Pi 3B header is bare for e03, e05 and e06, and all three can be wired at once: e03
uses pins 11, 13, 15 and 25, these two parts use 1, 3, 5, 6, 9, 16 and 17, and the two
sets do not meet. e06 itself needs nothing above pin 17.

The 3.5 inch LCD of e09 cannot be seated beside any of them, and an earlier version of
this paragraph said the opposite. The panel's 26-pin socket mates with header pins 1 to
26 and its carrier then runs the full length of the board over pins 27 to 40 without
contacts, so a seated panel leaves no pin reachable at all: not the 26 it occupies and
not the 14 it buries. Kit lab P19 settled that from the carrier's schematic on Thursday
8 October 2026, and the 2x20 stacking header that would raise every pin above the carrier
is not on the bench. e09's way through is the panel on eleven jumper wires, which re-pins
the sensor's VCC to 17 and the green LED to GPIO4, so e09 is its own sitting and its own
`config.txt` and not an addition to this one.

## config.txt

    dtparam=i2c_arm=on
    dtparam=i2c_arm_baudrate=100000

No overlay for the VL53L4CD: it is reached through `/dev/i2c-1` from user space by the ST
ULD. The ADXL345's overlay from the Linux volume's project 5 may stay; it binds 0x53 and
leaves 0x29 alone.

## Pre-flight, on the Pi 3B over SSH

    free -m
    grep MemAvailable /proc/meminfo
    vcgencmd get_throttled
    vcgencmd measure_temp
    i2cdetect -y 1
    i2ctransfer -y 1 w2@0x29 0x01 0x0f r2

The scan must show `29` and `53` (`UU` at 0x53 when the e05 driver is bound). The transfer
reads the two bytes at register 0x010F, the model id the ULD's `VL53L4CD_GetSensorId`
reads; the value it should return is read from the ULD's user manual before the first run
(to verify, UM2931 was not reachable on Wednesday 14 October 2026). The model file must fit
under 60 percent of `MemAvailable`, which SmolLM2-360M at 0.27 GB does on a 1 GB board
with about 450 MB available; `get_throttled` must read `0x0` before and after the run.

## The run

The binary is built on the board with the ULD out of tree:

    cd ~/edge-llm-pi
    cmake -S . -B build -DGGML_NATIVE=ON -DEDGELLM_VL53L4CD_ULD_DIR=$HOME/vl53l4cd-uld
    cmake --build build -j2 --target edgellm-e06
    build/projects/e06-vl53l4cd-too-close/edgellm-e06 --project e06 \
      --live vl53l4cd /dev/i2c-1:0x29:interval=0.1:budget=50 \
      --grammar projects/e06-vl53l4cd-too-close/grammar.gbnf \
      --prompt projects/e06-vl53l4cd-too-close/prompt.txt \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf --n-predict 40 \
      --state /tmp/e06.state \
      --log projects/e06-vl53l4cd-too-close/evidence/e06_run.csv
    vcgencmd get_throttled

The rule's verdict for every sample is printed to standard output as it is taken; the
model's answer goes only to the log. Stop with Ctrl-C after the 200 approaches; the sampler
stops ranging and the log is complete to the last row.

## Acceptance

    python tools/agreement.py projects/e06-vl53l4cd-too-close/evidence/e06_run.csv
    python tools/agreement.py projects/e06-vl53l4cd-too-close/evidence/e06_run.csv --md

Three quantities: the agreement table over the model-answered rows of the 200 approaches
(the count of rows whose model column holds no one of CLEAR, APPROACHING, TOO_CLOSE must
be zero); the repeatability at a fixed target of 200 mm over 100 samples (mean and sample
standard deviation of v0 from a run with the hand held still, `--max-events 100`); and the
decision latency, the mean of `prompt_ms` plus `generate_ms` over the model-answered rows,
against the approach time at each speed. The oracle check is for the stub replay only:

    build/tests/oracle_check /tmp/e06_stub.csv tests/oracle/e06.expected.csv
