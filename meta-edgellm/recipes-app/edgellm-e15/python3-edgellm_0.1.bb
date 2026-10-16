SUMMARY = "The shared Python decision loop of edge-llm-pi (common/python/edgellm)"
DESCRIPTION = "The Python mirror of the C core: engine over llama-cpp-python, stub \
engine, replay and serial sources, the rule registry, the logger and the loop. The \
two Python projects, e15 and e20, depend on it."
HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

SRC_URI = "file://common/python/edgellm"

S = "${WORKDIR}/common/python/edgellm"

inherit setuptools3

# The engine is an extra of the package, not a hard dependency, so this recipe parses
# and builds without a llama-cpp-python recipe; edgellm-e15 names that one.
RDEPENDS:${PN} = "python3-pyserial python3-core"
