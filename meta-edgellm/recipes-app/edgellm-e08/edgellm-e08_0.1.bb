SUMMARY = "e08, the Explorer700 console with a model: console plus inference daemon"
DESCRIPTION = "Catalogue variant O on the Pi 3 Model B Plus with the JOY-iT RB-Explorer700 \
seated: edgellm-e08-console owns the HAT (joystick through libgpiod 2, DS3231 and BMP280 \
through sysfs, the SSD1306 through its framebuffer, the buzzer on the rule) and asks \
edgellm-e08-daemon, which owns the model behind a Unix domain socket, for one sentence \
per question. Two binaries, two units; the daemon unit restarts on failure."

EDGELLM_PROJECT = "e08"
EDGELLM_PROJECT_DIR = "e08-explorer700-console"
EDGELLM_MODEL_PACKAGE = "model-smollm2-360m"

# The live half of the console needs the libgpiod 2 backend compiled in.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_GPIO=ON"

require recipes-app/edgellm-common.inc

DEPENDS += "libgpiod"
RDEPENDS:${PN} += "libgpiod"

# The shared include installs one binary and one unit named edgellm-e08.service from
# the recipe's files/. This project has two of each, kept beside the sources in
# projects/e08-explorer700-console/files so that the chapter prints the same bytes the
# image ships; the include's unit entry is dropped from SRC_URI and do_install is the
# project's own.
SRC_URI:remove = "file://edgellm-${EDGELLM_PROJECT}.service"

do_install() {
    install -Dm0755 ${B}/projects/${EDGELLM_PROJECT_DIR}/edgellm-e08-console \
        ${D}${bindir}/edgellm-e08-console
    install -Dm0755 ${B}/projects/${EDGELLM_PROJECT_DIR}/edgellm-e08-daemon \
        ${D}${bindir}/edgellm-e08-daemon
    install -d ${D}${datadir}/edgellm/e08
    install -m0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/prompt.txt \
        ${D}${datadir}/edgellm/e08/prompt.txt
    install -m0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/grammar.gbnf \
        ${D}${datadir}/edgellm/e08/grammar.gbnf
    install -d ${D}${datadir}/edgellm/e08/replay
    install -m0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/replay/*.csv \
        ${D}${datadir}/edgellm/e08/replay/
    install -Dm0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/files/edgellm-e08-daemon.service \
        ${D}${systemd_system_unitdir}/edgellm-e08-daemon.service
    install -Dm0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/files/edgellm-e08-console.service \
        ${D}${systemd_system_unitdir}/edgellm-e08-console.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-e08-daemon.service edgellm-e08-console.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} = "\
    ${bindir}/edgellm-e08-console \
    ${bindir}/edgellm-e08-daemon \
    ${datadir}/edgellm/e08 \
    ${systemd_system_unitdir}/edgellm-e08-daemon.service \
    ${systemd_system_unitdir}/edgellm-e08-console.service \
"

# The overlays the console expects are the four Raspberry Pi OS ones, not the Linux
# volume's bench-explorer700 overlay: that one gives the joystick to gpio-keys-polled and
# the buzzer to gpio-beeper, and a line a kernel consumer holds is not available to this
# console's gpiod requests. The kas file for the Pi 3B+ carries these in RPI_EXTRA_CONFIG
# when e08 is added to EDGELLM_APPS, which is not done here; the lines are also in
# projects/e08-explorer700-console/wiring.md:
#
#   dtparam=i2c_arm=on
#   dtparam=spi=on
#   dtoverlay=i2c-rtc,ds3231
#   dtoverlay=i2c-sensor,bmp280,addr=0x76
#   dtoverlay=pcf857x,pcf8574,addr=0x20
#   dtoverlay=ssd1306-spi,dc_pin=16,reset_pin=19
