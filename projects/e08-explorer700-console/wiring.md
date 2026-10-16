# e08 wiring: the Explorer700 seated on the Pi 3 Model B Plus

Project e08 is two processes on the Raspberry Pi 3 Model B Plus (1 GB) with the card
`eplepi` (Raspberry Pi OS, Debian 13 trixie, user `bing`) and the JOY-iT RB-Explorer700
seated on the 40-pin header. There is no flying lead: every part is routed on the HAT,
and the HAT covers the header for the whole sitting. The board's serial console is dead
(pinmux, see the e01 chapter), and the header is covered anyway, so the session is SSH.

## Pin table

Every BCM number below comes from page 3 of the JOY-iT manual (the per-peripheral block
diagram, read Wednesday 14 October 2026), whose page 2 fixes the convention that all
numbers are Raspberry Pi BCM numbers. The Linux volume's project 6 worked the same page
through line by line and its pin map carries the evidence column; none of these rows has
been confirmed on a powered board yet, and the first `i2cdetect`, `gpioinfo` and a thumb
on the joystick fill that column.

| Header pin | BCM | Part pin | Wire colour |
|---|---|---|---|
| 1 | 3V3 | HAT supply | on the HAT, no lead |
| 2, 4 | 5V | HAT supply | on the HAT |
| 3 | GPIO2, SDA | DS3231 0x68, BMP280 0x76, PCF8591 0x48, PCF8574 0x20 | on the HAT |
| 5 | GPIO3, SCL | the same four | on the HAT |
| 6, 9, 14, 20, 25 | GND | HAT ground | on the HAT |
| 19 | GPIO10, MOSI | SSD1306 MOSI | on the HAT |
| 23 | GPIO11, SCLK | SSD1306 SCK | on the HAT |
| 24 | GPIO8, CE0 | SSD1306 CS, driven by the SPI controller | on the HAT |
| 36 | GPIO16 | SSD1306 D/C (printed DNC on the diagram) | on the HAT |
| 35 | GPIO19 | SSD1306 RES, active low | on the HAT |
| 38 | GPIO20 | joystick push contact, closes to ground, internal pull-up | on the HAT |
| 40 | GPIO21 | PCF8574 INT, not used by this project | on the HAT |
| 12 | GPIO18 | IR receiver LFN0038K output, not used by this project | on the HAT |
| 37 | GPIO26 | LED1, not used by this project | on the HAT |
| 7 | GPIO4 | 1-Wire header for a DS18B20, not fitted | on the HAT |
| 29, 31, 33, 32 | GPIO5, 6, 13, 12 | sensor header D0 to D3, nothing attached | on the HAT |

Lines on the PCF8574 expander at 0x20, which gpiod sees as a second chip labelled
`pcf8574`: P0 joystick Left (net A), P1 Up (net B), P2 Down (net C), P3 Right (net D), P4
LED2, P5 and P6 the buttons L3 and L4 on the back of the board, P7 the buzzer (net Buzz).
The four joystick lines are inputs that the expander idles high itself; the buzzer is an
output, and this console drives it only from the rule.

## The one-HAT rule for this Pi

The Explorer700 occupies the whole header. Nothing else is seated on the Pi 3B+ during
e08: not the SIM7070G, not the SIM7020E, not the IKS4A1 flying leads, not the ADXL345.
Power off before seating or unseating it. The OLED's chip select is SPI0 CE0 on GPIO8 and
the overlay below disables `spidev0.0`, so nothing else may use SPI0 on this Pi while the
HAT is on.

## config.txt, on the Pi 3B+

The four overlays are Raspberry Pi OS's own (`/boot/firmware/overlays`), not the Linux
volume's `bench-explorer700` overlay: that one binds the joystick to `gpio-keys-polled` and
the buzzer to `gpio-beeper`, and a line held by a kernel consumer is refused to this
console's gpiod request. The OLED overlay's defaults are D/C on GPIO24 and reset on GPIO25
(read from `ssd1306-spi-overlay.dts` on Wednesday 14 October 2026), so both pins are given.

    dtparam=i2c_arm=on
    dtparam=spi=on
    dtoverlay=i2c-rtc,ds3231
    dtoverlay=i2c-sensor,bmp280,addr=0x76
    dtoverlay=pcf857x,pcf8574,addr=0x20
    dtoverlay=ssd1306-spi,dc_pin=16,reset_pin=19

Reboot after the edit; overlays are applied by the firmware before the kernel starts.

## Pre-flight, over SSH

    i2cdetect -y 1
    ls -l /dev/rtc0 /sys/class/rtc/rtc0/since_epoch
    cat /sys/bus/iio/devices/iio:device*/name
    cat /sys/class/graphics/fb*/name
    gpioinfo | grep -E 'pcf8574|gpiochip'
    gpioget -c /dev/gpiochip0 20
    free -m; vcgencmd get_throttled

Expected: 0x76, 0x48 and 0x20 answer in `i2cdetect` and 0x68 reads `UU` because
`rtc-ds1307` holds it; a device named `bmp280` under IIO; a framebuffer whose name
mentions `ssd130x`; a second gpiochip labelled `pcf8574` with eight lines; GPIO20 reads 1
with the joystick at rest (the console requests it with the pull-up and active low);
`MemAvailable` above 450 MB and `0x0`. A 0x77 instead of 0x76 means the sensor's address
strap differs on this board, and the overlay line takes `addr=0x77`.

The first run of the console prints which framebuffer, chip and IIO device it found. The
things to confirm by hand on the first sitting, each marked to verify in the chapter: the
direction mapping P0 to P3 (push the stick, read the menu cursor), the buzzer's polarity
(a buzzer that sounds constantly except on LOST means the line is active low and the
console's two writes swap), and the framebuffer depth the running kernel's `ssd130x`
reports (the console reads `bits_per_pixel` and draws 32, 16 or 1).

## The run

Two processes. First the daemon, which owns the model:

    cd ~/edge-llm-pi
    build/projects/e08-explorer700-console/edgellm-e08-daemon \
      --socket /tmp/e08.sock \
      --grammar projects/e08-explorer700-console/grammar.gbnf \
      --prompt projects/e08-explorer700-console/prompt.txt \
      --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf \
      --threads 4 --n-predict 64 --temperature 0.3 &

Then the console, which owns the HAT:

    cd ~/edge-llm-pi
    build/projects/e08-explorer700-console/edgellm-e08-console --project e08 --live \
      --socket /tmp/e08.sock \
      --grammar projects/e08-explorer700-console/grammar.gbnf \
      --prompt projects/e08-explorer700-console/prompt.txt \
      --log projects/e08-explorer700-console/evidence/e08_live.csv \
      2> projects/e08-explorer700-console/evidence/e08_live.stderr

Under systemd the two units in `files/` do the same with the socket at
`/run/edgellm/e08.sock`; the daemon unit has `Restart=on-failure` and `RestartSec=1`, the
console unit `Wants=` and `After=` the daemon (not `BindsTo=`, which would stop the
console during the daemon's restart).

## Acceptance

Fifty questions asked from the joystick, then the answer check and the agreement tool:

    python projects/e08-explorer700-console/tools/answer_check.py \
      projects/e08-explorer700-console/evidence/e08_live.csv --strict
    python tools/agreement.py projects/e08-explorer700-console/evidence/e08_live.csv

Pass: 50 of 50 answers carry the reading the question asked for within 1 percent of the
value the console logged with the event. The buzzer never sounds on model output: that is
a code review item, `console_actuate` in `src/console.c` is the only writer of expander
line P7 and it is called with the rule's index before the daemon is asked.

The restart test, under systemd, with the console running and a question asked every
few seconds:

    sudo systemctl kill -s SIGKILL edgellm-e08-daemon.service
    journalctl -u edgellm-e08-console.service -n 5

The console's stderr line `daemon recovered after N s` is the recovery figure, by the
console's CLOCK_MONOTONIC from the first lost reply to the first good one, with 0.5 s
resolution from the retry interval. The expected shape is RestartSec plus the model load
plus the system prompt decode; the measured figure goes into the chapter with its date.
