# SPDX-License-Identifier: GPL-2.0-or-later
#
# Host (native macOS) build of the KDE Framework code-generation tools needed at
# build time (kconfig_compiler, desktoptojson, ...). These cannot be cross-built
# for iOS — they must run on the host. Installed into the SAME prefix as the
# cross deps so the cross frameworks' *Config.cmake find the host tool targets
# (e.g. KF6ConfigCompilerTargets.cmake) right next to the iOS libraries.
#
# Driven by build-deps.sh BEFORE the iOS superbuild. No iOS toolchain here.
#
#   cmake -S packaging/ios/host-tools -B <b> -G Ninja \
#         -DEXTPREFIX=<shared-prefix> -DKF_REF=<git-ref>
#   cmake --build <b>

cmake_minimum_required(VERSION 3.19)
project(krita-ios-host-tools NONE)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
include(ExternalProject)

if(NOT DEFINED EXTPREFIX)
    message(FATAL_ERROR "EXTPREFIX is required")
endif()
if(NOT DEFINED KF_REF)
    set(KF_REF "refs/heads/master")
endif()

# Native build args. QT_HOST_PATH (the desktop-macOS Qt) provides Qt6 for the host.
set(_host_args
    -DCMAKE_INSTALL_PREFIX=${EXTPREFIX}
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_TESTING=OFF
    -DKF_IGNORE_PLATFORM_CHECK=ON
)
# CMAKE_PREFIX_PATH is a list; pass it through ExternalProject with a custom sep.
set(_prefix_path "${EXTPREFIX}|${QT_HOST_PATH}")

function(host_tool NAME REPO)
    cmake_parse_arguments(H "" "" "DEPENDS;CMAKE_ARGS" ${ARGN})
    ExternalProject_Add(${NAME}
        URL          https://github.com/KDE/${REPO}/archive/${KF_REF}.tar.gz
        DOWNLOAD_NAME ${NAME}.tar.gz
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
        DEPENDS      ${H_DEPENDS}
        LIST_SEPARATOR |
        CMAKE_ARGS   ${_host_args} -DCMAKE_PREFIX_PATH=${_prefix_path} ${H_CMAKE_ARGS}
    )
endfunction()

# ECM (arch-independent) first, then the tool-providing frameworks.
host_tool(host_ecm         extra-cmake-modules)
host_tool(host_kcoreaddons kcoreaddons DEPENDS host_ecm
    CMAKE_ARGS -DKCOREADDONS_USE_QML=OFF)
host_tool(host_kconfig     kconfig     DEPENDS host_kcoreaddons
    CMAKE_ARGS -DKCONFIG_USE_QML=OFF)
