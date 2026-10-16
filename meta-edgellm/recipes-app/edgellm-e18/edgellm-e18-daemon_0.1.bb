SUMMARY = "e18 inference daemon and the C ABI library behind it"
DESCRIPTION = "The C half of project e18: libedgellm_abi.so, which carries the four \
declarations of projects/e18-industrial-logger/c/abi.h, and the daemon that serves \
them over a Unix domain socket. Installed, versioned and upgraded on its own, with \
the Rust device service of edgellm-e18 untouched: that separation is the project."

# This recipe does NOT require recipes-app/edgellm-common.inc. That include builds one
# project binary from the repository's top level CMakeLists.txt with a grammar, a
# replay folder and one unit; e18's C half is its own CMake project two levels down
# (projects/e18-industrial-logger/c), builds a shared library rather than a project
# binary, and shares its grammar and prompt with the Rust service rather than owning
# them. Saying that here is cheaper than bending the include into a shape that would
# fit two languages.

HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

# The sources are the tree this layer sits in, not a remote: the recipe builds exactly
# the checked out revision with no network and no second clone. common/c carries the
# decision core the ABI library compiles in; grammars/ carries the GBNF.
SRC_URI = "\
    file://cmake \
    file://common/c \
    file://grammars \
    file://projects/e18-industrial-logger \
"

S = "${WORKDIR}"
OECMAKE_SOURCEPATH = "${S}/projects/e18-industrial-logger/c"
B = "${WORKDIR}/build"

DEPENDS = "llama-cpp"
RDEPENDS:${PN} = "llama-cpp model-qwen2.5-0.5b"

inherit cmake pkgconfig systemd

# The installed libllama from the llama-cpp recipe, found in the sysroot, and never
# the submodule: the project's CMakeLists.txt takes both paths as cache variables so
# that find_library does not have to guess inside a cross build.
EXTRA_OECMAKE = "\
    -DEDGELLM_STUB_MODEL=OFF \
    -DEDGELLM_LLAMA_INCLUDE_DIR=${STAGING_INCDIR} \
    -DEDGELLM_LLAMA_LIBRARY=${STAGING_LIBDIR}/libllama.so \
"

do_install:append() {
    install -d ${D}${datadir}/edgellm/e18
    install -m0644 ${S}/projects/e18-industrial-logger/grammar.gbnf \
        ${D}${datadir}/edgellm/e18/grammar.gbnf
    install -m0644 ${S}/projects/e18-industrial-logger/prompt.txt \
        ${D}${datadir}/edgellm/e18/prompt.txt
    install -Dm0644 \
        ${S}/projects/e18-industrial-logger/files/edgellm-e18-daemon.service \
        ${D}${systemd_system_unitdir}/edgellm-e18-daemon.service
}

# The daemon and the service meet on one group and neither is root. The user exists for
# both packages; this one creates it because it starts first.
inherit useradd
USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "--system --no-create-home --shell /sbin/nologin edgellm"
GROUPADD_PARAM:${PN} = "--system edgellm"

SYSTEMD_SERVICE:${PN} = "edgellm-e18-daemon.service"
SYSTEMD_AUTO_ENABLE = "enable"

# The shared library's soname is libedgellm_abi.so.1, from the ABI's major version in
# abi.h. A major bump renames the file, so a service linked against the old major keeps
# the old file rather than loading an incompatible one, and the version check in
# edgellm_abi_version() catches a minor replacement. The unversioned symlink and the
# header go to the development package, because only a build needs them.
FILES:${PN} = "\
    ${bindir}/edgellm-e18-daemon \
    ${libdir}/libedgellm_abi.so.* \
    ${datadir}/edgellm/e18 \
    ${systemd_system_unitdir}/edgellm-e18-daemon.service \
"
FILES:${PN}-dev = "${libdir}/libedgellm_abi.so ${includedir}/edgellm-e18"
