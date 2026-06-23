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

## 3. CircleCI (executor macOS) — **configurato**

Il file [`.circleci/config.yml`](../../.circleci/config.yml) è **già pronto**. Installa Qt
(host + iOS) con `aqtinstall`, cachea Qt e le dipendenze, esegue l'orchestratore e
archivia l'`.ipa` come *artifact*.

Attivazione:
1. Su https://app.circleci.com accedi con GitHub e **"Set Up Project"** sul repo.
2. CircleCI rileva `.circleci/config.yml`; al push la pipeline parte.
3. L'`.ipa` (quando prodotto) è in **Artifacts** del job `build-ios`.

Note:
- L'**executor macOS di CircleCI richiede un piano con minuti macOS** (non incluso nel
  free tier Linux); è la differenza principale rispetto ad altri runner.
- `xcode` (immagine), `resource_class` e `QT_VERSION` sono in cima al job — aggiornali se
  serve una versione diversa.
- Per il Simulator, duplica il job con `PLATFORM: SIMULATORARM64` (e adatta i path di cache,
  che includono `OS64`).

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
