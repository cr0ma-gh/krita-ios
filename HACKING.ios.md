# HACKING — Krita su iOS/iPadOS: stato del build e come riprendere

Companion operativo di [README.ios.md](README.ios.md). Documenta **dove è arrivato il
build su CI**, ogni problema risolto (e come), il **muro attuale** e la strada davanti.
Pensato per essere ripreso da te o da chi ha un Mac, senza ripartire da zero.

Repo CI: `github.com/cr0ma-gh/krita-ios`, branch `main`. Runner: GitHub Actions `macos-14`,
gratis per repo pubblici. Qt **6.9.1** (host+iOS via `install-qt-action`).

---

## Stato attuale (dopo 18 run CI, 21 commit)

✅ **Funziona end-to-end:** pipeline CI, superbuild dipendenze, lettura diagnostica.
✅ **Compilano e si installano per iOS (arm64):** zlib, eigen, libpng, libjpeg-turbo,
   lcms2, ECM, **proxy-libintl**, e **5 dei 7 KF6 framework**: KCoreAddons, KGuiAddons,
   KI18n (+ KItemViews/KWidgetsAddons in corso).
⛔ **Muro attuale:** `KConfig` → il suo generatore `kconfig_compiler_kf6` è un **tool host**
   ma viene compilato per iOS e non linka (`_qt_main_wrapper` undefined). Vedi §4.

Restano poi: completare i 7 framework, **~30 altre dipendenze**, la **compilazione di
Krita** (enorme), il **link statico** dei plugin. Realisticamente settimane.

---

## 1. Il loop di lavoro (come ho proceduto, come riprendere)

1. Modifica i sorgenti / `packaging/ios/3rdparty-ios/CMakeLists.txt`.
2. `git push` su `main` → parte il workflow `iOS build`.
3. Il workflow esegue [packaging/ios/build-krita-ios.sh](packaging/ios/build-krita-ios.sh)
   e **carica l'output come artifact `build.log`** (i log del job richiedono auth, gli
   artifact no).
4. Scarica e leggi `build.log`, trova il primo `FAILED:` / `error:`, correggi, ripeti.

Lettura artifact via API (per un repo pubblico la *lista* run è anonima; scaricare
artifact/log richiede un token — usa quello già in cache di git):
```bash
tok=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill | sed -n 's/^password=//p')
# ultimo run su main:
rid=$(curl -sL -H "Authorization: Bearer $tok" \
  "https://api.github.com/repos/cr0ma-gh/krita-ios/actions/runs?branch=main&per_page=1" \
  | grep -o '"id": [0-9]*' | head -1 | grep -o '[0-9]*')
aurl=$(curl -sL -H "Authorization: Bearer $tok" \
  "https://api.github.com/repos/cr0ma-gh/krita-ios/actions/runs/$rid/artifacts" \
  | grep -o '"archive_download_url": "[^"]*"' | head -1 | sed 's/.*: "//; s/"$//')
curl -sL -H "Authorization: Bearer $tok" "$aurl" -o bl.zip && unzip -o bl.zip
```
> Nota ambiente agent: github.com è raggiungibile solo con la sandbox dei comandi
> disabilitata; l'API ha rate-limit 60/h da anonimo (usa il token).

---

## 2. Problemi risolti (il percorso, per non rifarli)

| # | Sintomo | Causa | Fix |
|---|---|---|---|
| 1 | `ext_tiff` target inesistente | dep dichiarata ma non definita | rimossa da `ext_lcms2` |
| 2 | zlib 404 | `zlib.net` sposta i vecchi in `fossils/` | URL release GitHub |
| 3 | lcms2 "no CMakeLists" | usa autotools | helper `krita_ios_ext_autotools` |
| 4 | Python REQUIRED | PyKrita | `elseif(NOT IOS)` su `find_package(PythonLibrary)` |
| 5 | ECM non trovato | non nel superbuild | aggiunto `ext_ecm` |
| 6 | `ECMQueryQt: no qtpaths` | usa target `Qt6::qtpaths` (non ancora presente) | `-DQUERY_EXECUTABLE=$QT_HOST_PATH/bin/qtpaths` |
| 7 | download collisi `v6.5.0.tar.gz` | i tag GitHub hanno lo stesso basename | `DOWNLOAD_NAME ${NAME}.tar.gz` |
| 8 | KF6 non trovano ECM/Qt | `FIND_ROOT_PATH_MODE_PACKAGE ONLY` | passare `KRITA_DEPS_INSTALL_PREFIX`+`QT_IOS_ROOT`+`QT_HOST_PATH` ai sub-build |
| 9 | "platform iOS non supportata" | allowlist KDE | `-DKF_IGNORE_PLATFORM_CHECK=ON` |
| 10 | `_resources_N not in export set` | Qt statico + export KDE (irrisolto nelle release) | **bump a KF master** (+ Qt 6.9) |
| 11 | kwidgetsaddons/kguiaddons QProcess | release senza guardie iOS | le ha **solo master** |
| 12 | ki18n `LibIntl` REQUIRED | niente gettext su iOS | **proxy-libintl** stub locale |
| 13 | kcoreaddons `ksandbox.h` QProcess | plugin **QML** del framework | `-DKCOREADDONS_USE_QML=OFF`, `-DKCONFIG_USE_QML=OFF` |

Decisione architetturale chiave (#10/#11): **il supporto iOS dei KDE Frameworks esiste
solo in `master` (futuro 6.28), che richiede Qt ≥ 6.9.** Nessuna release lo ha.

---

## 3. Il muro attuale: host-tooling cross-compile (KConfig)

`kconfig_compiler_kf6` genera C++ dai file `.kcfg` **a build time**: deve essere un
eseguibile **host**. Il cross-build lo compila per iOS → non linka (`_qt_main_wrapper`)
e comunque non girerebbe sull'host. KConfig (anche master) fa `add_subdirectory(kconfig_compiler)`
**incondizionato**, senza modo di importarlo. Lo stesso vale per altri tool KDE
(`desktoptojson`, i tool di KI18n) e **Krita stessa** usa `kconfig_compiler` sui propri `.kcfg`.

**Soluzione (build a due passi, come fa KDE Craft / l'SDK Android):**
1. Build **nativo host** (macOS, niente toolchain iOS) di ECM + KConfig (+ KCoreAddons,
   KI18n) → installa in un `HOST_PREFIX`; ottieni `kconfig_compiler_kf6` eseguibile.
2. Nel cross-build, far sì che i framework e Krita **usino i tool host** invece di
   ricostruirli: tipicamente patchando i framework perché, se `CMAKE_CROSSCOMPILING`,
   importino `KF6::kconfig_compiler` dal `HOST_PREFIX` (e non aggiungano la subdir del tool).
3. Mettere `HOST_PREFIX/bin` nel PATH; il toolchain ha già `FIND_ROOT_PATH_MODE_PROGRAM BOTH`.

È un sotto-progetto infrastrutturale, non un fix puntuale: per questo il loop "leggi log →
fix" si ferma qui in modo pulito.

---

## 4. Roadmap residua (ordine)

1. **Host-tooling** (§3) — sblocca KConfig, KCompletion e poi Krita.
2. Completare i 7 framework (KItemViews, KWidgetsAddons, KCompletion) — probabili altri
   `QProcess`/QML/DBus da disattivare, già guardati in master dove possibile.
3. **~30 dipendenze** (vedi [packaging/ios/dependencies.md](packaging/ios/dependencies.md)):
   boost, OpenEXR(+Imath), OpenColorIO, exiv2, libraw, mypaint(+json-c), quazip, fftw,
   gsl, freetype, harfbuzz, fontconfig (config minimale), fribidi, libunibreak, webp,
   openjpeg, giflib, jpegxl, heif, tiff. Mista CMake/autotools (helper già pronti).
   Scartate su iOS: python/sip/pyqt, poppler, mlt, sdl2, x11/wayland, unwindstack.
4. **Configure di Krita** completo (tutte le `find_package` soddisfatte).
5. **Compilazione di Krita** — milione di righe; molti rami `#ifdef Q_OS_IOS` da aggiungere
   (gli shim e le guardie già fatti sono in `libs/ui/KisIOS*`, `libs/ui/input/KisIOS*`,
   `krita/CMakeLists.txt`, vedi [packaging/ios/README.md](packaging/ios/README.md)).
6. **Link statico** dei 116 plugin (`packaging/ios/static-plugins.cmake`) con
   `--whole-archive` per preservare i simboli di auto-registrazione.
7. `.ipa` → iPad via **SideStore/AltStore** (firma con Apple ID gratuito, no Mac).

## 5. Pin di riproducibilità (da fissare presto)

I framework usano `refs/heads/master.tar.gz` (non riproducibile). Appena KF **6.28** è
taggato, sostituire con `refs/tags/v6.28.0` per build stabili. Analogamente pinnare ECM.
