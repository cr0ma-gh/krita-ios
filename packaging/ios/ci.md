# Eseguire la build iOS di Krita su un runner

Tutta la logica di build sta in **[build-krita-ios.sh](build-krita-ios.sh)** (preflight →
dipendenze → configure → build → `.ipa` non firmato). Qualsiasi ambiente macOS la esegue
allo stesso modo: cambia solo il "guscio" che la lancia. Scegli il runner che preferisci.

Prerequisiti comuni su ogni runner: **macOS + Xcode**, `cmake`, `ninja`, e un
**Qt-for-iOS** (idealmente patchato — vedi README.ios.md §7) il cui percorso va in
`QT_IOS_ROOT`.

Variabili d'ambiente principali: `PLATFORM` (`OS64`|`SIMULATORARM64`),
`DEPLOYMENT_TARGET` (default `16.0`), `QT_IOS_ROOT` (obbligatoria), `WORK` (scratch).

---

## 1. GitHub Actions — **configurato, gratis per repo pubblici** ✅

Già pronto in [`.github/workflows/ios.yml`](../../.github/workflows/ios.yml), runner
`macos-14` (Apple silicon). Installa **sia il Qt host (desktop macOS) sia il Qt iOS** — la
cross-build Qt6 richiede entrambi — e li passa all'orchestratore via `QT_HOST_PATH` e
`QT_IOS_ROOT`. Parte da solo al push su `main`/`ios`, oppure da **Actions → iOS build →
Run workflow**. L'`.ipa` è negli *Artifacts* del job.

**È il percorso a costo zero**: i runner macOS sono gratuiti per i repository **pubblici**
(per i privati i minuti macOS contano 10× sul free tier, ~200 min/mese). Per un fork GPL di
Krita tenere il repo pubblico è naturale. Nessuna configurazione aggiuntiva: basta pushare.

## 2. Mac self-hosted (il tuo Mac come runner)

Se hai un Mac ma vuoi farlo guidare dal CI:
```sh
# sul Mac, una volta: registra il runner come da guida GitHub, poi
brew install cmake ninja
export QT_IOS_ROOT=/path/to/Qt/6.7.x/ios
```
Cambia in `ios.yml` `runs-on: macos-14` → `runs-on: [self-hosted, macOS]` e rimuovi lo
step `install-qt-action` (usi il Qt locale via `QT_IOS_ROOT`).

## 3. Altre alternative gratuite

Se non puoi usare GitHub Actions, lo stesso [build-krita-ios.sh](build-krita-ios.sh) gira
su altri runner macOS (verifica i limiti del free tier, cambiano spesso):
- **Codemagic** — CI mobile, free tier ~500 min/mese su M-series; gestisce anche il signing iOS.
- **Appveyor** — gratis per progetti open source, immagini macOS.
- **Self-hosted** (vedi §2) — gratis se hai un Mac qualsiasi.

> Versioni precedenti di questa guida configuravano Cirrus CI e CircleCI; rimossi a favore
> di GitHub Actions, gratuito per repo pubblici.

## 4. In locale, sul tuo Mac

Nessun CI, solo un terminale:
```sh
export QT_IOS_ROOT=/path/to/Qt/6.7.x/ios
export PLATFORM=OS64           # o SIMULATORARM64 per il Simulator
bash packaging/ios/build-krita-ios.sh
# -> _ios/krita-unsigned-OS64.ipa
```

---

## Dal `.ipa` all'iPad senza Mac

L'`.ipa` non è firmato. Per installarlo sul **tuo** iPad senza possedere un Mac:
**SideStore / AltStore** lo firmano con il tuo Apple ID gratuito direttamente dal
dispositivo (refresh ogni 7 giorni). Vedi [README.ios.md](../../README.ios.md) §6.

## Stato onesto

Finché il superbuild delle dipendenze ([dependencies.md](dependencies.md)) e il Qt-for-iOS
patchato non sono completi, lo script arriva fino a configure/build ma **non** produce
ancora `krita.app`: riporta a che punto si è fermato ed esce senza errore. È il
comportamento atteso in Fase 0.
