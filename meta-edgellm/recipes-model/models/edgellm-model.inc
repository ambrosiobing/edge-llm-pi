# The shared shape of a model package: one GGUF file fetched from Hugging Face by its
# resolve URL, checked against the SHA256 recorded in models/MODELS.md, installed
# under /usr/share/edgellm/models. Only Apache 2.0 and MIT models have a recipe; Llama
# 3.2 and Gemma are linked in models/MODELS.md and copied to the board by hand, because
# their terms are not the repository's to redistribute.
#
# External model partition, the option the image recipe describes: these packages
# install into the root filesystem by default, which is simplest for a first image.
# To keep the base rootfs small, build the same packages into a second ext4 (a wic
# partition mounted at /usr/share/edgellm/models) instead of IMAGE_INSTALL; see
# recipes-images/edgellm-image.bb for the sketch and its status.

EDGELLM_GGUF ?= ""
EDGELLM_HF_REPO ?= ""

SRC_URI = "https://huggingface.co/${EDGELLM_HF_REPO}/resolve/main/${EDGELLM_GGUF}"

inherit allarch

do_install() {
    install -d ${D}${datadir}/edgellm/models
    install -m 0644 ${WORKDIR}/${EDGELLM_GGUF} ${D}${datadir}/edgellm/models/${EDGELLM_GGUF}
}

FILES:${PN} = "${datadir}/edgellm/models/${EDGELLM_GGUF}"

# A gigabyte of weights is not something to strip, split or inspect for ELF headers.
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INSANE_SKIP:${PN} += "arch file-rdeps"
