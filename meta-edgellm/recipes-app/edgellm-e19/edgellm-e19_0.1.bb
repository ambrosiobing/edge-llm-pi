SUMMARY = "e19, the event driven cellular alert composer"
DESCRIPTION = "The rule of project e19 classifies an ADXL345 event and decides whether \
the Raspberry Pi 3 Model B Plus may send anything at all; on a TAP or a FREEFALL, and \
no more often than its minimum interval, the model composes one message of at most 160 \
characters under a grammar, and a field checker validates that message against the \
event before the single call to the send path hands it to the SIM7070G. A message that \
is missing a field, or that carries a number the event never had, is refused and logged."

EDGELLM_PROJECT = "e19"
EDGELLM_PROJECT_DIR = "e19-cellular-alert"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# Two backends: iio for the accelerometer through the binding of the Linux volume's
# project 5, and serial so the image carries the termios backend the other cellular
# projects use. The modem's AT port is opened by the project itself.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_IIO=ON -DEDGELLM_IO_SERIAL=ON"

require recipes-app/edgellm-common.inc

# The alert ledger of a live run goes beside the event log under the unit's state
# directory. i2c-tools and the kernel modules are in the image already for the Linux
# volume's project 5 overlay; nothing is added here for the sensor.
RDEPENDS:${PN} += "i2c-tools"
