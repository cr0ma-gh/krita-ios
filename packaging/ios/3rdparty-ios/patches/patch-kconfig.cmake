# SPDX-License-Identifier: GPL-2.0-or-later
# Run as a PATCH_COMMAND in the kconfig source dir (cross/iOS build only).
#
# KConfig's console tools don't work on iOS: they link Qt and need the iOS
# _qt_main_wrapper (missing for plain add_executable), and kconf_update uses
# QProcess (absent on iOS). kconfig_compiler is provided by the host build via
# KF6_HOST_TOOLING (see KF6ConfigConfig.cmake.in). src/core references
# KF6::kconf_update, so the tools can't simply be removed — replace them with
# trivial non-Qt dummies that build, satisfy the target references, and are
# never run on iOS.

# 1. kconfig_compiler: host-tooled — skip building it, keep the export non-empty.
set(_f "src/kconfig_compiler/CMakeLists.txt")
file(READ "${_f}" _c)
string(PREPEND _c "if(CMAKE_CROSSCOMPILING)\n  add_library(kconfig_compiler INTERFACE)\n  install(TARGETS kconfig_compiler EXPORT KF6ConfigCompilerTargets)\n  return()\nendif()\n")
file(WRITE "${_f}" "${_c}")

# 2. dummy source + dummy tool CMakeLists.
file(WRITE "src/_ios_dummy_tool.cpp" "int main(int, char **){ return 0; }\n")

file(WRITE "src/kconf_update/CMakeLists.txt"
"add_executable(kconf_update \"\${CMAKE_SOURCE_DIR}/src/_ios_dummy_tool.cpp\")\n"
"add_executable(KF6::kconf_update ALIAS kconf_update)\n"
"install(TARGETS kconf_update DESTINATION \"\${KDE_INSTALL_BINDIR}\")\n")

file(WRITE "src/kreadconfig/CMakeLists.txt"
"add_executable(kreadconfig6 \"\${CMAKE_SOURCE_DIR}/src/_ios_dummy_tool.cpp\")\n"
"add_executable(kwriteconfig6 \"\${CMAKE_SOURCE_DIR}/src/_ios_dummy_tool.cpp\")\n"
"install(TARGETS kreadconfig6 kwriteconfig6 DESTINATION \"\${KDE_INSTALL_BINDIR}\")\n")

message(STATUS "patch-kconfig: host-tooled kconfig_compiler; dummied console tools for iOS")
