SUMMARY = "e03, the LED status explainer"
DESCRIPTION = "The rule of project e03 lights one of three LED modules on BCM 17, 27 and \
22 of the Pi 3 Model B from the replayed state stream, and the model writes the one \
sentence that explains the colour. The first project with a live backend: libgpiod 2 \
through the gpio backend, opened by the project's --leds option."

EDGELLM_PROJECT = "e03"
EDGELLM_PROJECT_DIR = "e03-led-explainer"
EDGELLM_MODEL_PACKAGE = "model-smollm2-360m"

# The gpio backend is compiled in, so the bench run can open the three lines; the events
# still come from the replay folder the include installs.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_GPIO=ON"

require recipes-app/edgellm-common.inc

# libgpiod 2 for the backend, at build time and at run time; the include lists only the
# engine. The NanoPi image (kas/edgellm-nanopi.yml) uses this same recipe with the
# model-smollm2-135m package set in its EDGELLM_MODELS and the unit's offsets edited on
# the board, to confirm on the silkscreen.
DEPENDS += "libgpiod"
RDEPENDS:${PN} += "libgpiod"
