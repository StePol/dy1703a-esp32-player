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

