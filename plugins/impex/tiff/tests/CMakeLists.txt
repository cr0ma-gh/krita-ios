if (APPLE AND IOS)
    # Evita duplicazione su build iOS statica
    set(KRITA_JPEG_IMPORT_ONLY ON)
    # oppure
    # remove_definitions(-Dkrita_jpeg_export_EXPORTS)
endif()

kis_add_test(
    kis_tiff_test.cpp
    LINK_LIBRARIES kritaui kritatestsdk
    )

