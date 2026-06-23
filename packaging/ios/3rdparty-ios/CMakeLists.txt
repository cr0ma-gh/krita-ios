# SPDX-License-Identifier: GPL-2.0-or-later
#
# iOS dependency superbuild for Krita (Phase 0).
#
# Cross-compiles Krita's native dependencies for a single iOS platform
# (PLATFORM=OS64 or SIMULATORARM64) into ${EXTPREFIX}. Run it once per platform;
# packaging/ios/build-deps.sh then merges the two prefixes into .xcframeworks
# that the main Krita build consumes.
#
# This is a *starting* superbuild: it provides the correct ExternalProject
# plumbing (toolchain forwarding, build ordering, install prefix) plus a few
# representative, foundational packages. The full inventory — and which exact
# versions/patches to pin — lives in packaging/ios/dependencies.md and, upstream,
# in https://invent.kde.org/packaging/krita-deps-management . Align versions with
# that repo rather than trusting the defaults below.
#
# Usage (on macOS, driven by build-deps.sh):
#   cmake -S packaging/ios/3rdparty-ios -B <build> -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/modules/ios.toolchain.cmake \
#         -DPLATFORM=OS64 -DDEPLOYMENT_TARGET=16.0 \
#         -DEXTPREFIX=<prefix-for-this-platform>
#   cmake --build <build>

cmake_minimum_required(VERSION 3.19)
project(krita-ios-deps NONE)

# Use the extracted-file timestamps from "now" on URL downloads.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(ExternalProject)

if(NOT DEFINED EXTPREFIX)
    set(EXTPREFIX "${CMAKE_BINARY_DIR}/i")
endif()
file(MAKE_DIRECTORY "${EXTPREFIX}")
message(STATUS "Krita iOS deps: PLATFORM=${PLATFORM} EXTPREFIX=${EXTPREFIX}")

# Arguments forwarded to every CMake-based sub-build so each dependency is
# cross-compiled with the same iOS toolchain and lands in the shared prefix.
set(KRITA_IOS_EXT_CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DPLATFORM=${PLATFORM}
    -DDEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}
    -DCMAKE_INSTALL_PREFIX=${EXTPREFIX}
    -DCMAKE_PREFIX_PATH=${EXTPREFIX}
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF        # iOS is static-only
    -DBUILD_TESTING=OFF
)

# Helper: declare a CMake-based dependency that installs into EXTPREFIX.
#   krita_ios_ext(<name> URL <url> [DEPENDS ...] [CMAKE_ARGS ...])
function(krita_ios_ext NAME)
    cmake_parse_arguments(E "" "URL;URL_HASH" "DEPENDS;CMAKE_ARGS" ${ARGN})
    ExternalProject_Add(${NAME}
        URL          ${E_URL}
        URL_HASH     ${E_URL_HASH}
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
        INSTALL_DIR  ${EXTPREFIX}
        DEPENDS      ${E_DEPENDS}
        CMAKE_ARGS   ${KRITA_IOS_EXT_CMAKE_ARGS} ${E_CMAKE_ARGS}
        BUILD_BYPRODUCTS ${EXTPREFIX}/lib/lib${NAME}.a
    )
endfunction()

# ---------------------------------------------------------------------------
# Representative foundational packages. Pin versions/URLs against
# krita-deps-management; the values below are reasonable starting points and
# MUST be verified (and hashes added) before a real build.
# ---------------------------------------------------------------------------

# Header-only: trivial to cross-compile.
set(EIGEN3_VERSION "3.4.0" CACHE STRING "")
krita_ios_ext(ext_eigen3
    URL https://gitlab.com/libeigen/eigen/-/archive/${EIGEN3_VERSION}/eigen-${EIGEN3_VERSION}.tar.gz
    CMAKE_ARGS -DEIGEN_BUILD_DOC=OFF -DBUILD_TESTING=OFF
)

# zlib — base of the image-codec chain.
set(ZLIB_VERSION "1.3.1" CACHE STRING "")
krita_ios_ext(ext_zlib
    URL https://zlib.net/zlib-${ZLIB_VERSION}.tar.gz
)

# libjpeg-turbo — needs nothing; tiff/heif/etc. depend on it.
set(JPEGTURBO_VERSION "3.0.4" CACHE STRING "")
krita_ios_ext(ext_jpeg
    URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${JPEGTURBO_VERSION}/libjpeg-turbo-${JPEGTURBO_VERSION}.tar.gz
    CMAKE_ARGS -DENABLE_SHARED=OFF -DWITH_SIMD=ON
)

# libpng — depends on zlib.
set(PNG_VERSION "1.6.43" CACHE STRING "")
krita_ios_ext(ext_png
    URL https://download.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.gz
    DEPENDS ext_zlib
    CMAKE_ARGS -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF
)

# Little CMS — color management; needed by Krita core.
set(LCMS2_VERSION "2.16" CACHE STRING "")
krita_ios_ext(ext_lcms2
    URL https://github.com/mm2/Little-CMS/releases/download/lcms${LCMS2_VERSION}/lcms2-${LCMS2_VERSION}.tar.gz
)

# ---------------------------------------------------------------------------
# TODO (see dependencies.md for the full DAG, build systems and drop list):
#   image:   ext_tiff ext_giflib ext_webp ext_openjpeg ext_heif ext_jpegxl
#            ext_openexr(+Imath)
#   color:   ext_ocio(+expat,yaml-cpp,pystring,minizip)
#   text:    ext_freetype ext_fontconfig ext_harfbuzz ext_fribidi ext_unibreak
#   math:    ext_gsl ext_fftw3 ext_xsimd ext_boost
#   krita:   ext_mypaint(+json-c) ext_quazip ext_seexpr ext_exiv2 ext_libraw
#            ext_immer ext_lager ext_zug
#   kde:     ext_kconfig ext_kcoreaddons ext_kwidgetsaddons ext_kcompletion
#            ext_kguiaddons ext_ki18n ext_kitemviews ext_kitemmodels
#            (build AFTER patched Qt-for-iOS)
#   DROPPED on iOS: python sip pyqt poppler mlt sdl2 unwindstack x11 wayland
# ---------------------------------------------------------------------------
