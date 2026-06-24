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
    -DKRITA_DEPS_INSTALL_PREFIX=${EXTPREFIX}   # so sub-builds find_package() each other
    -DCMAKE_PREFIX_PATH=${EXTPREFIX}
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF        # iOS is static-only
    -DBUILD_TESTING=OFF
    -DKF_IGNORE_PLATFORM_CHECK=ON  # KDE Frameworks don't list iOS as supported yet
)
# Qt-dependent deps (KF6 frameworks, quazip) need the iOS Qt for libraries and
# the host Qt for moc/rcc/uic. Forward both when the orchestrator provides them.
if(DEFINED QT_IOS_ROOT AND QT_IOS_ROOT)
    list(APPEND KRITA_IOS_EXT_CMAKE_ARGS -DQT_IOS_ROOT=${QT_IOS_ROOT})
endif()
if(DEFINED QT_HOST_PATH AND QT_HOST_PATH)
    list(APPEND KRITA_IOS_EXT_CMAKE_ARGS -DQT_HOST_PATH=${QT_HOST_PATH})
endif()
# Host KDE tools (kconfig_compiler, ...) for cross builds — see KF6ConfigConfig.
if(DEFINED KF6_HOST_TOOLING AND KF6_HOST_TOOLING)
    list(APPEND KRITA_IOS_EXT_CMAKE_ARGS -DKF6_HOST_TOOLING=${KF6_HOST_TOOLING})
endif()

# Helper: declare a CMake-based dependency that installs into EXTPREFIX.
#   krita_ios_ext(<name> URL <url> [DEPENDS ...] [CMAKE_ARGS ...])
function(krita_ios_ext NAME)
    cmake_parse_arguments(E "" "URL;URL_HASH;PATCH" "DEPENDS;CMAKE_ARGS" ${ARGN})
    set(_patch "")
    if(E_PATCH)
        set(_patch ${CMAKE_COMMAND} -P ${E_PATCH})
    endif()
    ExternalProject_Add(${NAME}
        URL          ${E_URL}
        URL_HASH     ${E_URL_HASH}
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
        # Unique name: many tarballs share a basename (e.g. GitHub tag archives
        # all download as v6.5.0.tar.gz) and would otherwise collide.
        DOWNLOAD_NAME ${NAME}.tar.gz
        INSTALL_DIR  ${EXTPREFIX}
        DEPENDS      ${E_DEPENDS}
        PATCH_COMMAND ${_patch}
        CMAKE_ARGS   ${KRITA_IOS_EXT_CMAKE_ARGS} ${E_CMAKE_ARGS}
        BUILD_BYPRODUCTS ${EXTPREFIX}/lib/lib${NAME}.a
    )
endfunction()

# Helper: declare an autotools (./configure) dependency cross-compiled for iOS.
# Many of Krita's deps (lcms2, fribidi, ...) ship autotools, not CMake.
#   krita_ios_ext_autotools(<name> URL <url> [DEPENDS ...] [CONFIGURE_ARGS ...])
function(krita_ios_ext_autotools NAME)
    cmake_parse_arguments(E "" "URL;URL_HASH" "DEPENDS;CONFIGURE_ARGS" ${ARGN})
    if(PLATFORM STREQUAL "SIMULATORARM64")
        set(_sdk iphonesimulator)
        set(_minflag "-mios-simulator-version-min=${DEPLOYMENT_TARGET}")
    else()
        set(_sdk iphoneos)
        set(_minflag "-miphoneos-version-min=${DEPLOYMENT_TARGET}")
    endif()
    execute_process(COMMAND xcrun --sdk ${_sdk} --show-sdk-path
        OUTPUT_VARIABLE _sysroot OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND xcrun --sdk ${_sdk} --find clang
        OUTPUT_VARIABLE _cc OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(_flags "-arch arm64 -isysroot ${_sysroot} ${_minflag}")
    ExternalProject_Add(${NAME}
        URL          ${E_URL}
        URL_HASH     ${E_URL_HASH}
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
        DOWNLOAD_NAME ${NAME}.tar.gz
        DEPENDS      ${E_DEPENDS}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            "PKG_CONFIG_PATH=${EXTPREFIX}/lib/pkgconfig"
            <SOURCE_DIR>/configure
            --prefix=${EXTPREFIX}
            --host=arm-apple-darwin
            --disable-shared --enable-static
            "CC=${_cc}"
            "CFLAGS=${_flags} -I${EXTPREFIX}/include"
            "LDFLAGS=${_flags} -L${EXTPREFIX}/lib"
            ${E_CONFIGURE_ARGS}
        BUILD_COMMAND make -j4
        INSTALL_COMMAND make install
    )
endfunction()

# ---------------------------------------------------------------------------
# Representative foundational packages. Pin versions/URLs against
# krita-deps-management; the values below are reasonable starting points and
# MUST be verified (and hashes added) before a real build.
# ---------------------------------------------------------------------------

# Extra CMake Modules — KDE's CMake helpers, required by Krita and every KF
# framework. Arch-independent (installs .cmake modules only).
# iOS support in ECM/KF only exists in unreleased master (the released 6.x
# tags lack the iOS guards, e.g. kwidgetsaddons' QProcess/kmimetypeeditor).
# Master requires Qt >= 6.9.
krita_ios_ext(ext_ecm
    URL https://github.com/KDE/extra-cmake-modules/archive/refs/heads/master.tar.gz
    CMAKE_ARGS -DBUILD_TESTING=OFF
)

# Header-only: trivial to cross-compile.
set(EIGEN3_VERSION "3.4.0" CACHE STRING "")
krita_ios_ext(ext_eigen3
    URL https://gitlab.com/libeigen/eigen/-/archive/${EIGEN3_VERSION}/eigen-${EIGEN3_VERSION}.tar.gz
    CMAKE_ARGS -DEIGEN_BUILD_DOC=OFF -DBUILD_TESTING=OFF
)

# zlib — base of the image-codec chain. Use the GitHub release (permanent);
# zlib.net moves older releases into a fossils/ subdir and 404s the top URL.
set(ZLIB_VERSION "1.3.1" CACHE STRING "")
krita_ios_ext(ext_zlib
    URL https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz
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

# Little CMS — color management. Autotools build (the tarball has no
# CMakeLists.txt); skip the sample tools that would need jpeg/tiff.
set(LCMS2_VERSION "2.16" CACHE STRING "")
krita_ios_ext_autotools(ext_lcms2
    URL https://github.com/mm2/Little-CMS/releases/download/lcms${LCMS2_VERSION}/lcms2-${LCMS2_VERSION}.tar.gz
    CONFIGURE_ARGS --without-jpeg --without-tiff
)

# Stub libintl — iOS has no gettext, and KI18n's find_package(LibIntl) is
# REQUIRED. Built from the local proxy-libintl/ sources (no download).
ExternalProject_Add(ext_proxy_libintl
    SOURCE_DIR       ${CMAKE_CURRENT_LIST_DIR}/proxy-libintl
    DOWNLOAD_COMMAND ""
    INSTALL_DIR      ${EXTPREFIX}
    CMAKE_ARGS       ${KRITA_IOS_EXT_CMAKE_ARGS}
    BUILD_BYPRODUCTS ${EXTPREFIX}/lib/libintl.a
)

# --- KDE Frameworks 6 (CMake; need ECM + Qt6 + host Qt tools) --------------
# Krita requires: Config WidgetsAddons Completion CoreAddons GuiAddons I18n
# ItemViews. Built in dependency order from the KDE GitHub mirror tags.
set(KF6_VERSION "6.10.0" CACHE STRING "")

krita_ios_ext(ext_kcoreaddons
    URL https://github.com/KDE/kcoreaddons/archive/refs/heads/master.tar.gz
    DEPENDS ext_ecm
    CMAKE_ARGS -DKCOREADDONS_USE_QML=OFF)   # QML plugin uses QProcess (no iOS)
krita_ios_ext(ext_kconfig
    URL https://github.com/KDE/kconfig/archive/refs/heads/master.tar.gz
    DEPENDS ext_kcoreaddons
    CMAKE_ARGS -DKCONFIG_USE_QML=OFF        # Krita uses the C++ libs, not QML
    PATCH ${CMAKE_CURRENT_LIST_DIR}/patches/patch-kconfig.cmake)  # use host kconfig_compiler
krita_ios_ext(ext_kguiaddons
    URL https://github.com/KDE/kguiaddons/archive/refs/heads/master.tar.gz
    DEPENDS ext_ecm)
krita_ios_ext(ext_kwidgetsaddons
    URL https://github.com/KDE/kwidgetsaddons/archive/refs/heads/master.tar.gz
    DEPENDS ext_ecm)
krita_ios_ext(ext_kitemviews
    URL https://github.com/KDE/kitemviews/archive/refs/heads/master.tar.gz
    DEPENDS ext_kwidgetsaddons)
krita_ios_ext(ext_ki18n
    URL https://github.com/KDE/ki18n/archive/refs/heads/master.tar.gz
    DEPENDS ext_ecm ext_proxy_libintl)
krita_ios_ext(ext_kcodecs
    URL https://github.com/KDE/kcodecs/archive/refs/heads/master.tar.gz
    DEPENDS ext_ecm)
krita_ios_ext(ext_kcompletion
    URL https://github.com/KDE/kcompletion/archive/refs/heads/master.tar.gz
    DEPENDS ext_kconfig ext_kwidgetsaddons ext_kcoreaddons ext_kcodecs)
krita_ios_ext(ext_kcolorscheme
    URL https://github.com/KDE/kcolorscheme/archive/refs/heads/master.tar.gz
    DEPENDS ext_kconfig ext_kguiaddons ext_kcoreaddons ext_ki18n)

# --- Other native dependencies (non-KDE) -----------------------------------
# Boost: Krita's find_package(Boost) needs only the headers; install them
# directly (no b2 bootstrap). Build compiled Boost libs later if a link needs them.
set(BOOST_VERSION "1.86.0" CACHE STRING "")
string(REPLACE "." "_" _boost_us "${BOOST_VERSION}")
ExternalProject_Add(ext_boost
    URL https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${_boost_us}.tar.gz
    DOWNLOAD_NAME ext_boost.tar.gz
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/boost ${EXTPREFIX}/include/boost
)

# Immer / Zug / Lager — header-only C++ (CMake); Krita REQUIRES all three.
krita_ios_ext(ext_immer
    URL https://github.com/arximboldi/immer/archive/refs/tags/v0.8.1.tar.gz
    CMAKE_ARGS -Dimmer_BUILD_TESTS=OFF -Dimmer_BUILD_EXAMPLES=OFF -Dimmer_BUILD_DOCS=OFF -Dimmer_BUILD_EXTRAS=OFF)
krita_ios_ext(ext_zug
    URL https://github.com/arximboldi/zug/archive/refs/heads/master.tar.gz
    CMAKE_ARGS -Dzug_BUILD_TESTS=OFF -Dzug_BUILD_EXAMPLES=OFF)
krita_ios_ext(ext_lager
    URL https://github.com/arximboldi/lager/archive/refs/heads/master.tar.gz
    DEPENDS ext_immer ext_zug
    CMAKE_ARGS -Dlager_BUILD_TESTS=OFF -Dlager_BUILD_EXAMPLES=OFF -Dlager_BUILD_DOCS=OFF)

# expat — needed by exiv2 (XMP) and fontconfig.
set(EXPAT_VERSION "2.6.4" CACHE STRING "")
string(REPLACE "." "_" _expat_us "${EXPAT_VERSION}")
krita_ios_ext(ext_expat
    URL https://github.com/libexpat/libexpat/releases/download/R_${_expat_us}/expat-${EXPAT_VERSION}.tar.gz
    CMAKE_ARGS -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_DOCS=OFF -DEXPAT_SHARED_LIBS=OFF)

# FreeType without HarfBuzz (breaks the cycle), then HarfBuzz with FreeType.
krita_ios_ext(ext_freetype
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz
    DEPENDS ext_png ext_zlib
    CMAKE_ARGS -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_BZIP2=ON)
krita_ios_ext(ext_harfbuzz
    URL https://github.com/harfbuzz/harfbuzz/archive/refs/tags/10.2.0.tar.gz
    DEPENDS ext_freetype
    CMAKE_ARGS -DHB_HAVE_FREETYPE=ON -DHB_BUILD_TESTS=OFF -DHB_BUILD_UTILS=OFF -DHB_HAVE_GLIB=OFF -DHB_HAVE_ICU=OFF -DHB_HAVE_CORETEXT=ON)

# Exiv2 — Krita REQUIRES LibExiv2.
krita_ios_ext(ext_exiv2
    URL https://github.com/Exiv2/exiv2/archive/refs/tags/v0.28.5.tar.gz
    DEPENDS ext_expat ext_zlib
    CMAKE_ARGS -DEXIV2_BUILD_SAMPLES=OFF -DEXIV2_BUILD_EXIV2_COMMAND=OFF -DEXIV2_ENABLE_BMFF=OFF -DEXIV2_BUILD_UNIT_TESTS=OFF -DEXIV2_ENABLE_INIH=OFF)

# libtiff — made REQUIRED by Krita's CheckLibTIFFPSDSupport module.
krita_ios_ext(ext_tiff
    URL https://download.osgeo.org/libtiff/tiff-4.7.0.tar.gz
    DEPENDS ext_jpeg ext_zlib
    CMAKE_ARGS -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-docs=OFF -Dtiff-contrib=OFF
               -Djbig=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF -Dlerc=OFF)

# Fontconfig + libunibreak — the last REQUIRED deps (autotools).
krita_ios_ext_autotools(ext_libunibreak
    URL https://github.com/adah1972/libunibreak/releases/download/libunibreak_6_1/libunibreak-6.1.tar.gz)
krita_ios_ext_autotools(ext_fontconfig
    URL https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.15.0.tar.gz
    DEPENDS ext_freetype ext_expat
    CONFIGURE_ARGS --disable-docs --enable-libxml2=no)

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
