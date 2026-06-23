# SPDX-License-Identifier: GPL-2.0-or-later
#
# CMake toolchain for building Krita (and its dependencies) for iOS / iPadOS.
#
# This file is part of the *Phase 0* scaffolding described in README.ios.md.
# It is authored on any OS, but only *executes* on macOS (it needs the iOS SDK,
# Apple clang and `xcrun`). Configuring it on Windows/Linux will fail at the
# compiler check — that is expected; the real build runs on a macOS CI runner.
#
# Usage (on macOS):
#   cmake -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/modules/ios.toolchain.cmake \
#         -DPLATFORM=OS64 \
#         -DDEPLOYMENT_TARGET=16.0 \
#         -DCMAKE_INSTALL_PREFIX=<deps-prefix> \
#         <source-dir>
#
# PLATFORM values:
#   OS64            arm64 device      (iPhone/iPad, real hardware)  -> sysroot iphoneos
#   SIMULATORARM64  arm64 simulator   (Apple-silicon Mac)           -> sysroot iphonesimulator
#
# This intentionally relies on CMake's *built-in* iOS support (CMAKE_SYSTEM_NAME
# iOS, available since CMake 3.14) rather than re-implementing a large bespoke
# toolchain. Keep it small; let CMake/Xcode do the Apple-specific plumbing.

cmake_minimum_required(VERSION 3.16)

# Guard against double inclusion (CMake includes the toolchain several times).
if(DEFINED KRITA_IOS_TOOLCHAIN_INCLUDED)
    return()
endif()
set(KRITA_IOS_TOOLCHAIN_INCLUDED YES)

# --- Target platform -------------------------------------------------------
set(CMAKE_SYSTEM_NAME iOS)

# Convenience variable used by Krita's CMakeLists (if(IOS) ...). CMake itself
# only sets APPLE and CMAKE_SYSTEM_NAME; many projects expect IOS to exist too.
set(IOS TRUE)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "OS64")
endif()

if(NOT DEFINED DEPLOYMENT_TARGET)
    # "Current iPadOS" target. 16.0 covers every iPad able to run a modern
    # iPadOS while still giving us the recent UIKit/PencilKit APIs. Bump as
    # needed; lower only if you must support older hardware.
    set(DEPLOYMENT_TARGET "16.0")
endif()

if(PLATFORM STREQUAL "OS64")
    set(CMAKE_OSX_SYSROOT iphoneos)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(_KRITA_IOS_TRIPLE "arm64-apple-ios${DEPLOYMENT_TARGET}")
elseif(PLATFORM STREQUAL "SIMULATORARM64")
    set(CMAKE_OSX_SYSROOT iphonesimulator)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(_KRITA_IOS_TRIPLE "arm64-apple-ios${DEPLOYMENT_TARGET}-simulator")
else()
    message(FATAL_ERROR
        "Unsupported PLATFORM='${PLATFORM}'. Use OS64 or SIMULATORARM64.")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "${DEPLOYMENT_TARGET}"
    CACHE STRING "Minimum iOS deployment target" FORCE)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# --- Static-only world -----------------------------------------------------
# iOS forbids dlopen() of unsigned code and JIT, so every Krita plugin and
# every third-party dependency must be a static library. See README.ios.md §2.2
# and packaging/ios/static-plugins.cmake.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "iOS is static-only" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Skip tests when cross-compiling" FORCE)

# Bitcode was deprecated and removed by Apple; never enable it.
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE NO CACHE STRING "" FORCE)

# --- find_*() search behaviour ---------------------------------------------
# Programs (moc, rcc, host tools) may come from the host; libraries, headers
# and CMake packages must come from the iOS sysroot / our deps prefix only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Frameworks last: prefer the libraries we cross-compiled over system ones.
set(CMAKE_FIND_FRAMEWORK LAST)

# Let consumers extend the prefix (e.g. the dependency install root).
if(DEFINED KRITA_DEPS_INSTALL_PREFIX)
    list(APPEND CMAKE_FIND_ROOT_PATH "${KRITA_DEPS_INSTALL_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH "${KRITA_DEPS_INSTALL_PREFIX}")
endif()

message(STATUS "Krita iOS toolchain: PLATFORM=${PLATFORM} "
               "triple=${_KRITA_IOS_TRIPLE} sysroot=${CMAKE_OSX_SYSROOT}")
