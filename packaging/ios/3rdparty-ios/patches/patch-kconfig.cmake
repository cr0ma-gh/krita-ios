# SPDX-License-Identifier: GPL-2.0-or-later
# Run as a PATCH_COMMAND in the kconfig source dir.
#
# KConfig builds kconfig_compiler unconditionally, even when cross-compiling
# (it only skips the KF6::kconfig_compiler alias). On iOS that target fails to
# link (_qt_main_wrapper) and could never run on the host anyway. Skip building
# it when cross-compiling — consumers pick up the host tool via KF6_HOST_TOOLING
# (see KF6ConfigConfig.cmake.in). A dummy INTERFACE target keeps the
# KF6ConfigCompilerTargets export non-empty so install(EXPORT) still succeeds.
set(_f "src/kconfig_compiler/CMakeLists.txt")
file(READ "${_f}" _c)
string(PREPEND _c
"if(CMAKE_CROSSCOMPILING)\n\
    add_library(kconfig_compiler INTERFACE)\n\
    install(TARGETS kconfig_compiler EXPORT KF6ConfigCompilerTargets)\n\
    return()\n\
endif()\n")
file(WRITE "${_f}" "${_c}")
message(STATUS "patch-kconfig: kconfig_compiler skipped for cross build")
