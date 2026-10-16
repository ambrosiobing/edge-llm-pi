# e06 wiring: the VL53L4CD beside the ADXL345 on the Pi 3 Model B

Project e06 reads the Adafruit VL53L4CD single-zone time-of-flight sensor over i2c-dev on
the Raspberry Pi 3 Model B Rev 1.2 (1 GB). The sensor sits on the same bus as the DFRobot
SEN0032 ADXL345 of e05, which stays wired; nothing else is on the header and no HAT is
seated. The bus stays at 100 kHz because the ADXL345 on its current leads answers nothing
at 400 kHz (the measurement of Friday 9 October 2026). The card in the board is named in
the first line of the evidence file on the day of the run.

## Pin table

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | VL53L4CD VIN (and the ADXL345 VCC) | not recorded; read it off the lead |
| 9 | GND | VL53L4CD GND (and the ADXL345 GND) | not recorded |
| 3 | GPIO2, SDA1 | VL53L4CD SDA (and the ADXL345 SDA) | not recorded |
| 5 | GPIO3, SCL1 | VL53L4CD SCL (and the ADXL345 SCL) | not recorded |
| 17 | 3V3 | ADXL345 CS, strapped high (e05) | not recorded |
| 6 | GND | ADXL345 SDO, strapped low (e05) | not recorded |
| 16 | GPIO23 | ADXL345 INT1 (e05) | not recorded |
| none | | VL53L4CD XSHUT and GPIO1 | unconnected: no shutdown, no interrupt |

Two breakouts cannot share one header pin with jumper leads: the 3V3, GND, SDA and SCL
leads of the VL53L4CD join the ADXL345's on a breadboard rail or a split jumper, and the
join is recorded in the evidence file with the date. The breakout's VIN accepts 3 to 5 V and
its logic is level shifted; GPIO1 is a 2.8 V output and stays unconnected here.

The breakout carries its own 10 kOhm pull-ups on SDA and SCL; the Pi's pins 3 and 5 carry
fitted 1.8 kOhm pull-ups. In parallel that is about 1.5 kOhm, 2.2 mA sink at 3.3 V, inside
the 3 mA an I2C driver must sink, so no resistor is removed and none is added.

## The one-HAT rule for this Pi

The Pi 3B header is bare for e03, e05 and e06. The 3.5 inch LCD of e09 covers pins 27 to
40 and leaves pins 1 to 26 reachable, so the video of the volume can film e06 with the LCD
seated; e06 itself needs nothing above pin 17.

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
