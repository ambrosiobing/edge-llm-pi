SUMMARY = "e17, the cellular status summariser"
DESCRIPTION = "The rule of project e17 reads one status sample of the SIM7070G HAT, the \
signal quality from AT+CSQ and the registration state from AT+CEREG?, and calls the link \
OK, DEGRADED or DOWN; the model writes three sentences about the same fields every fifteen \
minutes. Two units: the sampler, which drives the AT dialogue through the termios backend \
and writes one sample a minute, and the loop, which reads that file and decides."

EDGELLM_PROJECT = "e17"
EDGELLM_PROJECT_DIR = "e17-cellular-summariser"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# The serial backend is compiled in, so the sampler can open the modem's port; the loop
# itself reads the sample file the sampler wrote, or the replay file the include installs.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_SERIAL=ON"

require recipes-app/edgellm-common.inc

# The second unit, beside the one the include installs. The sampler is a long running
# service and the loop is a oneshot over the file it produced, because a replay source
# ends at end of file: a growing file is not a live source, and the chapter says so.
SRC_URI += "file://edgellm-e17-sampler.service"

do_install:append() {
    install -Dm0644 ${WORKDIR}/edgellm-e17-sampler.service \
        ${D}${systemd_system_unitdir}/edgellm-e17-sampler.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-e17-sampler.service edgellm-e17.service"
FILES:${PN} += "${systemd_system_unitdir}/edgellm-e17-sampler.service"

# Neither unit is enabled at boot, which is the one place this recipe departs from its
# siblings. A sampling day starts deliberately: the antennas are fitted, the SIM is in, the
# 0 ohm logic voltage resistor of the HAT has been read on the board, and AT+CMNB has locked
# the radio access technology. A unit that started the modem at boot would skip all four.
SYSTEMD_AUTO_ENABLE = "disable"

# picocom for the first AT session by hand, which the wiring page asks for before the
# sampler is started. GPLv2; it is in meta-oe.
RDEPENDS:${PN} += "picocom"
