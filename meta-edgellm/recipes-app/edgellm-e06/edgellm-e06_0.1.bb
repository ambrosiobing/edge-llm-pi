SUMMARY = "e06, the VL53L4CD too close explainer"
DESCRIPTION = "The decision loop over the VL53L4CD single-zone time-of-flight sensor on \
the Pi 3B, through the ST ULD over i2c-dev. The rule decides CLEAR, APPROACHING or \
TOO_CLOSE on every sample; the model proposes the same enum and one sentence."

EDGELLM_PROJECT = "e06"
EDGELLM_PROJECT_DIR = "e06-vl53l4cd-too-close"
EDGELLM_MODEL_PACKAGE = "model-smollm2-360m"

# The ST ULD (STSW-IMG026) is not in this repository and is not fetched by a recipe: its
# download sits behind the licence page on st.com (to verify), so it is unpacked by hand
# on the build host and its folder named in local.conf or the kas file:
#
#   EDGELLM_VL53L4CD_ULD_DIR = "/home/<user>/vl53l4cd-uld"
#
# With the variable empty the package builds the replay path only, and the unit below
# stops at start with the reason printed by edgellm-e06.
EDGELLM_VL53L4CD_ULD_DIR ?= ""
# The sampler reaches the bus through i2c-dev itself; no core backend is compiled in.
EDGELLM_IO_OPTIONS = "-DEDGELLM_VL53L4CD_ULD_DIR=${EDGELLM_VL53L4CD_ULD_DIR}"

require recipes-app/edgellm-common.inc

RDEPENDS:${PN} += "i2c-tools"
