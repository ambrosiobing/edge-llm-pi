SUMMARY = "e04, the Pi thermal and throttle narrator over llama-server"
DESCRIPTION = "A C client of llama-server on the loopback interface: the SoC temperature \
from the thermal zone and the throttle word from vcgencmd, a two-line rule, one \
grammar-bounded narration per minute. Two units: the server that holds the model and the \
client that holds the loop, ordered After= and Requires=."

EDGELLM_PROJECT = "e04"
EDGELLM_PROJECT_DIR = "e04-thermal-narrator"
EDGELLM_MODEL_PACKAGE = "model-smollm2-360m"

# The live source is the sysfs backend, which is always compiled in; vcgencmd is run
# through popen by the client itself. No optional backend.
EDGELLM_IO_OPTIONS = ""

# Both units live in the project folder, beside the code they start, so the recipe
# adds that folder to the search path before the shared include adds the repository
# root. The include's SRC_URI names the client unit; the server unit is added below.
FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}/projects/e04-thermal-narrator/files:"

require recipes-app/edgellm-common.inc

SRC_URI += "file://edgellm-llama-server.service"

# libcurl at build time (the curl licence) and llama-server at run time. The client
# never links the engine library. vcgencmd is also needed at run time; which
# meta-raspberrypi package provides it on raspberrypi4-64 (userland, or rpi-utils) is
# to verify on the first image build, and the dependency is added then rather than
# guessed here.
DEPENDS += "curl"
RDEPENDS:${PN} += "llama-cpp-server"

do_install:append() {
    install -Dm0644 ${WORKDIR}/edgellm-llama-server.service \
        ${D}${systemd_system_unitdir}/edgellm-llama-server.service
}

SYSTEMD_SERVICE:${PN} += "edgellm-llama-server.service"
FILES:${PN} += "${systemd_system_unitdir}/edgellm-llama-server.service"
