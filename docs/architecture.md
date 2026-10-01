# Architektúra firmvéru

## Prehľad

Firmvér je sústredený v `src/main.cpp`. Hardvérové konštanty sú v `include/config.h`. Ovládač DY1703A a batériový monitor sú oddelené do vlastných PlatformIO knižníc.

```text
                 +----------------------+
                 |        ESP32         |
                 |                      |
 tlačidlá 1..8 ->| GPIO                 |
 Wi-Fi <-------->| Web server           |
 USB Serial <--->| servisné príkazy     |
 GPIO17 TX ------> DY1703A              |
 GPIO16 RX <------ DY1703A              |
 GPIO4 ----------> motor driver         |
 GPIO34 <--------- battery ADC           |
 GPIO23 ----------> battery switch      |
                 +----------------------+
```

## Hlavné moduly

### `src/main.cpp`
Zodpovedá za inicializáciu, Preferences, Wi-Fi, web server, autentifikáciu nastavení, LittleFS/logo, tlačidlá, prehrávanie, hlasitosť, LOOP, vibrácie, batériu, light sleep a Serial servis.

### `include/config.h`
Obsahuje pinout a systémové konštanty pre UART, tlačidlá, Wi-Fi, batériu, motor a časové limity.

### `lib/DY1703A/`
Implementuje UART príkazy play, pause, stop, previous, next, play track, volume a play state.

### `lib/BatteryMonitor/`
Implementuje spínané meranie batérie cez ADC. Delič je počas nečinnosti odpojený.

### `extra_script.py`
PlatformIO pre-build skript generujúci informácie o builde z Git-u.

### `release.py`
Pomocný skript pre vydanie novej semantickej verzie. Automaticky nájde najvyšší tag `vMAJOR.MINOR.PATCH`, zvýši PATCH/MINOR/MAJOR, po potvrdení vytvorí anotovaný tag a odošle ho na `origin`.

## Stav prehrávania

Firmware používa `currentTrack`, `currentPlaying` a `loopPlaybackArmed`. Približne každých 300 ms číta stav DY1703A. Pri prechode PLAY → STOP a zapnutom LOOP znovu spustí aktuálnu skladbu.

## Konfigurácia

Preferences namespace:

`settings`

Používané kľúče zahŕňajú `device`, `apssid`, `appass`, `ssid`, `wpass`, `setpass`, `volume`, `logourl`, `trk1..trk8`, `url1..url8`, `vib1..vib8`, `lop1..lop8`.

## Web API

Verejné:
```text
GET /
GET /logo
GET /track-logo?track=N
GET /api/status
GET /api/play
GET /api/pause
GET /api/stop
GET /api/volume?value=N
GET /api/volume/set?value=N
GET /settings-login
POST /api/login
```

Chránené:
```text
GET  /settings
GET  /api/settings
POST /api/settings
DELETE /api/logo
POST /api/logo
DELETE /api/track-logo?track=N
POST /api/track-logo?track=N
POST /api/defaults
GET  /api/backup
POST /api/restore
```

## Autentifikácia

Po úspešnom prihlásení sa vytvorí session token a cookie `DYSESSION=<token>`. Token sa po reštarte ESP32 stratí.

## LittleFS

Používané súbory:
- hlavné logo: `/logo`,
- logá skladieb: `/track1.logo` až `/track8.logo`.

Dočasné uploady používajú `/logo.tmp` a `/track.tmp`. Maximálna veľkosť každého loga je 300 kB. Stránka Údržba zobrazuje celkové/použité/voľné miesto LittleFS a súčet veľkosti všetkých log.

## Serial API

```text
CMD:WIFI_SSID=<ssid>
CMD:WIFI_PASSWORD=<heslo>
CMD:GET_WIFI
CMD:RESTART
```

Pri `CMD:GET_WIFI` sa heslo nevypisuje.

## Hlavná slučka

```text
pollSerialCommands()
        |
handleButtons()
        |
player.poll()
        |
play state / LOOP / motor
        |
batéria
        |
pending restart
        |
Wi-Fi
        |
light sleep
```

## Časovanie

| Funkcia | Hodnota |
|---|---:|
| Stav DY1703A + motor | 300 ms |
| Batéria | 10 s |
| AP grace po odpojení | 60 s |
| STA timeout | 15 s |
| Light sleep po nečinnosti | 30 s |
| Kontrola light sleep | 5 s |
| Debounce tlačidiel | 40 ms |
