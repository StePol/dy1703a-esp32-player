# Webové rozhranie a konfigurácia

## Hlavná stránka

Hlavná stránka obsahuje hlavné logo, mriežku ôsmich skladieb, ovládanie prehrávania, hlasitosť, batériu, vstup do nastavení a v spodnej časti názov zariadenia s verziou a Git SHA.

Každá skladba má pevné číslo 1–8. Horná časť karty spustí skladbu. Obrázok skladby je samostatná klikateľná oblasť a po nastavení URL otvorí externý odkaz. Logo skladby sa pri pravidelnom obnovovaní stavu stránky znovu nenačítava.

## Nastavenie skladieb

Pre každú z 8 skladieb možno nastaviť:
- názov,
- vibrácie,
- LOOP,
- externú URL,
- vlastné PNG/JPG/JPEG logo do 300 kB.

Číslo skladby 1–8 je pevný vizuálny identifikátor a nie je editovateľné. Stav loga je v nastaveniach zelený, ak je logo nahraté, a červený, ak chýba. Na hlavnej stránke sa LOOP a vibrácie zobrazujú ikonami.

Logá skladieb sú uložené v LittleFS ako `/track1.logo` až `/track8.logo`.

## Hlasitosť

Rozsah je 0–30. `/api/volume?value=N` mení aktuálnu hlasitosť a `/api/volume/set?value=N` ju zároveň uloží do Preferences ako štartovaciu hlasitosť.

## Zariadenie a Wi-Fi

Možno meniť názov zariadenia, AP SSID, AP heslo, domácu Wi-Fi SSID a heslo. Uloženie nastavení vedie k reštartu ESP32.

## Logo

Hlavné logo podporuje PNG/JPG/JPEG, maximálne 300 kB, je uložené ako `/logo` v LittleFS a môže mať voliteľnú URL. Stav hlavného loga je v nastaveniach zobrazený rovnakým zeleným/červeným spôsobom ako pri logách skladieb.

## Údržba

Sekcia obsahuje heslo pre vstup do nastavení a blok **Konfigurácia**. Po výbere konfiguračného súboru sú v jednom riadku tlačidlá **Zápis**, **Čítanie** a **Nastavenie z výroby**.

Blok **Pamäť LittleFS** zobrazuje grafické zaplnenie, použité a celkové miesto, voľné miesto a samostatný súčet veľkosti hlavného loga a ôsmich log skladieb.

V spodnej časti celej stránky Nastavenia sa zobrazuje názov zariadenia, verzia a Git SHA.

Výrobné nastavenia obnovia názov, AP údaje, STA údaje na prázdne hodnoty, heslo nastavení, hlasitosť 20, názvy skladieb, odkazy skladieb a vypnú vibrácie aj LOOP. Logá v LittleFS sa pri tomto kroku odstraňujú podľa aktuálnej implementácie resetu.

## Konfiguračný súbor

Záloha obsahuje základné nastavenia zariadenia, Wi-Fi, heslo nastavení, hlasitosť a pre každú skladbu názov, URL, vibrácie a LOOP. Samotné binárne obrázky log nie sú súčasťou textovej konfiguračnej zálohy.

## Status API

`GET /api/status` vracia okrem základného stavu aj `device`, `version`, `git`, `build`, aktuálnu skladbu, hlasitosť, batériu, hlavné logo a URL, názvy skladieb, odkazy, stav log skladieb, vibrácie a LOOP.

Chránené `GET /api/settings` navyše poskytuje údaje potrebné pre stránku nastavení vrátane `fsTotal`, `fsUsed` a `logoBytes`.

## Wi-Fi režimy

Ak je nastavené STA SSID, firmware sa pokúsi pripojiť ako klient. Timeout je 15 s. Pri zlyhaní prejde na AP.

Predvolené AP:
- SSID: `DY1703A-Player`
- heslo: `password`
- port: 80

Pri AP režime sa používa DNS wildcard. Ak po určitý čas nie je pripojený klient, Wi-Fi sa vypne.

## Bezpečnosť

Predvolené heslo nastavení je `12345` a predvolené AP heslo `password`. Web server používa HTTP bez TLS. Pri reálnom nasadení treba predvolené heslá zmeniť.

## Verziovanie firmvéru

Projekt používa SemVer `MAJOR.MINOR.PATCH` a Git tagy `vMAJOR.MINOR.PATCH`.

- **MAJOR** – významná alebo nekompatibilná zmena,
- **MINOR** – nová funkcia pri zachovaní kompatibility,
- **PATCH** – oprava alebo menšia úprava.

### Automatické vytvorenie ďalšej verzie

V koreňovom adresári je `release.py`. Číslo aktuálnej verzie si netreba pamätať.

```bash
python release.py
```

Bez parametra sa zvýši PATCH. Ďalšie možnosti:

```bash
python release.py minor
python release.py major
```

Skript:
1. vyžaduje čistý Git working tree,
2. vykoná `git fetch --tags`,
3. nájde najvyšší semantický tag `vX.Y.Z`,
4. vypočíta ďalšie číslo,
5. zobrazí starú a novú verziu,
6. vyžiada potvrdenie,
7. vytvorí anotovaný Git tag,
8. odošle tag na `origin`.

Ak žiadny semantický tag neexistuje, vychádza z `0.0.0`.

### Verzia pri kompilácii

PlatformIO spúšťa `extra_script.py` cez:

```ini
extra_scripts = pre:extra_script.py
```

Pre-build skript zistí posledný Git tag, skrátený 8-znakový SHA aktuálneho commitu a počet commitov. Následne v build adresári vytvorí `build_info.h` s `BUILD_VERSION`, `BUILD_GIT_SHA` a `BUILD_NUMBER`.

Web zobrazuje napríklad:

```text
v1.2.3 · 779f25d5
```

Git SHA odlišuje konkrétny build aj vtedy, keď od posledného tagu pribudli ďalšie commity. Bežné `pio run` samo nevytvára novú vydanú verziu.
