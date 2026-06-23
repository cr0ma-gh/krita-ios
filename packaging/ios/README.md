# Krita per iOS / iPadOS — packaging

Impalcatura di **Fase 0** del port iPadOS. Per il quadro completo (fattibilità, blocchi,
roadmap a fasi, stime) vedi **[../../README.ios.md](../../README.ios.md)**.

> ⚠️ Questo **non** è ancora un'app funzionante. È l'ossatura cross-platform che si può
> scrivere su Windows; la compilazione vera gira su **macOS** (vedi pipeline CI).

## Contenuto di questa cartella

| File | Ruolo |
|---|---|
| [Info.plist.in](Info.plist.in) | Template del bundle `.app` (id, orientamenti, tipi documento `.kra`, accesso file, Apple Pencil 120 Hz). Processato con `configure_file()`. |
| [LaunchScreen.storyboard](LaunchScreen.storyboard) | Schermata di avvio minima (icona centrata). |
| [Assets.xcassets/](Assets.xcassets) | Catalogo icona app (fornire `AppIcon-1024.png`). |
| [static-plugins.cmake](static-plugins.cmake) | Denylist + snapshot dei 124 plugin + generatore del codice di registrazione statica. |
| [KisStaticPluginsInit.cpp.in](KisStaticPluginsInit.cpp.in) | Template della TU con i `Q_IMPORT_PLUGIN`. |
| [plugins-audit.md](plugins-audit.md) | Inventario completo dei plugin e analisi delle esclusioni. |
| [3rdparty-ios/](3rdparty-ios) | Superbuild `ExternalProject` delle dipendenze native per iOS. |
| [dependencies.md](dependencies.md) | DAG completo delle dipendenze, sistemi di build, drop list. |
| [build-deps.sh](build-deps.sh) | Driver (macOS) che compila il superbuild per una piattaforma. |
| [build-krita-ios.sh](build-krita-ios.sh) | Orchestratore end-to-end: deps → configure → build → `.ipa`. |
| [ci.md](ci.md) | Come eseguire la build su GitHub Actions, self-hosted, CircleCI o in locale. |

File correlati fuori da qui:
- [../../cmake/modules/ios.toolchain.cmake](../../cmake/modules/ios.toolchain.cmake) — toolchain CMake iOS.
- [../../.github/workflows/ios.yml](../../.github/workflows/ios.yml) — build su runner macOS.

## Flusso di build (sul Mac / CI)

```sh
# 1. Dipendenze native cross-compilate (Fase 0, superbuild parziale)
bash packaging/ios/build-deps.sh OS64 "$DEPS_PREFIX" 16.0

# 2. Configura Krita per iOS
cmake -S . -B build-ios -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/modules/ios.toolchain.cmake \
  -DPLATFORM=OS64 -DDEPLOYMENT_TARGET=16.0 \
  -DKRITA_DEPS_INSTALL_PREFIX=$DEPS_PREFIX \
  -DBUILD_WITH_QT6=ON -DALLOW_UNSTABLE=QT6

# 3. Compila
cmake --build build-ios --parallel
```

`PLATFORM`: `OS64` (iPad reale) oppure `SIMULATORARM64` (Simulator su Mac Apple silicon).

## Registrazione statica dei plugin

In `krita/CMakeLists.txt`, dietro un ramo iOS, generare e includere la TU:

```cmake
if(IOS)
    include(${CMAKE_SOURCE_DIR}/packaging/ios/static-plugins.cmake)
    krita_ios_generate_plugin_imports(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/KisStaticPluginsInit.cpp)
    target_sources(krita PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/KisStaticPluginsInit.cpp)
endif()
```

Ogni plugin va compilato con `QT_STATICPLUGIN` e linkato con `--whole-archive` (o linkage
OBJECT/STATIC che preserva i simboli di auto-registrazione). `KoJsonTrader`
([../../libs/koplugin/KoJsonTrader.cpp](../../libs/koplugin/KoJsonTrader.cpp)) ha già un
ramo "mobile" da estendere al caso "solo statico".

## Dal `.ipa` all'iPad senza Mac

Il workflow CI produce un `krita-unsigned-OS64.ipa`. Per installarlo sul tuo iPad senza
possedere un Mac: **SideStore / AltStore** firmano con il tuo Apple ID gratuito
direttamente dal dispositivo (refresh ogni 7 giorni). Vedi [../../README.ios.md](../../README.ios.md) §6.

## Stato

**Fatto (autorato su Windows, da compilare sul Mac CI):**
- Toolchain iOS + variabile `IOS`.
- Ramo `if(IOS)` in `krita/CMakeLists.txt`: bundle `.app`, `Info.plist` configurato,
  LaunchScreen/Assets come risorse, generazione registrazione statica plugin. I rami
  macOS (`.icns`, `actool`, `krita_version`) sono guardati con `AND NOT IOS`.
- `main.cc`: `AA_DontUseNativeMenuBar` esteso a iOS.
- Shim nativi in `libs/ui/`: [KisIOSFileProxy.mm](../../libs/ui/KisIOSFileProxy.mm)
  (copia dei file security-scoped del picker nella sandbox + bookmark) e
  [input/KisIOSTabletBridge.mm](../../libs/ui/input/KisIOSTabletBridge.mm) (cattura
  Apple Pencil a 120 Hz via gesture recognizer non invasivo → sink C++), agganciati in
  `libs/ui/CMakeLists.txt` sotto `if(IOS)`.
- **Moduli desktop resi sicuri per iOS.** Verifica: `KWindowSystem` non è usato da Krita;
  X11/KCrash/DBus sono già esclusi perché il loro blocco è `if(NOT WIN32 AND NOT APPLE
  AND NOT ANDROID AND NOT HAIKU)` e iOS è APPLE. Il vero rischio era il codice **macOS**
  sotto `APPLE` (che include iOS): `libs/macosutils` (AppKit) e i link relativi ora sono
  guardati con `AND NOT IOS` (in `libs/CMakeLists.txt`, `libs/ui/CMakeLists.txt`,
  `CMakeLists.txt`), e su iOS si linkano invece i framework `Foundation`+`UIKit`. Tutto
  l'uso di `KisMacos*` nel codice è già dietro `#ifdef Q_OS_MACOS` (non definito su iOS),
  quindi non servono stub di sorgente.

**Da fare (Fase 0 → 3):**
1. Completare il superbuild [3rdparty-ios/](3rdparty-ios) con tutto il DAG di
   [dependencies.md](dependencies.md), allineando versioni/patch a `krita-deps-management`.
2. Sostituire il Qt stock con un **Qt-for-iOS patchato** (patch di Krita).
3. Collegare il sink di `KisIOSTabletBridge` alla sintesi di `QTabletEvent` per
   `KisInputManager`, e chiamare `KisIOSFileProxy` da `KisImportExportManager`.
