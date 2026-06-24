# SPDX-License-Identifier: GPL-2.0-or-later
#
# Injected via CMAKE_PROJECT_krita_INCLUDE, right after project(krita ...), to
# patch iOS-specific gaps in the dependency configs before Krita's CMake runs.

# libtiff's installed config references the CMath::CMath target, but its bundled
# FindCMath.cmake (which would define it) is not installed for consumers. On
# Apple libm is part of libSystem, so an empty interface target satisfies the
# reference without an explicit -lm.
if(NOT TARGET CMath::CMath)
    add_library(CMath::CMath INTERFACE IMPORTED)
endif()
