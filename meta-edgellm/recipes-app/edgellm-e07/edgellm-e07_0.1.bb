SUMMARY = "e07, the IKS4A1 climate and motion narrator"
DESCRIPTION = "C++17 over the C API: six IIO devices of the X-NUCLEO-IKS4A1 and the \
sht4x hwmon device read every 60 s on the Pi 3 Model B Plus, the pressure trend by \
rule, one three-sentence summary an hour by the model, into one log."

EDGELLM_PROJECT = "e07"
EDGELLM_PROJECT_DIR = "e07-iks4a1-narrator"
# The 0.5B model is the Pi 3 upper rung; it is admitted only when e01's MemAvailable
# reading on the 3B+ is at least 818 MB. Below that, set model-smollm2-360m here and
# in the unit's --model path, with no other change.
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# The bench run reads sysfs through the iio backend of io.h; the sht4x attributes go
# through the sysfs backend, which is always compiled in.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_IIO=ON"

require recipes-app/edgellm-common.inc

# The shield's device tree overlay is the Linux volume's project 10 overlay,
# bench-iks4a1.dtbo from meta-bench (recipes-bench/bench-iks4a1), applied with
# dtoverlay=bench-iks4a1 in config.txt by the kas file. It is not duplicated here.
RDEPENDS:${PN} += "bench-iks4a1"
