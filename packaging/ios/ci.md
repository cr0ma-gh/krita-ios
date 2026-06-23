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

## 1. GitHub Actions (incluso)

Già pronto in [`.github/workflows/ios.yml`](../../.github/workflows/ios.yml). Runner
`macos-14` (Apple silicon). Avvio: scheda **Actions → iOS build → Run workflow**, oppure
push su un branch `ios`. Qt arriva da `install-qt-action`; l'artifact `.ipa` è scaricabile
a fine job. Gratis per repo pubblici; ~free-tier minuti per repo privati.

## 2. Mac self-hosted (il tuo Mac come runner)

Se hai un Mac ma vuoi farlo guidare dal CI:
```sh
# sul Mac, una volta: registra il runner come da guida GitHub, poi
brew install cmake ninja
export QT_IOS_ROOT=/path/to/Qt/6.7.x/ios
```
Cambia in `ios.yml` `runs-on: macos-14` → `runs-on: [self-hosted, macOS]` e rimuovi lo
step `install-qt-action` (usi il Qt locale via `QT_IOS_ROOT`).

## 3. Cirrus CI (macOS gratuito per progetti open source) — **configurato**

Il file [`.cirrus.yml`](../../.cirrus.yml) al root del repo è **già pronto**. Installa Qt
(host + iOS) con `aqtinstall`, cachea Qt e le dipendenze, esegue l'orchestratore e
pubblica l'`.ipa`.

Attivazione:
1. Installa la **Cirrus CI** app sul tuo repo GitHub (https://cirrus-ci.org → "Install").
2. Push: la build parte automaticamente; l'`.ipa` finisce negli *Artifacts* del task.

Note:
- L'immagine `macos_instance` (`ghcr.io/cirruslabs/macos-runner:sequoia`) e `QT_VERSION`
  sono in cima al file — aggiornale se serve una versione di Xcode/Qt diversa.
- Per costruire anche per il Simulator, usa la matrice `PLATFORM` commentata in fondo al file.
- macOS su Cirrus è gratuito per repo pubblici entro i limiti del free tier OSS.

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
