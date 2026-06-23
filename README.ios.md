# Porting di Krita su iPadOS — Roadmap di fattibilità

> Documento di analisi e pianificazione. **Non** è una guida a un port già funzionante:
> a oggi **non esiste** un port iOS/iPadOS ufficiale o community di Krita. Questo file
> descrive cosa servirebbe per costruirne uno, con stime, blocchi e un piano a fasi.
>
> Contesto di chi pianifica: macchina di sviluppo **Windows 11**, nessun Mac,
> obiettivo **uso personale / sideload** (quindi nessun vincolo di licenza GPL — vedi §2).
>
> Redatto: 2026-06. Basato su analisi del sorgente `krita-master`.

---

## Indice

1. [Verdetto in breve](#1-verdetto-in-breve)
2. [I tre vincoli duri](#2-i-tre-vincoli-duri)
3. [Strategia: il port Android è il modello](#3-strategia-il-port-android-è-il-modello)
4. [Analisi di fattibilità per sottosistema](#4-analisi-di-fattibilità-per-sottosistema)
5. [Matrice delle dipendenze da portare](#5-matrice-delle-dipendenze-da-portare)
6. [Pipeline di build senza Mac](#6-pipeline-di-build-senza-mac)
7. [Roadmap a fasi con stime](#7-roadmap-a-fasi-con-stime)
8. [Cosa si può iniziare subito su Windows](#8-cosa-si-può-iniziare-subito-su-windows)
9. [Rischi e domande aperte](#9-rischi-e-domande-aperte)

---

## 1. Verdetto in breve

**Fattibile sul piano tecnico, ma è un progetto da mesi, non da giorni.** Krita è una
delle più grandi applicazioni Qt esistenti (~1M+ righe C++, **222** moduli plugin, ~50
dipendenze native). Tre fatti however rendono il port *più* trattabile di quanto sembri:

- Il canvas **supporta già OpenGL ES** (path Android/ANGLE già nel codice).
- Esiste già un **layer UI touch in QtQuick** (`qmlmodules/`).
- Il **port Android** ha già introdotto codepath "mobile" in ~58 file: il port iOS in
  larga parte li rispecchia con un ramo `#ifdef Q_OS_IOS` accanto a `Q_OS_ANDROID`.

Stima realistica per uno sviluppatore solo, part-time: **9–18 mesi** per arrivare a
un'app sideloadabile, usabile e stabile. Un primo "si avvia e disegna su una tela" è
raggiungibile molto prima (vedi Fase 0–2). La distribuzione su App Store pubblico è
**bloccata** da licenza (vedi §2), ma per il tuo obiettivo (sideload personale) è
irrilevante.

---

## 2. I tre vincoli duri

### 2.1 Non si compila iOS su Windows
Xcode, l'SDK iOS, `clang` per Apple, la firma del codice, il Simulator e la generazione
di `.ipa`/`.xcframework` **esistono solo su macOS**. Su Windows si può:
- scrivere e modificare **tutto** il codice C++/CMake/Objective-C++ (portabile);
- preparare toolchain file, progetto Xcode, script di build, shim nativi;
- **non** si può eseguire il compilatore iOS né produrre un binario eseguibile.

→ La build vera gira su un **runner macOS in cloud** (GitHub Actions). Vedi §6 per la
pipeline Mac-free end-to-end, incluso il sideload via **SideStore/AltStore** (firma con
il tuo Apple ID *dall'iPad stesso*, senza possedere un Mac).

### 2.2 Solo link statico (niente plugin dinamici)
iOS **vieta** `dlopen`/JIT e il caricamento di codice eseguibile non firmato. Krita oggi
carica i plugin dinamicamente con `QPluginLoader` ([KoPluginLoader.cpp](libs/koplugin/KoPluginLoader.cpp))
e li scopre via `KoJsonTrader` ([KoJsonTrader.cpp](libs/koplugin/KoJsonTrader.cpp)).
Su iOS **tutti i 222 plugin** vanno linkati staticamente e registrati con
`Q_IMPORT_PLUGIN` + i metadati JSON embedded. Il path Android di `KoJsonTrader` è il
punto di partenza, ma su iOS va portato all'estremo (zero `.so`). Questo è il lavoro di
ingegneria *centrale* del port.

### 2.3 GPL vs App Store (non blocca il sideload)
Krita è **GPLv3**, considerata incompatibile con le ToS dell'App Store (disputa storica
VLC/FSF). Distribuzione pubblica su App Store → **impossibile** senza un'eccezione di
licenza dai detentori dei diritti (Krita Foundation/KDE).
**Per uso personale / sideload il problema non esiste**: la GPL consente di compilare e
installare sul proprio dispositivo liberamente.

---

## 3. Strategia: il port Android è il modello

Krita è già un codebase "mobile-aware". I file con codepath specifici per Android sono il
**modello esatto** dei punti che iOS deve gestire. Strategia: per ognuno, aggiungere un
ramo `#if defined(Q_OS_IOS)` accanto a quello Android, oppure un sibling nativo.

File chiave (analizzati nel sorgente) e relativo lavoro iOS:

| Area | File Android esistente | Lavoro iOS |
|---|---|---|
| Accesso file sandbox | `libs/ui/KisAndroidFileProxy.cpp` | Nuovo `KisIOSFileProxy` su `UIDocumentPicker` + security-scoped bookmarks |
| Menu contestuale touch | [KisLongPressEventFilter.cpp](libs/ui/KisLongPressEventFilter.cpp) | Riutilizzabile quasi as-is (touch = touch) |
| Scoperta plugin | [KoJsonTrader.cpp](libs/koplugin/KoJsonTrader.cpp) | Estendere il path mobile a "solo statico" |
| Shell applicazione | [KisApplication.cpp](libs/ui/KisApplication.cpp), [KisMainWindow.cpp](libs/ui/KisMainWindow.cpp) | Ramo `Q_OS_IOS`: window singola, niente menubar nativa |
| Path risorse | [KoResourcePaths.cpp](libs/resources/KoResourcePaths.cpp) | Bundle `.app` read-only + Documents scrivibile |
| Font UI | `libs/ui/KisUiFont.cpp` | Font di sistema iOS (San Francisco) |
| Scroll cinetico | [KisKineticScroller.cpp](libs/widgetutils/KisKineticScroller.cpp) | Riutilizzabile |
| Dialoghi file | [KoFileDialog.cpp](libs/widgetutils/KoFileDialog.cpp) | Bridge a `UIDocumentPicker` |
| Logging/crash | `libs/global/KisAndroidLogHandler.cpp`, `KisUsageLogger.cpp` | `os_log` di Apple; rimuovere `unwindstack` (solo Android) |
| Canvas GL ES | [kis_opengl.cpp](libs/ui/opengl/kis_opengl.cpp) | Forzare `RendererOpenGLES`; valutare Metal via Qt RHI (Qt6) |
| UI popup touch | [qmlmodules/](qmlmodules/) (`KisQmlPopupWidgetManager`) | Riutilizzabile come base UI tablet |

L'elenco completo dei ~58 file con `Q_OS_ANDROID` è la checklist di partenza per i
codepath `Q_OS_IOS`.

---

## 4. Analisi di fattibilità per sottosistema

Legenda difficoltà: 🟢 facile · 🟡 medio · 🔴 difficile · ⚫ da rimuovere nella v1

| Sottosistema | Difficoltà | Note |
|---|---|---|
| Build Qt per iOS (con patch Krita) | 🔴 | Qt supporta iOS, ma serve build da sorgente con le patch di Krita, **su Mac** |
| Cross-compile dipendenze native | 🔴 | ~40 lib via `krita-deps-management`; serve toolchain iOS per ciascuna |
| Staticizzazione 222 plugin | 🔴 | Il cuore del port; `Q_IMPORT_PLUGIN` + registrazione statica `KoJsonTrader` |
| Canvas / rendering | 🟡 | GL ES già supportato; tuning per il tiled renderer e i limiti GPU iPad |
| Input Apple Pencil / touch | 🟡 | Bridge nativo `UITouch`/`UIPencilInteraction` → `KisInputManager`; pressione, tilt, palm rejection, double-tap |
| Accesso file / sandbox | 🟡 | `UIDocumentPicker`, bookmark, niente filesystem arbitrario |
| Ciclo di vita app / memoria | 🟡 | Suspend/resume, `didReceiveMemoryWarning` (iPad ha limiti RAM duri) |
| Adattamento UI desktop→tablet | 🔴 | Docker, menu, hover, target piccoli: il lavoro più aperto e lungo |
| PyKrita / scripting Python | ⚫ | Rimuovere nella v1 (CPython 3.13 supporta iOS, ma i moduli C dinamici e i plugin script no) |
| Import PDF (Poppler) | ⚫ | Stack pesante; rimandare a versione successiva |
| Render animazione (MLT/SDL2) | ⚫/🟡 | MLT è pesante; SDL2 supporta iOS. Rimandare l'export video |
| Stampa, WinTab, X11/Wayland | ⚫ | Non applicabili su iOS |

---

## 5. Matrice delle dipendenze da portare

Dipendenze reali rilevate dai `find_package` nel sorgente. Quasi tutte sono portabili
con un toolchain iOS; le poche dolorose sono marcate.

**Portabili senza patch significative (🟢):**
Eigen3, Boost, Lager/Zug/Immer (header-only), zlib, libpng, libjpeg(-turbo), libtiff,
OpenEXR, OpenJPEG, WebP, GIF, JPEG-XL, HEIF, LCMS2, GSL, FFTW3, xsimd (path NEON arm64),
HarfBuzz, FreeType, FriBidi, libunibreak, QuaZip, LibMyPaint, KSeExpr.

**Portabili ma fastidiose (🟡):**
- **OpenColorIO** — trascina yaml-cpp/expat/pystring; buildabile ma laborioso.
- **Fontconfig** — concetto desktop; su iOS serve un workaround (config minimale o bypass).
- **SDL2** — supporta iOS, ma serve solo se si vuole audio per l'animazione.
- **KDE Frameworks** (KConfig, KCoreAddons, KWidgetsAddons, KCompletion, KGuiAddons,
  KI18n, KItemModels, KItemViews, KWindowSystem) — per lo più portabili; `KWindowSystem`
  e `KCrash` sono desktop-centrici e vanno stubbati/esclusi.

**Da rimuovere nella v1 (⚫):**
Python + SIP + PyQt (scripting), Poppler (PDF), MLT (render video), libunwindstack
(crash Android), X11 / Wayland / Qt private Wayland (Linux).

**Toolchain (host-side, ok su qualsiasi OS):** ECM (Extra CMake Modules).

> La sorgente che builda le dipendenze è il repo separato
> **`https://invent.kde.org/packaging/krita-deps-management`** (vedi `3rdparty/README.md`).
> Il port iOS richiede di aggiungere un profilo/toolchain iOS a quel sistema, oppure
> ricostruirne uno parallelo con un toolchain CMake iOS.

---

## 6. Pipeline di build senza Mac

Obiettivo: end-to-end **senza possedere un Mac**, partendo da Windows.

```
[Windows: scrivi codice/CMake/shim]
        │  git push
        ▼
[GitHub Actions runner macos-latest (Apple silicon)]
   1. installa Xcode + CMake + Ninja
   2. build Qt-for-iOS con patch Krita (cache)
   3. build dipendenze → .xcframework (cache)
   4. build Krita statica → krita.app / .ipa (unsigned o dev-signed)
   5. upload artifact (.ipa)
        │  scarica .ipa
        ▼
[iPad: SideStore/AltStore firma con il tuo Apple ID e installa]
   - refresh ogni 7 giorni (limite account gratuito Apple)
```

Note pratiche:
- **Runner macOS**: GitHub Actions offre `macos-14`/`macos-latest` (Apple silicon).
  Gratuito per repo pubblici; per privati ~2000 min/mese inclusi.
- **Simulator vs device**: il Simulator (`arm64-apple-ios-simulator`) gira solo su Mac,
  quindi non aiuta su Windows. L'unico modo di *vedere* l'app girando senza Mac è
  installarla su un **iPad fisico** via SideStore.
- **SideStore/AltStore**: permette di sideloadare un `.ipa` non firmato sul *proprio*
  iPad usando solo un Apple ID gratuito, senza Mac. È il pezzo che chiude il cerchio col
  vincolo "solo Windows".
- **Firma**: per un Apple ID gratuito l'app scade ogni 7 giorni e va riaperta SideStore
  per il refresh. Un account Developer a pagamento (99$/anno) estende a 1 anno e toglie
  vari limiti.

---

## 7. Roadmap a fasi con stime

Stime per **uno sviluppatore solo** (range ampio: dipende dall'esperienza con Qt/CMake/iOS).

### Fase 0 — Harness di build & dipendenze · 🔴 *2–4 mesi*
- Toolchain CMake iOS (`ios.toolchain.cmake`), arch `arm64-iphoneos`.
- Build di Qt-for-iOS con le patch Krita.
- Cross-compile incrementale delle dipendenze §5 in `.xcframework`.
- Skeleton progetto Xcode + CI GitHub Actions macOS.
- **Milestone:** le dipendenze e Qt compilano e linkano per iOS in CI.

### Fase 1 — Link statico & primo avvio · 🔴 *2–3 mesi*
- Convertire i 222 plugin in librerie statiche; `Q_IMPORT_PLUGIN`.
- Adattare `KoJsonTrader`/`KoPluginLoader` alla scoperta statica.
- Stub/escludere KWindowSystem, KCrash, X11, Python, Poppler, MLT.
- `main.cc` + bundle risorse iOS.
- **Milestone:** l'app si avvia sul Simulator/iPad e mostra la UI (anche se rotta).

### Fase 2 — Canvas che disegna · 🟡 *1–2 mesi*
- Forzare il renderer GL ES; far funzionare il tiled canvas.
- Far girare un brush engine di base su un layer.
- **Milestone:** si crea un documento e si disegna con un dito. *Primo traguardo "wow".*

### Fase 3 — Input Apple Pencil/touch nativo · 🟡 *1–2 mesi*
- Shim Objective-C++ `UITouch`/`UIPencilInteraction` → `KisInputManager`.
- Pressione, tilt, azimuth, palm rejection, double-tap (Pencil 2/Pro).
- Gesti: pan/zoom/rotate a due dita, undo a tre dita.
- **Milestone:** disegno con Pencil con pressione e gesti.

### Fase 4 — File, sandbox, ciclo di vita · 🟡 *~1 mese*
- `KisIOSFileProxy` su `UIDocumentPicker` + bookmark.
- Salvataggio `.kra`, import/export PNG/JPEG/PSD nella sandbox.
- Gestione suspend/resume e memory warning.
- **Milestone:** apri/salva file reali; l'app sopravvive al background.

### Fase 5 — Adattamento UI tablet · 🔴 *2–4 mesi (aperto)*
- Docker → pannelli touch (riuso `qmlmodules/`), target ingranditi.
- Toolbar/menu ripensati per il tocco, popup palette colori/brush.
- Layout per iPad (split view, orientamento).
- **Milestone:** UX usabile senza mouse/tastiera.

### Fase 6 — Stabilizzazione & packaging · *continuativo*
- Profiling memoria (limite duro iPad), riduzione footprint.
- Test su più modelli di iPad, gestione thermal/perf.
- Pacchetto `.ipa` e flusso SideStore documentato.

**Totale a "v1 sideloadabile e usabile": ~9–18 mesi solo.**
Primo "disegna su tela" (fine Fase 2): plausibilmente ~5–9 mesi dall'inizio.

---

## 8. Cosa si può iniziare subito su Windows

Tutto ciò che è codice/configurazione portabile, senza compilare:

1. **Toolchain CMake iOS** — aggiungere `cmake/modules/ios.toolchain.cmake` e i preset.
2. **Cartella `packaging/ios/`** — skeleton progetto Xcode + `Info.plist` +
   `LaunchScreen` + asset icona, in parallelo a `packaging/android/`.
3. **Workflow CI** — `.github/workflows/ios.yml` per la build su runner macOS.
4. **Audit di staticizzazione** — generare l'elenco dei 222 plugin e il blocco
   `Q_IMPORT_PLUGIN` corrispondente; mappare i metadati JSON.
5. **Shim nativi (scrittura, non build)** — `KisIOSFileProxy.mm`, bridge Pencil in
   Objective-C++, stub di `KWindowSystem`/`KCrash` dietro `#ifdef Q_OS_IOS`.
6. **Checklist `Q_OS_IOS`** — dai ~58 file Android, lista dei punti da affiancare.

Questi artefatti compilano poi *as-is* sul runner macOS in Fase 0/1.

---

## 9. Rischi e domande aperte

- **Patch Qt**: Krita mantiene patch a Qt (`config-qt-patches-present.h.cmake`). Vanno
  riapplicate al Qt-for-iOS — possibile attrito a ogni aggiornamento di Qt.
- **Qt5 vs Qt6**: il default è Qt5.15; iOS moderno e il backend RHI/Metal sono meglio
  serviti da **Qt6**, ma in Krita Qt6 è ancora marcato "non production-ready". Decisione
  architetturale da prendere in Fase 0.
- **Memoria**: Krita è avido; iPad uccide i processi oltre soglia. Il tiled image
  backend va profilato presto.
- **Manutenzione**: senza upstreaming in Krita, ogni rebase sul master costa. Valutare
  fin da subito di tenere i codepath `Q_OS_IOS` minimi e proporli upstream.
- **Effort reale**: anche il team Krita ha valutato iOS e non l'ha fatto, soprattutto per
  effort/manutenzione e per il blocco App Store. Aspettativa onesta: è un progetto serio,
  non un weekend.

---

*Prossimo passo suggerito (se vuoi procedere): generare l'impalcatura della Fase 0
descritta in §8 — toolchain iOS, `packaging/ios/`, workflow CI — tutto creabile da qui
su Windows.*
