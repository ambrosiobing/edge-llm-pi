# e08: the HAT is confirmed, the buzzer is active low, and the under-voltage was the
# supply. Saturday 14 November 2026, 00:25 to 00:45

    board      Raspberry Pi 3 Model B Plus Rev 1.3, revision a020d3, 905 MB
    card       rpi3BV1p2, Raspberry Pi OS trixie, kernel 6.18.50+rpt-rpi-v8, user bing
    hat        Waveshare Explorer700, seated with the board powered off
    overlays   dtparam=spi=on, i2c-rtc ds3231, i2c-sensor bmp280 addr=0x76,
               pcf857x pcf8574 addr=0x20, ssd1306-spi dc_pin=16 reset_pin=19
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 258 MiB against 760 MiB available
    closed     no acceptance clause. This is bring-up and two defects found in it.

## ONE: THE UNDER-VOLTAGE WAS THE SUPPLY, AND THE SIGNATURE WAS ALREADY ON RECORD

The first boot with the HAT seated read `throttled=0x50000` at 34.3 C, which is bit 16,
under-voltage has occurred, and bit 18, throttling has occurred. **Bit 19, the soft
temperature limit, was not set and the board was cold**, so this was never heat.

`0x50000` is character for character a state recorded on Tuesday 27 October 2026 in
`projects/e03-led-explainer/evidence/2026-10-27_1905_supply-substitution_pi4b-eplepi.md`,
in the list belonging to the white ISY adapter, against the black 65 W adapter's
`0x0 0x80000 0x80008` with zero under-voltage samples. That record ends "board A's supply:
the black 65 W adapter, from now on", which is why the white one was on this board.

Swapping the adapter and its cable as one unit, changing nothing else, and rebooting:

    before   throttled=0x50000   34.3 C
    after    throttled=0x0       34.9 C

**The 27 October finding extends from the Pi 4B to the Pi 3B+**, and it extends across a
different connector: that comparison was USB-C into a 4B, this is micro-USB into a 3B+, so
the cable is not excluded as the part that mattered. Adapter and cable moved together and
were not separated.

## TWO: THE BUZZER IS ACTIVE LOW, AND THE CODE MEANT THE OPPOSITE OF WHAT IT SAID

`wiring.md` had carried "buzzer, net Buzz, polarity to verify on the first sound" since the
sheet was written. The first sound came at about 00:40 and it answered.

Starting the console made the board **begin** to sound, continuously. The console requests
expander line P7 as an output with the value `GPIOD_LINE_VALUE_INACTIVE`, which without
`active_low` is electrically low. **So the buzzer sounds when P7 is low**: it is active low,
and the expander's idle high after reset is why the board had been silent until then.

A prediction written before the test said the opposite, that starting the console would
silence a buzzer left sounding by the idle high. Wrong in direction, right in subject.

What the polarity cost the design, before the fix:

    requested at INACTIVE          meant silent      was sounding, the whole run
    console_actuate ACTIVE 150 ms  meant one sound   was 150 ms of silence
    teardown INACTIVE              meant silent      was sounding

So the buzzer would have sounded for the entire sitting and fallen silent for a sixth of a
second on each transition into LOST, which is the inverse of the rule's one action.

`gpiod_line_settings_set_active_low(ls, true)` is now set on the output path as well as the
input path. One line, rebuilt on the board, and **the console starts silent**. The fix is
established by intervention and not by argument.

## THREE: THE HAT IS CONFIRMED, BY THE CONSOLE'S OWN DISCOVERY LINE

    e08: HAT open: expander /dev/gpiochip2, press on GPIO20, /dev/fb1 128x64 at 32 bpp,
         /sys/bus/iio/devices/iio:device0, /sys/class/rtc/rtc0

and from the pre-flight before it: `i2cdetect -y 1` answers at 0x20, 0x48, 0x68 and 0x76,
three of them `UU` because a driver holds them and 0x48 bare as the chapter expects; the IIO
device is named `bmp280`; the framebuffer is `ssd130xdrmfb`, which is the DRM driver and not
the older `ssd1307fb`; `gpiodetect` lists `gpiochip2 [pcf8574] (8 lines)`.

**The published pre-flight could not have shown that last one.** It ran
`gpioinfo | grep pcf8574`, and libgpiod 2 prints `gpiochipN - N lines:` and the line entries
and never the chip label, so that command returns nothing on a correct board. Corrected to
`gpiodetect` the same day.

## FOUR: A SOCKET FILE IS NOT A RUNNING DAEMON

The console reported `engine unknown, daemon away at start`. The daemon had been started at
00:25 and `/tmp/e08.sock` existed, owned by `bing`, which was taken as proof it was running.
It was not: a unix socket file outlives the process that bound it. **The check must be
`pgrep -f edgellm-e08-daemon`, not `ls` on the socket.** Why the daemon left is not
established; its log was not read before the board was powered off.

## WHAT THIS DOES NOT ESTABLISH

    established   the under-voltage was the supply: 0x50000 to 0x0 on an adapter and
                  cable swap at the same temperature, nothing else changed
    established   the buzzer is active low, by the first sound and by the silence after
                  a one-line fix
    established   every part of the HAT is present and bound: expander, push line,
                  framebuffer, IIO device, RTC
    established   gpioinfo cannot show a chip label under libgpiod 2; gpiodetect can
    established   a socket file is not a running daemon
    NOT CLOSED    clause one, the fifty answers from the joystick. None was asked: the
                  log holds its header and nothing else
    NOT CLOSED    clause three, the recovery after signal 9
    NOT SHOWN     whether the adapter or its cable carried the under-voltage; they moved
                  together
    NOT SHOWN     why the daemon was away. Its log was not read
    OPEN DEFECT   console.c:218, snprintf writing up to 247 bytes into 64, warned by the
                  compiler on this build. It truncates a version string and blocks nothing
    OPEN CHORE    evidence/e08_live.stderr is owned by root, written by a root shell's
                  redirect, and a later run as bing cannot write it
