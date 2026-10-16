SUMMARY = "e16, the NB-IoT sleep interval from a measured current profile"
DESCRIPTION = "The rule of project e16 picks the largest of four sleep intervals whose \
battery life, computed by a deterministic C function from a logged PPK2 profile, meets \
a stated target and fits inside the power-saving-mode window the network granted; the \
model proposes among the same four and writes one sentence. Nothing is decided live: \
an attach and a PSM cycle take minutes, so the service replays the scenario file."

EDGELLM_PROJECT = "e16"
EDGELLM_PROJECT_DIR = "e16-nbiot-sleep-interval"
EDGELLM_MODEL_PACKAGE = "model-smollm2-360m"

# The termios backend is compiled in so that the image can run the AT bring-up of
# wiring.md with --at on /dev/ttyAMA0. The scenarios themselves always come from the
# replay folder the shared include installs; the serial port is never the event source.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_SERIAL=ON"

require recipes-app/edgellm-common.inc

# The overlay this project needs is not a device tree fragment of its own: it is the
# Raspberry Pi's own disable-bt, which moves the PL011 off the Bluetooth controller so
# that /dev/ttyAMA0 is the module's port. On a bench image that line belongs in the
# machine configuration, and the kas file for this board sets it; the recipe states it
# here so the dependency is not lost between the two files.
#
#   ENABLE_UART = "1"
#   RPI_EXTRA_CONFIG += "dtoverlay=disable-bt"
#
# The serial getty must not be enabled on ttyAMA0 in the same image, or the login shell
# consumes the module's replies and the port answers nothing the AT plan expects.
SERIAL_CONSOLES = ""

# The module's own supply does not come from this image, and neither does the
# instrument: the PPK2 is on the host that runs tools/ppk2_capture.py, and the capture
# that produced the scenario file was taken before the image was built.
