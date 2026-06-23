# SPDX-License-Identifier: GPL-2.0-or-later
#
# Static plugin registration for the Krita iOS/iPadOS build (Phase 0/1).
#
# iOS forbids dlopen() of unsigned code, so Krita's ~124 KPluginFactory plugins
# (normally discovered at runtime by KoJsonTrader via QPluginLoader) must be
# linked statically and registered up-front with Q_IMPORT_PLUGIN. See
# README.ios.md §2.2 and packaging/ios/plugins-audit.md for the full inventory.
#
# This file provides:
#   * KRITA_IOS_PLUGIN_DENYLIST       — factories NOT imported on iOS (+ reason)
#   * KRITA_IOS_ALL_PLUGIN_FACTORIES  — snapshot of every factory found in tree
#   * krita_ios_generate_plugin_imports(...) — writes the registration TU
#
# The snapshot list is a *convenience*; the canonical list should be regenerated
# by scanning K_PLUGIN_FACTORY_WITH_JSON in the configured plugin targets so it
# never drifts. The same scan that produced plugins-audit.md can refresh it.

cmake_minimum_required(VERSION 3.17)  # CMAKE_CURRENT_FUNCTION_LIST_DIR

# --- Factories that must NOT be imported on iOS ----------------------------
set(KRITA_IOS_PLUGIN_DENYLIST
    KritaPlatformPluginWaylandFactory   # Linux Wayland QPA — not applicable on iOS
    KritaPlatformPluginXcbFactory       # Linux X11/XCB QPA — not applicable on iOS
    KritaPyQtPluginFactory              # PyKrita scripting — dropped in v1 (no dynamic CPython)
    PDFImportFactory                    # Poppler PDF import — heavy stack, deferred past v1
    QMicFactory                         # G'MIC via external process — iOS forbids subprocess
    SPenSettingsFactory                 # Samsung S-Pen settings — Android-only hardware
    DbExplorerFactory                   # developer-only resource DB explorer — not shipped
    KritaExampleFactory                 # SDK example filter — not user facing
    CACHE STRING "Krita plugin factories excluded from the iOS static build")

# --- Snapshot of all plugin factories discovered in the source tree --------
# Generated 2026-06 from K_PLUGIN_FACTORY_WITH_JSON occurrences (124 distinct,
# tests/dummies excluded). Keep in sync with packaging/ios/plugins-audit.md.
set(KRITA_IOS_ALL_PLUGIN_FACTORIES
    AnimationDockersPluginFactory
    ArrangeDockerPluginFactory
    BlurFilterPluginFactory
    BugInfoFactory
    CSVImportFactory
    ChannelDockerPluginFactory
    ClonesArrayFactory
    ColorSelectorNgPluginFactory
    ColorSmudgePaintOpPluginFactory
    ColorsFiltersFactory
    CompositionDockerPluginFactory
    CurvePaintOpPluginFactory
    DbExplorerFactory
    DefaultToolsFactory
    DeformPaintOpPluginFactory
    DodgeBurnPluginFactory
    ExperimentPaintOpPluginFactory
    ExportFactory
    FilterOpFactory
    GaussianHighPassPluginFactory
    GridDockerPluginFactory
    GridPaintOpPluginFactory
    HairyPaintOpPluginFactory
    HatchingPaintOpPluginFactory
    HeightMapImportFactory
    HistogramDockerPluginFactory
    HistoryPluginFactory
    ImageShapePluginFactory
    ImagesplitFactory
    ImportFactory
    IndexColorsFactory
    KarbonToolsPluginFactory
    KisBrushExportFactory
    KisBrushImportFactory
    KisCSVExportFactory
    KisEmbossFilterPluginFactory
    KisExifIOPluginFactory
    KisGIFExportFactory
    KisGIFImportFactory
    KisHeightMapExportFactory
    KisIptcIOPluginFactory
    KisOilPaintFilterPluginFactory
    KisPhongBumpmapFactory
    KisPixelizeFilterPluginFactory
    KisQImageIOExportFactory
    KisQImageIOImportFactory
    KisRGBEImportFactory
    KisRainDropsFilterPluginFactory
    KisRawImportFactory
    KisRoundCornersFilterPluginFactory
    KisSampleScreenColorFactory
    KisSmallTilesFilterPluginFactory
    KisSpriterExportFactory
    KisTGAExportFactory
    KisTGAImportFactory
    KisToolEncloseAndFillPluginFactory
    KisWebPExportFactory
    KisWebPImportFactory
    KritaASCCDLFactory
    KritaColorGeneratorFactory
    KritaConvertHeightToNormalMapFilterFactory
    KritaConvolutionFiltersFactory
    KritaEdgeDetectionFilterFactory
    KritaExampleFactory
    KritaExtensionsColorsFactory
    KritaFastColorTransferFactory
    KritaGradientGeneratorFactory
    KritaGradientMapFilterFactory
    KritaHalftoneFactory
    KritaImageEnhancementFactory
    KritaLayerDockerPluginFactory
    KritaMultigridPatternGeneratorFactory
    KritaNoiseFilterFactory
    KritaNormalizeFilterFactory
    KritaPatternGeneratorFactory
    KritaPlatformPluginWaylandFactory
    KritaPlatformPluginXcbFactory
    KritaPyQtPluginFactory
    KritaRandomPickFilterFactory
    KritaScreentoneGeneratorFactory
    KritaSeExprGeneratorFactory
    KritaSimplexNoiseGeneratorFactory
    KritaThresholdFactory
    KritaWaveFilterFactory
    KrzExportFactory
    LayerGroupSwitcherFactory
    LayerSplitFactory
    LevelsFilterFactory
    LogDockerPluginFactory
    LutDockerPluginFactory
    MyPaintOpPluginFactory
    OffsetImageFactory
    OverviewDockerPluginFactory
    PDFImportFactory
    PaletteDockPluginFactory
    PaletteDockerPluginFactory
    PalettizeFactory
    ParticlePaintOpPluginFactory
    PathShapesPluginFactory
    PatternDockerPluginFactory
    PluginFactory
    PosterizeFactory
    PresetDockerPluginFactory
    PresetHistoryPluginFactory
    PropagateColorsFilterFactory
    QMicFactory
    RecorderDockerPluginFactory
    ResetTransparentFactory
    ResourceManagerFactory
    RoundMarkerPaintOpPluginFactory
    SPenSettingsFactory
    SketchPaintOpPluginFactory
    SnapshotPluginFactory
    SprayPaintOpPluginFactory
    StoryboardDockerPluginFactory
    TangentNormalPaintOpPluginFactory
    TasksetDockerPluginFactory
    TextPropertiesPluginFactory
    ToolDynaFactory
    TouchDockerPluginFactory
    UnsharpPluginFactory
    WGColorSelectorPluginFactory
    WaveletDecomposeFactory
    XCFImportFactory
    CACHE STRING "Snapshot of all Krita plugin factories (regenerate by scanning)")

# --- Generator -------------------------------------------------------------
# krita_ios_generate_plugin_imports(
#     OUTPUT     <path/to/KisStaticPluginsInit.cpp>
#     [FACTORIES <list>]   # defaults to KRITA_IOS_ALL_PLUGIN_FACTORIES
# )
# Emits a translation unit with Q_IMPORT_PLUGIN for every factory not in the
# denylist. Add the OUTPUT file to the krita executable target's sources, and
# make sure each plugin is built with QT_STATICPLUGIN and linked statically.
function(krita_ios_generate_plugin_imports)
    cmake_parse_arguments(ARG "" "OUTPUT" "FACTORIES" ${ARGN})
    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "krita_ios_generate_plugin_imports: OUTPUT is required")
    endif()
    if(NOT ARG_FACTORIES)
        set(ARG_FACTORIES ${KRITA_IOS_ALL_PLUGIN_FACTORIES})
    endif()

    set(_imports "")
    set(_kept 0)
    set(_skipped 0)
    foreach(_f IN LISTS ARG_FACTORIES)
        if(_f IN_LIST KRITA_IOS_PLUGIN_DENYLIST)
            string(APPEND _imports "// excluded on iOS: ${_f}\n")
            math(EXPR _skipped "${_skipped} + 1")
        else()
            string(APPEND _imports "Q_IMPORT_PLUGIN(${_f})\n")
            math(EXPR _kept "${_kept} + 1")
        endif()
    endforeach()

    set(KRITA_IOS_PLUGIN_IMPORTS "${_imports}")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/KisStaticPluginsInit.cpp.in"
        "${ARG_OUTPUT}"
        @ONLY)
    message(STATUS "Krita iOS: imported ${_kept} static plugins, skipped ${_skipped}")
endfunction()
