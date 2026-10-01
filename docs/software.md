# Webové rozhranie a konfigurácia

## Hlavná stránka

Obsahuje logo, názov zariadenia, verziu, stav prehrávania, 8 skladieb, Play/Pauza/Stop, hlasitosť, batériu a vstup do nastavení.

## Nastavenie skladieb

Pre každú z 8 skladieb možno nastaviť:
- názov,
- vibrácie,
- LOOP.

Na hlavnej stránke sa LOOP a vibrácie zobrazujú samostatnými ikonami.

## Hlasitosť

Rozsah je 0–30.

`/api/volume?value=N` zmení aktuálnu hlasitosť.

`/api/volume/set?value=N` zmení a uloží hlasitosť do Preferences, takže sa použije po ďalšom štarte.

## Zariadenie a Wi-Fi

Možno meniť názov zariadenia, AP SSID, AP heslo, domácu Wi-Fi SSID a heslo. Uloženie nastavení vedie k reštartu ESP32.

## Údržba

Obsahuje heslo pre vstup do nastavení, zápis konfigurácie, obnovenie konfigurácie a obnovenie výrobných nastavení.

Výrobné nastavenia podľa aktuálneho firmvéru obnovia názov, AP údaje, STA údaje na prázdne hodnoty, heslo nastavení, hlasitosť 20, názvy skladieb a vypnú vibrácie aj LOOP. Logo sa pritom neodstraňuje.

## Logo

- PNG/JPG/JPEG
- max. 300 kB
- LittleFS: `/logo`
- voliteľná URL po kliknutí

## Konfiguračný súbor

```text
# DY1703A Player configuration
version=1
device=...
apssid=...
appass=...
ssid=...
wpass=...
setpass=...
volume=20
trk1=Skladba 1
vib1=0
lop1=0
...
trk8=Skladba 8
vib8=0
lop8=0
```

## Status API

`GET /api/status` vracia device, version, git, build, playing, track, volume, battery, voltage, logo, logoUrl, names, vibration a loop.

## Wi-Fi režimy

Ak je nastavené STA SSID, firmware sa pokúsi pripojiť ako klient. Timeout je 15 s. Pri zlyhaní prejde na AP.

Predvolené AP:
- SSID: `DY1703A-Player`
- heslo: `password`
- port: 80

Pri AP režime sa používa DNS wildcard. Ak po určitý čas nie je pripojený klient, Wi-Fi sa vypne.

## Bezpečnosť

Predvolené heslo nastavení je `12345` a predvolené AP heslo `password`. Web server používa HTTP bez TLS. Pri reálnom nasadení treba predvolené heslá zmeniť a zariadenie nepovažovať za bezpečný administračný server v nedôveryhodnej sieti.



## Verziovanie firmvéru

Projekt používa verzie v tvare `MAJOR.MINOR.PATCH` a Git tagy v tvare `vMAJOR.MINOR.PATCH`, napríklad `v1.0.0` alebo `v1.2.3`.

Význam častí verzie:
- **MAJOR** – hlavná verzia; zvyšuje sa pri významnej alebo nekompatibilnej zmene,
- **MINOR** – nová funkcia pri zachovaní kompatibility,
- **PATCH** – oprava alebo menšia úprava bez novej hlavnej funkcie.

Verzia sa do firmvéru nezapisuje ručne. PlatformIO spúšťa pred kompiláciou `extra_script.py` cez:

```ini
extra_scripts = pre:extra_script.py
```

Skript zistí:
- posledný Git tag pomocou `git describe --tags --abbrev=0`,
- skrátený 8-znakový SHA aktuálneho commitu pomocou `git rev-parse --short=8 HEAD`,
- poradové číslo buildu ako počet commitov pomocou `git rev-list --count HEAD`.

Ak posledný tag zodpovedá tvaru `vX.Y.Z` alebo `X.Y.Z`, použije sa z neho verzia `X.Y.Z`. Ak platný tag neexistuje, verzia je `0.0.0-dev`.

Pred kompiláciou sa automaticky vytvorí `build_info.h` v build adresári s hodnotami:

```cpp
#define BUILD_VERSION "1.0.0"
#define BUILD_GIT_SHA "7519cfbf"
#define BUILD_NUMBER "..."
```

Súbor je generovaný automaticky a neukladá sa ručne do zdrojového kódu. Webové rozhranie zobrazuje verziu a skrátený Git SHA, napríklad:

```text
v1.0.0 · 7519cfbf
```

Git SHA jednoznačne určuje konkrétny commit použitý pri zostavení. Dôležité je, že aktuálna implementácia používa **posledný dostupný semantický tag**, nie informáciu o počte commitov od tagu. Preto build vytvorený po tage `v1.0.0` môže naďalej zobrazovať `v1.0.0`, pričom konkrétnu revíziu odlišuje Git SHA.

### Vytvorenie novej verzie

Po dokončení a commitnutí zmien sa nová vydaná verzia označí Git tagom, napríklad:

```bash
git tag v1.1.0
git push origin v1.1.0
```

Nasledujúci build automaticky prevezme verziu z tohto tagu. Pri opravnom vydaní sa analogicky použije napríklad `v1.1.1`; pri novej hlavnej verzii napríklad `v2.0.0`.
