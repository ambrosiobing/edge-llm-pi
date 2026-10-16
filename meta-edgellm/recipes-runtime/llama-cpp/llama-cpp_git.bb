SUMMARY = "llama.cpp at the tag the volume pins"
DESCRIPTION = "The inference engine every project uses: libllama and the ggml \
libraries, plus llama-bench (the instrument e01 is priced against) and llama-server \
(the HTTP integration e04 talks to). One tag for the whole volume, recorded in \
cmake/llama_tag.cmake and models/MODELS.md; a chapter never quotes a number from \
another build."
HOMEPAGE = "https://github.com/ggml-org/llama.cpp"

LICENSE = "MIT"
# The md5 of the upstream LICENSE file was not computed on the authoring laptop. The
# first BitBake run reports the mismatch together with the real value; paste that
# value here and keep it, which is how every other recipe got its checksum too.
LIC_FILES_CHKSUM = "file://LICENSE;md5=00000000000000000000000000000000"

SRC_URI = "git://github.com/ggml-org/llama.cpp;protocol=https;branch=master"

# Tag v0.6.0 (Sunday 5 October 2026), the commit the annotated tag points at, read
# from the GitHub API on Wednesday 14 October 2026. Confirm on the build host with
#   git -C third_party/llama.cpp rev-parse v0.6.0^{commit}
# before the first image build; the tag object itself is 69ecae92.
SRCREV = "8345f333951c661d166b00e6f9362e553768f292"
PV = "0.6.0+git${SRCPV}"

S = "${WORKDIR}/git"

inherit cmake pkgconfig

# GGML_NATIVE=OFF: the build runs on the host, so -march=native would describe the
# wrong CPU. The target flags come from the machine tune (TUNE_CCARGS) that Yocto
# already passes to the compiler; GGML_CPU_ARM_ARCH names the ISA the ggml kernels
# are selected for, per machine below. Both Pi families are Cortex-A53 or A72,
# armv8-a with CRC. The NanoPi (Cortex-A7, armv7) keeps the tune's flags alone; the
# string ggml expects for 32-bit ARM is to verify on the first NanoPi build.
EXTRA_OECMAKE = "\
    -DGGML_NATIVE=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DLLAMA_BUILD_COMMON=ON \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_TOOLS=ON \
    -DLLAMA_BUILD_SERVER=ON \
    -DLLAMA_TOOLS_INSTALL=ON \
    -DLLAMA_CURL=OFF \
    -DLLAMA_OPENSSL=OFF \
"
EXTRA_OECMAKE:append:raspberrypi4-64 = " -DGGML_CPU_ARM_ARCH=armv8-a+crc"
EXTRA_OECMAKE:append:raspberrypi3-64 = " -DGGML_CPU_ARM_ARCH=armv8-a+crc"

PACKAGES =+ "${PN}-bench ${PN}-server"

FILES:${PN}-bench = "${bindir}/llama-bench"
FILES:${PN}-server = "${bindir}/llama-server"

# llama.cpp installs unversioned .so files, which Yocto would otherwise sort into the
# -dev package and leave the runtime package empty.
FILES:${PN} += "${libdir}/*.so"
INSANE_SKIP:${PN} += "dev-so"
FILES:${PN}-dev = "${includedir} ${libdir}/cmake ${libdir}/pkgconfig"

RDEPENDS:${PN}-bench = "${PN}"
RDEPENDS:${PN}-server = "${PN}"
