# Dipendenze native di Krita per iOS

Inventario completo (dai `find_package` del sorgente) con ordine di build, sistema di
build e stato su iOS. Il superbuild in [3rdparty-ios/](3rdparty-ios/) implementa questo
DAG; le **versioni e le patch esatte** vanno allineate a
[krita-deps-management](https://invent.kde.org/packaging/krita-deps-management) (alcune
librerie usano fork/patch specifici di Krita — non fidarsi dei default upstream).

Legenda: 🟢 standard · 🟡 trascina sotto-dipendenze · ⚫ **escluso su iOS (v1)**

## Ordine di build (livelli)

**L0 — strumenti host (non cross-compilati):** ECM (Extra CMake Modules).

**L1 — base (nessuna dipendenza):**
| Pkg | Build | iOS |
|---|---|---|
| Eigen3 | CMake (header-only) | 🟢 |
| zlib | CMake | 🟢 |
| libjpeg-turbo | CMake | 🟢 |
| boost | b2/bootstrap | 🟡 (serve toolset clang-ios) |
| GSL | autotools/CMake | 🟢 |
| FFTW3 | CMake | 🟢 |
| xsimd | CMake (header-only, NEON arm64) | 🟢 |
| expat | CMake | 🟢 (per OCIO/exiv2) |
| Imath | CMake | 🟢 (per OpenEXR) |
| Immer, Lager, Zug | CMake (header-only) | 🟢 |

**L2 — codec immagine (dipendono da L1):**
| Pkg | Dipende da | iOS |
|---|---|---|
| libpng | zlib | 🟢 |
| libtiff | jpeg, zlib | 🟢 |
| giflib | — | 🟢 |
| libwebp | — | 🟢 |
| openjpeg | — | 🟢 |
| OpenEXR | Imath, zlib | 🟡 |
| libheif | libde265 (+x265) | 🟡 |
| libjxl (JPEG-XL) | brotli, highway | 🟡 |
| LittleCMS (lcms2) | jpeg, tiff | 🟢 |

**L3 — testo:**
| Pkg | Dipende da | iOS |
|---|---|---|
| freetype | zlib, png | 🟢 |
| harfbuzz | freetype | 🟢 |
| fribidi | — | 🟢 |
| libunibreak | — | 🟢 |
| fontconfig | freetype, expat | 🟡 (su iOS niente config di sistema: serve un fonts.conf minimale nel bundle) |

**L4 — colore / krita-specifiche:**
| Pkg | Dipende da | iOS |
|---|---|---|
| OpenColorIO | expat, yaml-cpp, pystring, minizip-ng | 🟡 |
| libmypaint | json-c | 🟢 |
| quazip | zlib (+Qt) | 🟢 |
| KSeExpr | — | 🟢 |
| exiv2 | expat | 🟢 |
| libraw (KDcraw) | jpeg, lcms2 | 🟡 |

**L5 — KDE Frameworks (dopo Qt-for-iOS patchato):**
KConfig, KCoreAddons, KWidgetsAddons, KCompletion, KGuiAddons, KI18n, KItemViews,
KItemModels, KColorScheme (Qt6). Build CMake standard, dipendono da ECM + Qt.

## Esclusi su iOS (v1) ⚫

| Pkg | Motivo |
|---|---|
| Python, SIP, PyQt5/6 | Scripting PyKrita rimosso nella v1 (niente CPython dinamico) |
| Poppler | Import PDF — stack pesante, rimandato |
| MLT (Mlt7) | Render video/animazione — pesante |
| SDL2 | Audio per animazione — opzionale, rimandato |
| libunwindstack | Unwinding crash Android |
| X11 / Xinput, Wayland | Solo Linux (già esclusi: il loro blocco è `NOT APPLE`) |

## Note per iOS

- **Solo statico**: tutte con `BUILD_SHARED_LIBS=OFF` (vedi il toolchain). Niente `.dylib`.
- **Due architetture**: ogni libreria va compilata per `OS64` (device) e `SIMULATORARM64`
  (Simulator), poi unite in `.xcframework` da [build-deps.sh](build-deps.sh).
- **boost**: non usa CMake per il build; serve un `user-config.jam` con un toolset
  `clang-darwin` che punta a `xcrun --sdk iphoneos clang` e i flag `-arch arm64
  -miphoneos-version-min=...`. È la dipendenza più scomoda del livello L1.
- **fontconfig**: su iOS non esiste `/etc/fonts`; impacchettare un `fonts.conf` minimale
  nel bundle e puntarci `FONTCONFIG_PATH` (come già fa il ramo Android in `main.cc`).
