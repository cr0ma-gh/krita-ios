add_subdirectory( dtd )
add_subdirectory( data )
add_subdirectory( integration )
# Install the application icons following the freedesktop icon theme spec
add_subdirectory( "pics/branding/${BRANDING}" )

if (ANDROID)
    if (QT_MAJOR_VERSION STREQUAL "5")
        set (QtAndroidExtras_INCLUDE_DIRS ${Qt5AndroidExtras_INCLUDE_DIRS})
        include_directories (${QtAndroidExtras_INCLUDE_DIRS})
    endif()
endif()

set(krita_SRCS main.cc)

if (IOS)
    # iOS links every plugin statically (no dlopen); generate the
    # Q_IMPORT_PLUGIN translation unit. See packaging/ios/static-plugins.cmake
    # and README.ios.md §2.2.
    include(${CMAKE_SOURCE_DIR}/packaging/ios/static-plugins.cmake)
    krita_ios_generate_plugin_imports(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/KisStaticPluginsInit.cpp)
    list(APPEND krita_SRCS ${CMAKE_CURRENT_BINARY_DIR}/KisStaticPluginsInit.cpp)
endif()

# Set the application icon on the application
if (NOT APPLE)
	file(GLOB ICON_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/*-apps-krita.png")
elseif(APPLE AND NOT IOS)
    # XCode below 26 cannot work with .icon format
    if(${XCODE_VERSION} VERSION_GREATER_EQUAL "26.0")
        find_program(XCRUN_EXECUTABLE NAMES xcrun)
        set(KRITA_ASSETS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/krita.icon")
        cmake_path(SET ASSETS_PATH "${CMAKE_CURRENT_BINARY_DIR}/assets")

        set(ASSETS_FILES "${ASSETS_PATH}/Assets.car" "${ASSETS_PATH}/krita.icns")
        add_custom_command(
          OUTPUT ${ASSETS_FILES}
          COMMAND ${CMAKE_COMMAND} ARGS -E make_directory "${ASSETS_PATH}"
          COMMAND ${XCRUN_EXECUTABLE} ARGS actool "${KRITA_ASSETS_ICON}"
            --compile ${ASSETS_PATH}
            --platform macosx --minimum-deployment-target ${MACOS_MINIMUM_VERSION}
            --app-icon krita --output-partial-info-plist "${CMAKE_CURRENT_BINARY_DIR}/krita_icon.plist"
            --output-format human-readable-text
            --standalone-icon-behavior all
          DEPENDS
            "${KRITA_ASSETS_ICON}"
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    else()
        set(ICON_SRCS
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/16-apps-krita.png"
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/32-apps-krita.png"
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/48-apps-krita.png"
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/128-apps-krita.png"
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/256-apps-krita.png"
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/512-apps-krita.png"
             "${CMAKE_CURRENT_SOURCE_DIR}/pics/branding/${BRANDING}/1024-apps-krita.png"
        )
    endif() # XCode version
endif()

if (WIN32)
    # Private headers are needed for main.cc to enable WinTab support
    set_source_files_properties(
        main.cc
        PROPERTIES
        INCLUDE_DIRECTORIES "${Qt${QT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS}"
        )
endif()

if (WIN32)
    set(krita_windows_stub_ICONS_SRCS "")
    ecm_add_app_icon(krita_windows_stub_ICONS_SRCS ICONS ${ICON_SRCS} OUTFILE_BASENAME krita)

    # This is used to generate an ICO file for the file icon, We don't actually
    # want to embed this icon into an EXE.
	file(GLOB krafile_ICON_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/*-mimetypes-application-x-krita.png")
    set(krafile_dummy_ICONS_SRCS "")
    ecm_add_app_icon(krafile_dummy_ICONS_SRCS ICONS ${krafile_ICON_SRCS} OUTFILE_BASENAME kritafile)
    add_library(krafile_dummy_obj OBJECT ${krafile_dummy_ICONS_SRCS})

    # These icons are used to replace those in the shell extension package.
    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/krita.ico
            ${CMAKE_CURRENT_BINARY_DIR}/kritafile.ico
        DESTINATION ${CMAKE_INSTALL_PREFIX}/installer
    )
elseif(APPLE AND NOT IOS)
    if(${XCODE_VERSION} VERSION_GREATER_EQUAL "26.0")
      set_source_files_properties(${ASSETS_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    else()
      ecm_add_app_icon(krita_SRCS OUTFILE_BASENAME krita ICONS ${ICON_SRCS})
    endif()
elseif(NOT APPLE)
    ecm_add_app_icon(krita_SRCS ICONS ${ICON_SRCS})
endif()

# Install the mimetype icons
ecm_install_icons(ICONS
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/16-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/22-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/32-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/48-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/64-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/128-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/256-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/512-mimetypes-application-x-krita.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/pics/mimetypes/1024-mimetypes-application-x-krita.png"
    DESTINATION ${KDE_INSTALL_ICONDIR} THEME hicolor)

# separate listing, both used by Krita and KritaSketch
set(krita_QRCS
    ${CMAKE_SOURCE_DIR}/krita/krita.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/Breeze-dark/breeze-dark-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/Breeze-light/breeze-light-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/layerbox/layerbox-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/layers/layers-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/misc-light/misc-light-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/misc-dark/misc-dark-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/paintops/paintops-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/tools/SVG/16/tools-svg-16-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/tool_transform/tool-transform-icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/svg/svg-icons.qrc
    ${CMAKE_SOURCE_DIR}/libs/flake/flake.qrc
    ${CMAKE_SOURCE_DIR}/libs/widgets/kritawidgets.qrc
    ${CMAKE_SOURCE_DIR}/pics/icons.qrc
    ${CMAKE_SOURCE_DIR}/krita/data/aboutdata/aboutdata.qrc
    ${CMAKE_SOURCE_DIR}/krita/data/shaders/shaders.qrc
    ${CMAKE_SOURCE_DIR}/krita/data/cursors/cursors.qrc
    ${CMAKE_SOURCE_DIR}/krita/data/gamutmasks/gamutmasks.qrc
    ${CMAKE_SOURCE_DIR}/plugins/paintops/defaultpresets/defaultpresets.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/branding/${BRANDING}/branding.qrc
    ${CMAKE_SOURCE_DIR}/krita/data/splash/splash.qrc
    ${CMAKE_SOURCE_DIR}/krita/pics/sponsors/sponsors.qrc
    CACHE INTERNAL "krita_QRCS"
)

if (QT_MAJOR_VERSION STREQUAL "6")
    list(APPEND krita_QRCS ${CMAKE_SOURCE_DIR}/krita/data/mime-database/mime-database.qrc)
endif()

qt_add_resources(krita_SRCS ${krita_QRCS})

if (ANDROID AND QT_MAJOR_VERSION STREQUAL "5")
    kis_add_library(krita SHARED ${krita_SRCS})
    target_link_libraries(krita PRIVATE Qt5::AndroidExtras)
elseif (WIN32)
    # Build the main implementation into a DLL to be called by a stub EXE.
    kis_add_library(krita SHARED ${krita_SRCS})
    set_target_properties(krita PROPERTIES PREFIX "")
elseif (IOS)
    # On iOS a GUI application is an application bundle (.app).
    add_executable(krita MACOSX_BUNDLE ${krita_SRCS})
else()
    add_executable(krita ${krita_SRCS})
endif()
target_link_libraries(krita
    PRIVATE
                      kritamultiarch
                      kritaui
                      Qt${QT_MAJOR_VERSION}::Core
                      Qt${QT_MAJOR_VERSION}::Gui
                      Qt${QT_MAJOR_VERSION}::Widgets
                      Qt${QT_MAJOR_VERSION}::Xml
                      Qt${QT_MAJOR_VERSION}::Network
                      Qt${QT_MAJOR_VERSION}::PrintSupport
                      Qt${QT_MAJOR_VERSION}::Svg)
if(HAVE_KCRASH)
    target_link_libraries(krita PRIVATE KF${KF_MAJOR}::Crash)
endif()

if (IOS)
    # iOS links every plugin statically. Whole-archive each one so the factory
    # registered by Q_IMPORT_PLUGIN (in KisStaticPluginsInit.cpp above) — and
    # all code reachable only through Krita's runtime factory registration —
    # survives the linker's dead-stripping. The plugin targets are collected by
    # kis_add_library (which also defines QT_STATICPLUGIN on them).
    get_property(_krita_ios_plugins GLOBAL PROPERTY KRITA_IOS_PLUGIN_TARGETS)
    if (_krita_ios_plugins)
        list(REMOVE_DUPLICATES _krita_ios_plugins)
        foreach(_plugin IN LISTS _krita_ios_plugins)
            target_link_libraries(krita PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>")
        endforeach()
        list(LENGTH _krita_ios_plugins _krita_ios_plugin_count)
        message(STATUS "Krita iOS: whole-archive linking ${_krita_ios_plugin_count} static plugins")
    endif()

    # exiv2 (XMP) and fontconfig (XML config) use expat, and KI18n/fontconfig
    # use iconv. As prebuilt static libs they don't propagate these leaf deps
    # to the final executable link, so link them here explicitly (last, so the
    # archives that reference them are seen first).
    find_package(EXPAT)
    if (TARGET EXPAT::EXPAT)
        target_link_libraries(krita PRIVATE EXPAT::EXPAT)
    endif()
    find_package(Iconv)
    if (TARGET Iconv::Iconv)
        target_link_libraries(krita PRIVATE Iconv::Iconv)
    endif()
endif()

if (APPLE AND NOT IOS)
    set(KRITA_VERSION_NUMBER_STRING "${KRITA_STABLE_VERSION_MAJOR}.${KRITA_STABLE_VERSION_MINOR}.${KRITA_VERSION_RELEASE}")
    set_target_properties(krita PROPERTIES INSTALL_RPATH "@loader_path/../../../../lib;@loader_path/../lib;
        @loader_path/../Frameworks;@executable_path/../../../../lib;@executable_path/../lib;@executable_path/../Frameworks")
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.template)
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "org.krita")
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Krita")
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_ICON_FILE "krita")
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_LONG_VERSION_STRING ${KRITA_VERSION_STRING})
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING ${KRITA_VERSION_NUMBER_STRING})
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION ${KRITA_VERSION_NUMBER_STRING})
    set_target_properties(krita PROPERTIES MACOSX_BUNDLE_COPYRIGHT "GNU Public License, V2 or, at your option, any later version.")
endif ()

if (IOS)
    # iPadOS application bundle. The Info.plist is configured from the template
    # in packaging/ios (document types, orientations, Apple Pencil, file access).
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.krita")
    set(MACOSX_BUNDLE_BUNDLE_NAME "krita")
    if(NOT DEPLOYMENT_TARGET)
        set(DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    configure_file(
        ${CMAKE_SOURCE_DIR}/packaging/ios/Info.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)
    set_target_properties(krita PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.kde.krita")

    # Bundle the launch screen and the app-icon asset catalog as resources.
    target_sources(krita PRIVATE
        ${CMAKE_SOURCE_DIR}/packaging/ios/LaunchScreen.storyboard
        ${CMAKE_SOURCE_DIR}/packaging/ios/Assets.xcassets)
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/packaging/ios/LaunchScreen.storyboard
        ${CMAKE_SOURCE_DIR}/packaging/ios/Assets.xcassets
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif ()

if (NOT ANDROID AND NOT IOS)
  set(KRITAVERSION_SRCS kritaversion.cpp)
  add_executable(krita_version ${KRITAVERSION_SRCS})
  target_link_libraries(krita_version
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Core
        kritaversion
  )

  if(APPLE)
      set_target_properties(krita_version PROPERTIES INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../lib;@executable_path/../Frameworks")
      set_property(TARGET krita_version PROPERTY MACOSX_BUNDLE OFF)
  endif ()

  install(TARGETS krita_version ${INSTALL_TARGETS_DEFAULT_ARGS})
endif()


if(APPLE AND NOT IOS)
    set(KRITA_ASSET_KRZ_ICNS "krita-krz.icns" "krita-kra.icns")
    add_custom_command(
      OUTPUT ${KRITA_ASSET_KRZ_ICNS}
      COMMAND ${CMAKE_COMMAND}
      ARGS -E copy
      "./pics/mimetypes/krz/krita-krz.icns"
      "./pics/mimetypes/krita-kra.icns"
      "${CMAKE_CURRENT_BINARY_DIR}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_source_files_properties(${KRITA_ASSET_KRZ_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # make krita depend on assets icons
    if(${XCODE_VERSION} VERSION_GREATER_EQUAL "26.0")
      target_sources(krita PRIVATE ${ASSETS_FILES})
    endif()
    target_sources(krita PRIVATE ${KRITA_ASSET_KRZ_ICNS})

    install(TARGETS krita ${INSTALL_TARGETS_DEFAULT_ARGS} BUNDLE DESTINATION bin)
elseif(IOS)
    # krita is a MACOSX_BUNDLE on iOS too, so it needs a BUNDLE DESTINATION.
    install(TARGETS krita ${INSTALL_TARGETS_DEFAULT_ARGS} BUNDLE DESTINATION bin)
else()
    install(TARGETS krita ${INSTALL_TARGETS_DEFAULT_ARGS})
endif()
install(FILES org.kde.krita.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES  krita.action kritamenu.action DESTINATION ${KDE_INSTALL_DATADIR}/krita/actions)
install(FILES  org.kde.krita.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )

install(DIRECTORY DESTINATION ${KDE_INSTALL_DATADIR}/krita/shortcuts)

if (WIN32)
    configure_file(versioninfo.rc.in ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc)
    if (QT_MAJOR_VERSION STREQUAL "6")
        configure_file(krita6.exe.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/krita.exe.manifest)
    else()
        configure_file(krita.exe.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/krita.exe.manifest)
    endif()
    if (MINGW)
        # CMake does not know how to embed manifest on mingw, so we create our own RC file to be included
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/krita.exe.manifest.rc "1 24 \"krita.exe.manifest\"\n")
        set(KRITA_MANIFEST_SRC ${CMAKE_CURRENT_BINARY_DIR}/krita.exe.manifest.rc)
    else()
        set(KRITA_MANIFEST_SRC
            ${CMAKE_CURRENT_BINARY_DIR}/krita.exe.manifest)
    endif()
    add_executable(krita_windows_stub_exe
        windows_stub_main.cpp
        ${krita_windows_stub_ICONS_SRCS}
        ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc
        ${KRITA_MANIFEST_SRC}
    )
    target_link_libraries(krita_windows_stub_exe PRIVATE krita)
    if (QT_MAJOR_VERSION STREQUAL "6")
        target_link_libraries(krita_windows_stub_exe PRIVATE Qt6::EntryPointPrivate)
    else()
        target_link_libraries(krita_windows_stub_exe PRIVATE Qt5::WinMain)
    endif()
    set_target_properties(krita_windows_stub_exe
        PROPERTIES
            OUTPUT_NAME "krita"
    )
    if (MSVC)
        target_link_options(krita_windows_stub_exe PRIVATE "/DEBUG:NONE")
    endif()
    install(TARGETS krita_windows_stub_exe ${INSTALL_TARGETS_DEFAULT_ARGS})

    add_executable(krita_windows_stub_com
        windows_stub_main.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc
        ${KRITA_MANIFEST_SRC}
    )
    target_link_libraries(krita_windows_stub_com PRIVATE krita)
    set_target_properties(krita_windows_stub_com
        PROPERTIES
            WIN32_EXECUTABLE false
            OUTPUT_NAME "krita"
            SUFFIX ".com"
    )
    if (MSVC)
        target_link_options(krita_windows_stub_com PRIVATE "/DEBUG:NONE")
        # If MSVC uses incremental linking, it will race the database
        # for krita.exe with the one for krita.com.
        add_dependencies(krita_windows_stub_com krita_windows_stub_exe)
    endif()
    install(TARGETS krita_windows_stub_com ${INSTALL_TARGETS_DEFAULT_ARGS})
endif (WIN32)
