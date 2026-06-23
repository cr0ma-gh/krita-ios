#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Cross-compile Krita's native dependencies for iOS (Phase 0).
# Runs on macOS. Drives the superbuild in packaging/ios/3rdparty-ios for one
# platform and installs into a prefix that the main Krita build then consumes.
#
# Usage:
#   packaging/ios/build-deps.sh <PLATFORM> <EXTPREFIX> [DEPLOYMENT_TARGET]
#     PLATFORM           OS64 | SIMULATORARM64
#     EXTPREFIX          install prefix for this platform's static libs
#     DEPLOYMENT_TARGET  iOS min version (default 16.0)
#
# Because Krita is built per-platform anyway, this builds deps for a single
# platform. To produce universal .xcframeworks instead, build both platforms
# into two prefixes and run `xcodebuild -create-xcframework` over the matching
# .a pairs (see packaging/ios/dependencies.md).

set -euo pipefail

PLATFORM="${1:?PLATFORM required (OS64 | SIMULATORARM64)}"
EXTPREFIX="${2:?EXTPREFIX required}"
DEPLOYMENT_TARGET="${3:-16.0}"

# Resolve repo root from this script's location (packaging/ios/build-deps.sh).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TOOLCHAIN="${SRC_ROOT}/cmake/modules/ios.toolchain.cmake"
BUILD_DIR="${EXTPREFIX}.build"

if [[ "$(uname)" != "Darwin" ]]; then
    echo "error: iOS dependencies can only be cross-compiled on macOS." >&2
    exit 1
fi

echo "==> Building Krita iOS deps"
echo "    platform : ${PLATFORM}"
echo "    prefix   : ${EXTPREFIX}"
echo "    min iOS  : ${DEPLOYMENT_TARGET}"
echo "    sdk      : $(xcrun --sdk iphoneos --show-sdk-version)"

mkdir -p "${EXTPREFIX}"

cmake -S "${SRC_ROOT}/packaging/ios/3rdparty-ios" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DPLATFORM="${PLATFORM}" \
    -DDEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    -DEXTPREFIX="${EXTPREFIX}"

# ExternalProject targets serialise on their DEPENDS, so a plain build resolves
# the dependency order. Use --parallel for the independent leaves.
cmake --build "${BUILD_DIR}" --parallel

echo "==> Done. Static deps installed in ${EXTPREFIX}"
echo "    Pass -DKRITA_DEPS_INSTALL_PREFIX=${EXTPREFIX} to the Krita configure."
