# SPDX-License-Identifier: GPL-2.0-or-later
# Stub libintl for iOS (see libintl.c). Installs libintl.a + libintl.h so
# KI18n's find_package(LibIntl) succeeds.
cmake_minimum_required(VERSION 3.16)
project(proxy_libintl C)

add_library(intl STATIC libintl.c)
set_target_properties(intl PROPERTIES OUTPUT_NAME intl)

install(TARGETS intl ARCHIVE DESTINATION lib)
install(FILES libintl.h DESTINATION include)
