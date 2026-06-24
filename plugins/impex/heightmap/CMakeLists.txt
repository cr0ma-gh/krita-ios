add_subdirectory(tests)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# Shared options-widget + utils built once into a static helper library, so a
# static link of both plugins (iOS) does not duplicate the moc'd
# KisWdgOptionsHeightmap symbols. Other platforms are unaffected.
set(kritaheightmapcommon_SOURCES
    kis_wdg_options_heightmap.cpp
    kis_heightmap_utils.cpp
    )
ki18n_wrap_ui(kritaheightmapcommon_SOURCES kis_wdg_options_heightmap.ui )
add_library(kritaheightmapcommon STATIC ${kritaheightmapcommon_SOURCES})
target_link_libraries(kritaheightmapcommon PUBLIC kritaui)

set(kritaheightmapimport_SOURCES
    kis_heightmap_import.cpp
    )

kis_add_library(kritaheightmapimport MODULE ${kritaheightmapimport_SOURCES})

target_link_libraries(kritaheightmapimport kritaui kritaheightmapcommon)

install(TARGETS kritaheightmapimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritaheightmapexport_SOURCES
    kis_heightmap_export.cpp
    )

kis_add_library(kritaheightmapexport MODULE ${kritaheightmapexport_SOURCES})

target_link_libraries(kritaheightmapexport kritaui kritaimpex kritaheightmapcommon)

install(TARGETS kritaheightmapexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_heightmap.desktop DESTINATION ${KDE_INSTALL_APPDIR})
