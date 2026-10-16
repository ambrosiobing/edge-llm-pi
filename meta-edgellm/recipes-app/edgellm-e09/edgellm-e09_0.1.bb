SUMMARY = "e09, the LCD prompt and verdict display"
DESCRIPTION = "The ADXL345 event loop of e05 in C++ with the 3.5 inch panel showing the \
prompt, the model's grammar output and the rule's verdict as three panes, the touch \
controller choosing which pane is enlarged, and the three LEDs driven by the rule. \
Raspberry Pi 3 Model B Rev 1.2; the panel binds through the bench's bench-lcd35a \
overlay from the Linux volume's project 7 and the sensor through i2c-sensor,adxl345."

EDGELLM_PROJECT = "e09"
EDGELLM_PROJECT_DIR = "e09-lcd-verdict-display"
EDGELLM_MODEL_PACKAGE = "model-smollm2-360m"

# The bench run reads the ADXL345 through the IIO backend and drives the LEDs through
# the gpio backend; both are compiled in here.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_IIO=ON -DEDGELLM_IO_GPIO=ON"

require recipes-app/edgellm-common.inc

# The panel is reached through libdrm (MIT) and the LEDs through libgpiod 2; both are
# found with pkg-config by the project's CMakeLists.txt. Without libdrm at build time
# the binary compiles and refuses everything but --headless, which is not what an image
# for this board wants, so the dependency is stated rather than left to chance.
DEPENDS += "libdrm libgpiod"
RDEPENDS:${PN} += "libdrm libgpiod"
