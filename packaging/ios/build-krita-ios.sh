#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# End-to-end iOS/iPadOS build orchestrator for Krita (Phase 0).
#
# One entry point that any macOS environment uses — a GitHub-hosted runner, a
# self-hosted Mac, or a developer's Mac. Keeps the CI YAML thin and
# makes the exact same build reproducible locally. See packaging/ios/ci.md.
#
# Steps: preflight -> dependencies -> configure -> build -> package unsigned .ipa
#
# Configuration (environment variables):
#   PLATFORM            OS64 | SIMULATORARM64                 (default OS64)
#   DEPLOYMENT_TARGET   minimum iOS version                   (default 16.0)
#   QT_IOS_ROOT         path to a Qt-for-iOS install          (REQUIRED)
#   WORK                scratch dir for prefixes/builds        (default ./_ios)
#   SKIP_DEPS           "1" to reuse an existing DEPS_PREFIX  (default auto)
#
# Honest status: the dependency superbuild is partial and Qt is expected to be
# stock unless you point QT_IOS_ROOT at a patched build, so the configure/build
# steps may not fully succeed yet. They are run with set +e so the script always
# reaches packaging and reports how far it got.

set -euo pipefail

PLATFORM="${PLATFORM:-OS64}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-16.0}"
WORK="${WORK:-$PWD/_ios}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TOOLCHAIN="${SRC_ROOT}/cmake/modules/ios.toolchain.cmake"

DEPS_PREFIX="${DEPS_PREFIX:-${WORK}/deps-${PLATFORM}}"
BUILD_DIR="${BUILD_DIR:-${WORK}/build-${PLATFORM}}"

# --- preflight -------------------------------------------------------------
if [[ "$(uname)" != "Darwin" ]]; then
    echo "error: this script builds for iOS and must run on macOS." >&2
    exit 1
fi
for tool in cmake ninja xcrun; do
    command -v "$tool" >/dev/null || { echo "error: '$tool' not found in PATH." >&2; exit 1; }
done
if [[ -z "${QT_IOS_ROOT:-}" ]]; then
    echo "error: set QT_IOS_ROOT to a Qt-for-iOS install (ideally patched)." >&2
    exit 1
fi

echo "==> Krita iOS build"
echo "    platform : ${PLATFORM}    min iOS: ${DEPLOYMENT_TARGET}"
echo "    qt       : ${QT_IOS_ROOT}"
echo "    sdk      : $(xcrun --sdk iphoneos --show-sdk-version)"
echo "    work     : ${WORK}"
mkdir -p "${WORK}"

# --- 1. dependencies -------------------------------------------------------
if [[ "${SKIP_DEPS:-}" == "1" || ( -d "${DEPS_PREFIX}/lib" && -n "$(ls -A "${DEPS_PREFIX}/lib" 2>/dev/null)" ) ]]; then
    echo "==> Reusing dependencies in ${DEPS_PREFIX}"
else
    echo "==> Building dependencies"
    bash "${SCRIPT_DIR}/build-deps.sh" "${PLATFORM}" "${DEPS_PREFIX}" "${DEPLOYMENT_TARGET}"
fi

# --- 2. configure ----------------------------------------------------------
echo "==> Configuring Krita"
# Qt6 cross-compilation needs the host Qt (moc/rcc/uic) in addition to the iOS
# Qt. Forward QT_HOST_PATH when provided (e.g. a desktop-macOS Qt of the same
# version); harmless to omit if the iOS Qt already knows its host tools.
extra_cmake=()
if [[ -n "${QT_HOST_PATH:-}" ]]; then
    extra_cmake+=( -DQT_HOST_PATH="${QT_HOST_PATH}" )
    # Host Qt tools (qtpaths/moc/rcc/uic) on PATH for ECM/KDE queries and the build.
    export PATH="${QT_HOST_PATH}/bin:${PATH}"
fi
set +e
cmake -S "${SRC_ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DPLATFORM="${PLATFORM}" \
    -DDEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    -DKRITA_DEPS_INSTALL_PREFIX="${DEPS_PREFIX}" \
    -DCMAKE_PREFIX_PATH="${DEPS_PREFIX};${QT_IOS_ROOT}" \
    -DBUILD_WITH_QT6=ON -DALLOW_UNSTABLE=QT6 \
    -DBUILD_TESTING=OFF \
    "${extra_cmake[@]}"
cfg=$?

# --- 3. build --------------------------------------------------------------
if [[ $cfg -eq 0 ]]; then
    echo "==> Building"
    cmake --build "${BUILD_DIR}" --parallel
fi
set -e

# --- 4. package ------------------------------------------------------------
APP="${BUILD_DIR}/krita/krita.app"
if [[ -d "${APP}" ]]; then
    echo "==> Packaging unsigned .ipa"
    rm -rf "${WORK}/Payload"
    mkdir -p "${WORK}/Payload"
    cp -R "${APP}" "${WORK}/Payload/"
    ( cd "${WORK}" && zip -qr "krita-unsigned-${PLATFORM}.ipa" Payload )
    echo "==> Built ${WORK}/krita-unsigned-${PLATFORM}.ipa"
    echo "    Sideload it onto your iPad with SideStore/AltStore (free Apple ID)."
else
    echo "==> krita.app not produced yet — expected during Phase 0."
    echo "    Most likely the dependency superbuild or Qt-for-iOS is incomplete;"
    echo "    see packaging/ios/dependencies.md and README.ios.md §7."
    exit 0
fi
