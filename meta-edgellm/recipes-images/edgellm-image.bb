SUMMARY = "Bench image for the edge-llm-pi projects"
DESCRIPTION = "core-image-minimal plus what the bench board needs to run a project as \
a service: libgpiod and i2c-tools for the pre-flight checks, can-utils for the CAN \
projects, an SSH server, llama.cpp with llama-bench and llama-server, the chosen \
models, and the project applications. The bench's own bench-image adds the same \
base tools; this image stands alone so the layer parses without meta-bench."

require recipes-core/images/core-image-minimal.bb

LICENSE = "MIT"

IMAGE_FEATURES += "ssh-server-openssh"

# Root without a password, right for an isolated bench and wrong anywhere else; the
# bench image says the same and so does its README.
IMAGE_FEATURES += "debug-tweaks"

# Which models and which projects go in. A kas file or local.conf overrides both;
# the defaults are the Pi 3 family default model and the Pi 4 enum default, and the
# one project whose recipe exists in this layer today.
EDGELLM_MODELS ?= "model-smollm2-360m model-qwen2.5-0.5b"
EDGELLM_APPS ?= "edgellm-e02"

IMAGE_INSTALL:append = " \
    libgpiod \
    libgpiod-tools \
    i2c-tools \
    can-utils \
    llama-cpp \
    llama-cpp-bench \
    llama-cpp-server \
    ${EDGELLM_MODELS} \
    ${EDGELLM_APPS} \
"

# Room for the models in the root filesystem: the two defaults are about 760 MB
# together. 1.5 GB extra, in kB.
IMAGE_ROOTFS_EXTRA_SPACE = "1572864"

# EXTERNAL MODEL PARTITION, the option, not built on Wednesday 14 October 2026.
#
# Models in the rootfs make every A/B update (the Linux volume, project 19) carry a
# gigabyte that never changes. The alternative is a third wic partition holding
# /usr/share/edgellm/models, built from the same model packages into their own ext4
# and mounted by a unit the applications order After=. The sketch, to be done in a
# later sitting and tested on a card before any chapter claims it:
#
#   1. a recipe edgellm-models-fs that inherits image with IMAGE_INSTALL = the
#      model packages only, IMAGE_FSTYPES = "ext4", PACKAGE_INSTALL stripped of the
#      base system (IMAGE_LINGUAS = "", no packagegroup-core-boot);
#   2. a .wks.in with a third "part /usr/share/edgellm/models --source rootfs
#      --rootfs-dir=${DEPLOY_DIR_IMAGE}/edgellm-models-fs" line after the root
#      partition, selected with WKS_FILE in the kas file;
#   3. EDGELLM_MODELS = "" in this image so the rootfs carries no weights, and a
#      usr-share-edgellm-models.mount unit in edgellm-common.inc's After= and
#      RequiresMountsFor=.
#
# Until that is on a card, the rootfs route above is the one that is built.
