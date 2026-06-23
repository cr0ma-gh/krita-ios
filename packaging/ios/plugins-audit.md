# Audit di staticizzazione dei plugin Krita per iOS

Parte dell'impalcatura di **Fase 0/1** (vedi [README.ios.md](../../README.ios.md) §2.2).

iOS vieta `dlopen()` di codice non firmato e il JIT: i plugin di Krita — normalmente
`.so`/`.dll` caricati a runtime da `KoJsonTrader` tramite `QPluginLoader`
([KoPluginLoader.cpp](../../libs/koplugin/KoPluginLoader.cpp)) — devono essere **linkati
staticamente** e registrati con `Q_IMPORT_PLUGIN`. Questo file inventaria tutti i plugin
e indica quali importare.

## Numeri

- **124** factory di plugin distinte (`K_PLUGIN_FACTORY_WITH_JSON`), test/dummy esclusi.
- **8** escluse su iOS (denylist, vedi sotto) → **116** importate staticamente.
- Generazione del codice di registrazione: [static-plugins.cmake](static-plugins.cmake)
  → template [KisStaticPluginsInit.cpp.in](KisStaticPluginsInit.cpp.in).

Distribuzione per categoria: filtri 33 · impex (import/export) 27 · docker 27 ·
paintop 14 · extensions 13 · tool 8 · generators 7 · metadata 3 · flake 2 · platforms 2.

## Esclusi su iOS (denylist)

| Factory | Motivo |
|---|---|
| `KritaPlatformPluginWaylandFactory` | QPA Wayland — solo Linux |
| `KritaPlatformPluginXcbFactory` | QPA X11/XCB — solo Linux |
| `KritaPyQtPluginFactory` | Scripting PyKrita — rimosso nella v1 (niente CPython dinamico) |
| `PDFImportFactory` | Import PDF (Poppler) — stack pesante, rimandato oltre la v1 |
| `QMicFactory` | G'MIC via processo esterno — iOS vieta i sottoprocessi |
| `SPenSettingsFactory` | Impostazioni S-Pen Samsung — hardware solo Android |
| `DbExplorerFactory` | Esploratore DB risorse — solo sviluppo, non distribuito |
| `KritaExampleFactory` | Filtro di esempio dell'SDK — non rivolto all'utente |

> Da rivalutare presto: `KisRawImportFactory` (libraw, pesante) e
> `RecorderDockerPluginFactory` (registrazione canvas su frame PNG, attento alla memoria)
> sono **importati** ma potrebbero essere disattivati per il primo bring-up.

## Inventario completo

| Factory | JSON metadata | Source |
|---|---|---|
| `AnimationDockersPluginFactory` | `krita_animationdocker.json` | [plugins/dockers/animation/KisAnimDockers.cpp](plugins/dockers/animation/KisAnimDockers.cpp) |
| `ArrangeDockerPluginFactory` | `krita_arrangedocker.json` | [plugins/dockers/arrangedocker/arrangedocker.cpp](plugins/dockers/arrangedocker/arrangedocker.cpp) |
| `BlurFilterPluginFactory` | `kritablurfilter.json` | [plugins/filters/blur/blur.cpp](plugins/filters/blur/blur.cpp) |
| `BugInfoFactory` | `kritabuginfo.json` | [plugins/extensions/buginfo/buginfo.cpp](plugins/extensions/buginfo/buginfo.cpp) |
| `CSVImportFactory` | `krita_csv_import.json` | [plugins/impex/csv/kis_csv_import.cpp](plugins/impex/csv/kis_csv_import.cpp) |
| `ChannelDockerPluginFactory` | `krita_channeldocker.json` | [plugins/dockers/channeldocker/channeldocker.cpp](plugins/dockers/channeldocker/channeldocker.cpp) |
| `ClonesArrayFactory` | `kritaclonesarray.json` | [plugins/extensions/clonesarray/clonesarray.cpp](plugins/extensions/clonesarray/clonesarray.cpp) |
| `ColorSelectorNgPluginFactory` | `krita_colorselectorng.json` | [plugins/dockers/advancedcolorselector/colorselectorng.cpp](plugins/dockers/advancedcolorselector/colorselectorng.cpp) |
| `ColorSmudgePaintOpPluginFactory` | `kritacolorsmudgepaintop.json` | [plugins/paintops/colorsmudge/colorsmudge_paintop_plugin.cpp](plugins/paintops/colorsmudge/colorsmudge_paintop_plugin.cpp) |
| `ColorsFiltersFactory` | `kritacolorsfilter.json` | [plugins/filters/colorsfilters/colorsfilters.cpp](plugins/filters/colorsfilters/colorsfilters.cpp) |
| `CompositionDockerPluginFactory` | `krita_compositiondocker.json` | [plugins/dockers/compositiondocker/compositiondocker.cpp](plugins/dockers/compositiondocker/compositiondocker.cpp) |
| `CurvePaintOpPluginFactory` | `kritacurvepaintop.json` | [plugins/paintops/curvebrush/curve_paintop_plugin.cpp](plugins/paintops/curvebrush/curve_paintop_plugin.cpp) |
| `DbExplorerFactory` | `kritadbexplorer.json` | [plugins/extensions/dbexplorer/DbExplorer.cpp](plugins/extensions/dbexplorer/DbExplorer.cpp) |
| `DefaultToolsFactory` | `kritatoolknife.json` | [plugins/tools/tool_knife/ToolKnife.cpp](plugins/tools/tool_knife/ToolKnife.cpp) |
| `DeformPaintOpPluginFactory` | `kritadeformpaintop.json` | [plugins/paintops/deform/deform_paintop_plugin.cpp](plugins/paintops/deform/deform_paintop_plugin.cpp) |
| `DodgeBurnPluginFactory` | `kritadodgeburn.json` | [plugins/filters/dodgeburn/DodgeBurnPlugin.cpp](plugins/filters/dodgeburn/DodgeBurnPlugin.cpp) |
| `ExperimentPaintOpPluginFactory` | `kritaexperimentpaintop.json` | [plugins/paintops/experiment/experiment_paintop_plugin.cpp](plugins/paintops/experiment/experiment_paintop_plugin.cpp) |
| `ExportFactory` | `krita_heif_export.json` | [plugins/impex/heif/HeifExport.cpp](plugins/impex/heif/HeifExport.cpp) |
| `FilterOpFactory` | `kritafilterop.json` | [plugins/paintops/filterop/filterop.cpp](plugins/paintops/filterop/filterop.cpp) |
| `GaussianHighPassPluginFactory` | `kritagaussianhighpassfilter.json` | [plugins/filters/gaussianhighpass/gaussianhighpass.cpp](plugins/filters/gaussianhighpass/gaussianhighpass.cpp) |
| `GridDockerPluginFactory` | `krita_griddocker.json` | [plugins/dockers/griddocker/griddocker.cpp](plugins/dockers/griddocker/griddocker.cpp) |
| `GridPaintOpPluginFactory` | `kritagridpaintop.json` | [plugins/paintops/gridbrush/grid_paintop_plugin.cpp](plugins/paintops/gridbrush/grid_paintop_plugin.cpp) |
| `HairyPaintOpPluginFactory` | `kritahairypaintop.json` | [plugins/paintops/hairy/hairy_paintop_plugin.cpp](plugins/paintops/hairy/hairy_paintop_plugin.cpp) |
| `HatchingPaintOpPluginFactory` | `kritahatchingpaintop.json` | [plugins/paintops/hatching/hatching_paintop_plugin.cpp](plugins/paintops/hatching/hatching_paintop_plugin.cpp) |
| `HeightMapImportFactory` | `krita_heightmap_import.json` | [plugins/impex/heightmap/kis_heightmap_import.cpp](plugins/impex/heightmap/kis_heightmap_import.cpp) |
| `HistogramDockerPluginFactory` | `krita_histogramdocker.json` | [plugins/dockers/histogram/histogramdocker.cpp](plugins/dockers/histogram/histogramdocker.cpp) |
| `HistoryPluginFactory` | `kritahistorydocker.json` | [plugins/dockers/historydocker/History.cpp](plugins/dockers/historydocker/History.cpp) |
| `ImageShapePluginFactory` | `krita_shape_image.json` | [plugins/flake/imageshape/ImageShapePlugin.cpp](plugins/flake/imageshape/ImageShapePlugin.cpp) |
| `ImagesplitFactory` | `kritaimagesplit.json` | [plugins/extensions/imagesplit/imagesplit.cpp](plugins/extensions/imagesplit/imagesplit.cpp) |
| `ImportFactory` | `krita_heif_import.json` | [plugins/impex/heif/HeifImport.cpp](plugins/impex/heif/HeifImport.cpp) |
| `IndexColorsFactory` | `kritaindexcolors.json` | [plugins/filters/indexcolors/indexcolors.cpp](plugins/filters/indexcolors/indexcolors.cpp) |
| `KarbonToolsPluginFactory` | `karbon_tools.json` | [plugins/tools/karbonplugins/tools/KarbonToolsPlugin.cpp](plugins/tools/karbonplugins/tools/KarbonToolsPlugin.cpp) |
| `KisBrushExportFactory` | `krita_brush_export.json` | [plugins/impex/brush/kis_brush_export.cpp](plugins/impex/brush/kis_brush_export.cpp) |
| `KisBrushImportFactory` | `krita_brush_import.json` | [plugins/impex/brush/kis_brush_import.cpp](plugins/impex/brush/kis_brush_import.cpp) |
| `KisCSVExportFactory` | `krita_csv_export.json` | [plugins/impex/csv/kis_csv_export.cpp](plugins/impex/csv/kis_csv_export.cpp) |
| `KisEmbossFilterPluginFactory` | `kritaembossfilter.json` | [plugins/filters/embossfilter/kis_emboss_filter_plugin.cpp](plugins/filters/embossfilter/kis_emboss_filter_plugin.cpp) |
| `KisExifIOPluginFactory` | `kritaexif.json` | [plugins/metadata/exif/kis_exif_plugin.cpp](plugins/metadata/exif/kis_exif_plugin.cpp) |
| `KisGIFExportFactory` | `krita_gif_export.json` | [plugins/impex/gif/kis_gif_export.cpp](plugins/impex/gif/kis_gif_export.cpp) |
| `KisGIFImportFactory` | `krita_gif_import.json` | [plugins/impex/gif/kis_gif_import.cpp](plugins/impex/gif/kis_gif_import.cpp) |
| `KisHeightMapExportFactory` | `krita_heightmap_export.json` | [plugins/impex/heightmap/kis_heightmap_export.cpp](plugins/impex/heightmap/kis_heightmap_export.cpp) |
| `KisIptcIOPluginFactory` | `kritaiptc.json` | [plugins/metadata/iptc/kis_iptc_plugin.cpp](plugins/metadata/iptc/kis_iptc_plugin.cpp) |
| `KisOilPaintFilterPluginFactory` | `kritaoilpaintfilter.json` | [plugins/filters/oilpaintfilter/kis_oilpaint_filter_plugin.cpp](plugins/filters/oilpaintfilter/kis_oilpaint_filter_plugin.cpp) |
| `KisPhongBumpmapFactory` | `kritaphongbumpmapfilter.json` | [plugins/filters/phongbumpmap/kis_phong_bumpmap_plugin.cpp](plugins/filters/phongbumpmap/kis_phong_bumpmap_plugin.cpp) |
| `KisPixelizeFilterPluginFactory` | `kritapixelizefilter.json` | [plugins/filters/pixelizefilter/kis_pixelize_filter_plugin.cpp](plugins/filters/pixelizefilter/kis_pixelize_filter_plugin.cpp) |
| `KisQImageIOExportFactory` | `krita_qimageio_export.json` | [plugins/impex/qimageio/kis_qimageio_export.cpp](plugins/impex/qimageio/kis_qimageio_export.cpp) |
| `KisQImageIOImportFactory` | `krita_qimageio_import.json` | [plugins/impex/qimageio/kis_qimageio_import.cpp](plugins/impex/qimageio/kis_qimageio_import.cpp) |
| `KisRGBEImportFactory` | `krita_rgbe_import.json` | [plugins/impex/rgbe/RGBEImport.cpp](plugins/impex/rgbe/RGBEImport.cpp) |
| `KisRainDropsFilterPluginFactory` | `kritaraindropsfilter.json` | [plugins/filters/raindropsfilter/kis_raindrops_filter_plugin.cpp](plugins/filters/raindropsfilter/kis_raindrops_filter_plugin.cpp) |
| `KisRawImportFactory` | `krita_raw_import.json` | [plugins/impex/raw/kis_raw_import.cpp](plugins/impex/raw/kis_raw_import.cpp) |
| `KisRoundCornersFilterPluginFactory` | `kritaroundcornersfilter.json` | [plugins/filters/roundcorners/kis_round_corners_filter_plugin.cpp](plugins/filters/roundcorners/kis_round_corners_filter_plugin.cpp) |
| `KisSampleScreenColorFactory` | `kritasamplescreencolor.json` | [plugins/extensions/samplescreencolor/KisSampleScreenColor.cpp](plugins/extensions/samplescreencolor/KisSampleScreenColor.cpp) |
| `KisSmallTilesFilterPluginFactory` | `kritasmalltilesfilter.json` | [plugins/filters/smalltilesfilter/kis_small_tiles_filter_plugin.cpp](plugins/filters/smalltilesfilter/kis_small_tiles_filter_plugin.cpp) |
| `KisSpriterExportFactory` | `krita_spriter_export.json` | [plugins/impex/spriter/kis_spriter_export.cpp](plugins/impex/spriter/kis_spriter_export.cpp) |
| `KisTGAExportFactory` | `krita_tga_export.json` | [plugins/impex/tga/kis_tga_export.cpp](plugins/impex/tga/kis_tga_export.cpp) |
| `KisTGAImportFactory` | `krita_tga_import.json` | [plugins/impex/tga/kis_tga_import.cpp](plugins/impex/tga/kis_tga_import.cpp) |
| `KisToolEncloseAndFillPluginFactory` | `kritatoolencloseandfill.json` | [plugins/tools/tool_enclose_and_fill/KisToolEncloseAndFillPlugin.cpp](plugins/tools/tool_enclose_and_fill/KisToolEncloseAndFillPlugin.cpp) |
| `KisWebPExportFactory` | `krita_webp_export.json` | [plugins/impex/webp/kis_webp_export.cpp](plugins/impex/webp/kis_webp_export.cpp) |
| `KisWebPImportFactory` | `krita_webp_import.json` | [plugins/impex/webp/kis_webp_import.cpp](plugins/impex/webp/kis_webp_import.cpp) |
| `KritaASCCDLFactory` | `` | [plugins/filters/asccdl/kis_asccdl_filter.cpp](plugins/filters/asccdl/kis_asccdl_filter.cpp) |
| `KritaColorGeneratorFactory` | `kritacolorgenerator.json` | [plugins/generators/solid/colorgenerator.cpp](plugins/generators/solid/colorgenerator.cpp) |
| `KritaConvertHeightToNormalMapFilterFactory` | `kritaconvertheighttonormalmap.json` | [plugins/filters/convertheightnormalmap/kis_convert_height_to_normal_map_filter.cpp](plugins/filters/convertheightnormalmap/kis_convert_height_to_normal_map_filter.cpp) |
| `KritaConvolutionFiltersFactory` | `kritaconvolutionfilters.json` | [plugins/filters/convolutionfilters/convolutionfilters.cpp](plugins/filters/convolutionfilters/convolutionfilters.cpp) |
| `KritaEdgeDetectionFilterFactory` | `kritaedgedetection.json` | [plugins/filters/edgedetection/kis_edge_detection_filter.cpp](plugins/filters/edgedetection/kis_edge_detection_filter.cpp) |
| `KritaExampleFactory` | `kritaexample.json` | [plugins/filters/example/example.cpp](plugins/filters/example/example.cpp) |
| `KritaExtensionsColorsFactory` | `kritaextensioncolorsfilters.json` | [plugins/filters/colors/colors.cpp](plugins/filters/colors/colors.cpp) |
| `KritaFastColorTransferFactory` | `kritafastcolortransfer.json` | [plugins/filters/fastcolortransfer/fastcolortransfer.cpp](plugins/filters/fastcolortransfer/fastcolortransfer.cpp) |
| `KritaGradientGeneratorFactory` | `KritaGradientGenerator.json` | [plugins/generators/gradient/KisGradientGeneratorPlugin.cpp](plugins/generators/gradient/KisGradientGeneratorPlugin.cpp) |
| `KritaGradientMapFilterFactory` | `KritaGradientMapFilter.json` | [plugins/filters/gradientmap/KisGradientMapFilterPlugin.cpp](plugins/filters/gradientmap/KisGradientMapFilterPlugin.cpp) |
| `KritaHalftoneFactory` | `KritaHalftone.json` | [plugins/filters/halftone/KisHalftoneFilter.cpp](plugins/filters/halftone/KisHalftoneFilter.cpp) |
| `KritaImageEnhancementFactory` | `kritaimageenhancement.json` | [plugins/filters/imageenhancement/imageenhancement.cpp](plugins/filters/imageenhancement/imageenhancement.cpp) |
| `KritaLayerDockerPluginFactory` | `kritalayerdocker.json` | [plugins/dockers/layerdocker/LayerDocker.cpp](plugins/dockers/layerdocker/LayerDocker.cpp) |
| `KritaMultigridPatternGeneratorFactory` | `kritamultigridpatterngenerator.json` | [plugins/generators/multigridpattern/multigridpatterngenerator.cpp](plugins/generators/multigridpattern/multigridpatterngenerator.cpp) |
| `KritaNoiseFilterFactory` | `kritanoisefilter.json` | [plugins/filters/noisefilter/noisefilter.cpp](plugins/filters/noisefilter/noisefilter.cpp) |
| `KritaNormalizeFilterFactory` | `kritanormalize.json` | [plugins/filters/normalize/kis_normalize.cpp](plugins/filters/normalize/kis_normalize.cpp) |
| `KritaPatternGeneratorFactory` | `kritapatterngenerator.json` | [plugins/generators/pattern/patterngenerator.cpp](plugins/generators/pattern/patterngenerator.cpp) |
| `KritaPlatformPluginWaylandFactory` | `kritaplatformwayland.json` | [plugins/platforms/wayland/KritaPlatformPluginWayland.cpp](plugins/platforms/wayland/KritaPlatformPluginWayland.cpp) |
| `KritaPlatformPluginXcbFactory` | `kritaplatformxcb.json` | [plugins/platforms/xcb/KritaPlatformPluginXcb.cpp](plugins/platforms/xcb/KritaPlatformPluginXcb.cpp) |
| `KritaPyQtPluginFactory` | `kritapykrita.json` | [plugins/extensions/pykrita/plugin/plugin.cpp](plugins/extensions/pykrita/plugin/plugin.cpp) |
| `KritaRandomPickFilterFactory` | `kritarandompickfilter.json` | [plugins/filters/randompickfilter/randompickfilter.cpp](plugins/filters/randompickfilter/randompickfilter.cpp) |
| `KritaScreentoneGeneratorFactory` | `KritaScreentoneGenerator.json` | [plugins/generators/screentone/KisScreentoneGeneratorPlugin.cpp](plugins/generators/screentone/KisScreentoneGeneratorPlugin.cpp) |
| `KritaSeExprGeneratorFactory` | `generator.json` | [plugins/generators/seexpr/generator.cpp](plugins/generators/seexpr/generator.cpp) |
| `KritaSimplexNoiseGeneratorFactory` | `kritasimplexnoisegenerator.json` | [plugins/generators/simplexnoise/simplexnoisegenerator.cpp](plugins/generators/simplexnoise/simplexnoisegenerator.cpp) |
| `KritaThresholdFactory` | `kritathreshold.json` | [plugins/filters/threshold/threshold.cpp](plugins/filters/threshold/threshold.cpp) |
| `KritaWaveFilterFactory` | `kritawavefilter.json` | [plugins/filters/wavefilter/wavefilter.cpp](plugins/filters/wavefilter/wavefilter.cpp) |
| `KrzExportFactory` | `krita_krz_export.json` | [plugins/impex/krz/krz_export.cpp](plugins/impex/krz/krz_export.cpp) |
| `LayerGroupSwitcherFactory` | `kritalayergroupswitcher.json` | [plugins/extensions/layergroupswitcher/layergroupswitcher.cpp](plugins/extensions/layergroupswitcher/layergroupswitcher.cpp) |
| `LayerSplitFactory` | `kritalayersplit.json` | [plugins/extensions/layersplit/layersplit.cpp](plugins/extensions/layersplit/layersplit.cpp) |
| `LevelsFilterFactory` | `kritalevelsfilter.json` | [plugins/filters/levelfilter/KisLevelsFilterPlugin.cpp](plugins/filters/levelfilter/KisLevelsFilterPlugin.cpp) |
| `LogDockerPluginFactory` | `` | [plugins/dockers/logdocker/LogDocker.cpp](plugins/dockers/logdocker/LogDocker.cpp) |
| `LutDockerPluginFactory` | `krita_lutdocker.json` | [plugins/dockers/lut/lutdocker.cpp](plugins/dockers/lut/lutdocker.cpp) |
| `MyPaintOpPluginFactory` | `kritamypaintop.json` | [plugins/paintops/mypaint/MyPaintPaintOpPlugin.cpp](plugins/paintops/mypaint/MyPaintPaintOpPlugin.cpp) |
| `OffsetImageFactory` | `kritaoffsetimage.json` | [plugins/extensions/offsetimage/offsetimage.cpp](plugins/extensions/offsetimage/offsetimage.cpp) |
| `OverviewDockerPluginFactory` | `krita_overviewdocker.json` | [plugins/dockers/overview/overviewdocker.cpp](plugins/dockers/overview/overviewdocker.cpp) |
| `PDFImportFactory` | `krita_pdf_import.json` | [plugins/impex/pdf/kis_pdf_import.cpp](plugins/impex/pdf/kis_pdf_import.cpp) |
| `PaletteDockPluginFactory` | `krita_artisticcolorselector.json` | [plugins/dockers/artisticcolorselector/artisticcolorselector_plugin.cpp](plugins/dockers/artisticcolorselector/artisticcolorselector_plugin.cpp) |
| `PaletteDockerPluginFactory` | `krita_palettedocker.json` | [plugins/dockers/palettedocker/palettedocker.cpp](plugins/dockers/palettedocker/palettedocker.cpp) |
| `PalettizeFactory` | `kritapalettize.json` | [plugins/filters/palettize/palettize.cpp](plugins/filters/palettize/palettize.cpp) |
| `ParticlePaintOpPluginFactory` | `kritaparticlepaintop.json` | [plugins/paintops/particle/particle_paintop_plugin.cpp](plugins/paintops/particle/particle_paintop_plugin.cpp) |
| `PathShapesPluginFactory` | `calligra_shape_paths.json` | [plugins/flake/pathshapes/PathShapesPlugin.cpp](plugins/flake/pathshapes/PathShapesPlugin.cpp) |
| `PatternDockerPluginFactory` | `krita_patterndocker.json` | [plugins/dockers/patterndocker/patterndocker.cpp](plugins/dockers/patterndocker/patterndocker.cpp) |
| `PluginFactory` | `calligra_tool_defaults.json` | [plugins/tools/defaulttool/Plugin.cpp](plugins/tools/defaulttool/Plugin.cpp) |
| `PosterizeFactory` | `kritaposterize.json` | [plugins/filters/posterize/posterize.cpp](plugins/filters/posterize/posterize.cpp) |
| `PresetDockerPluginFactory` | `krita_brushhud.json` | [plugins/dockers/brushhud/brushhuddocker.cpp](plugins/dockers/brushhud/brushhuddocker.cpp) |
| `PresetHistoryPluginFactory` | `krita_presethistory.json` | [plugins/dockers/presethistory/presethistory.cpp](plugins/dockers/presethistory/presethistory.cpp) |
| `PropagateColorsFilterFactory` | `kritapropagatecolorsfilter.json` | [plugins/filters/propagatecolors/KisPropagateColorsFilterPlugin.cpp](plugins/filters/propagatecolors/KisPropagateColorsFilterPlugin.cpp) |
| `QMicFactory` | `kritaqmic.json` | [plugins/extensions/qmic/QMic.cpp](plugins/extensions/qmic/QMic.cpp) |
| `RecorderDockerPluginFactory` | `krita_recorderdocker.json` | [plugins/dockers/recorder/recorderdocker.cpp](plugins/dockers/recorder/recorderdocker.cpp) |
| `ResetTransparentFactory` | `kritaresettransparent.json` | [plugins/filters/resettransparent/KisResetTransparentFilter.cpp](plugins/filters/resettransparent/KisResetTransparentFilter.cpp) |
| `ResourceManagerFactory` | `kritaresourcemanager.json` | [plugins/extensions/resourcemanager/resourcemanager.cpp](plugins/extensions/resourcemanager/resourcemanager.cpp) |
| `RoundMarkerPaintOpPluginFactory` | `kritaroundmarkerpaintop.json` | [plugins/paintops/roundmarker/roundmarker_paintop_plugin.cpp](plugins/paintops/roundmarker/roundmarker_paintop_plugin.cpp) |
| `SPenSettingsFactory` | `kritaspensettings.json` | [plugins/extensions/spensettings/SPenSettings.cpp](plugins/extensions/spensettings/SPenSettings.cpp) |
| `SketchPaintOpPluginFactory` | `kritasketchpaintop.json` | [plugins/paintops/sketch/sketch_paintop_plugin.cpp](plugins/paintops/sketch/sketch_paintop_plugin.cpp) |
| `SnapshotPluginFactory` | `kritasnapshotdocker.json` | [plugins/dockers/snapshotdocker/SnapshotPlugin.cpp](plugins/dockers/snapshotdocker/SnapshotPlugin.cpp) |
| `SprayPaintOpPluginFactory` | `kritaspraypaintop.json` | [plugins/paintops/spray/spray_paintop_plugin.cpp](plugins/paintops/spray/spray_paintop_plugin.cpp) |
| `StoryboardDockerPluginFactory` | `krita_storyboarddocker.json` | [plugins/dockers/storyboarddocker/StoryboardDocker.cpp](plugins/dockers/storyboarddocker/StoryboardDocker.cpp) |
| `TangentNormalPaintOpPluginFactory` | `kritatangentnormalpaintop.json` | [plugins/paintops/tangentnormal/kis_tangent_normal_paintop_plugin.cpp](plugins/paintops/tangentnormal/kis_tangent_normal_paintop_plugin.cpp) |
| `TasksetDockerPluginFactory` | `` | [plugins/dockers/tasksetdocker/tasksetdocker.cpp](plugins/dockers/tasksetdocker/tasksetdocker.cpp) |
| `TextPropertiesPluginFactory` | `krita_textproperties.json` | [plugins/dockers/textproperties/TextPropertiesPlugin.cpp](plugins/dockers/textproperties/TextPropertiesPlugin.cpp) |
| `ToolDynaFactory` | `kritatooldyna.json` | [plugins/tools/tool_dyna/tool_dyna.cpp](plugins/tools/tool_dyna/tool_dyna.cpp) |
| `TouchDockerPluginFactory` | `kritatouchdocker.json` | [plugins/dockers/touchdocker/TouchDocker.cpp](plugins/dockers/touchdocker/TouchDocker.cpp) |
| `UnsharpPluginFactory` | `kritaunsharpfilter.json` | [plugins/filters/unsharp/unsharp.cpp](plugins/filters/unsharp/unsharp.cpp) |
| `WGColorSelectorPluginFactory` | `krita_widegamutcolorselector.json` | [plugins/dockers/widegamutcolorselector/WGColorSelectorPlugin.cpp](plugins/dockers/widegamutcolorselector/WGColorSelectorPlugin.cpp) |
| `WaveletDecomposeFactory` | `kritawaveletdecompose.json` | [plugins/extensions/waveletdecompose/waveletdecompose.cpp](plugins/extensions/waveletdecompose/waveletdecompose.cpp) |
| `XCFImportFactory` | `krita_xcf_import.json` | [plugins/impex/xcf/kis_xcf_import.cpp](plugins/impex/xcf/kis_xcf_import.cpp) |

---
*Rigenera questo inventario eseguendo lo scan di `K_PLUGIN_FACTORY_WITH_JSON` sui sorgenti
dei plugin (stesso comando usato per produrre questa tabella) e aggiorna in parallelo
`KRITA_IOS_ALL_PLUGIN_FACTORIES` in [static-plugins.cmake](static-plugins.cmake).*
