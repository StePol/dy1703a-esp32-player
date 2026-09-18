# DY1703A ESP32 Audio Player

Prehrávač zvukov založený na module **DY1703A / DY-SV17F** a riadený **ESP32 DevKit**. Projekt obsahuje firmvér, KiCad schému a lokálne KiCad knižnice.

> Dokumentácia zodpovedá aktuálnemu obsahu vetvy `main` a bola pripravená na základe revízie `8a4de6d` z 16. 9. 2026.

## Funkcie

- prehrávanie až 8 skladieb,
- 8 fyzických tlačidiel,
- webové ovládanie cez Wi-Fi,
- STA alebo vlastný AP režim,
- Play / Pauza / Stop,
- hlasitosť 0–30 a uloženie štartovacej hlasitosti,
- názvy skladieb,
- individuálne vibrácie,
- individuálny LOOP,
- orientačné meranie batérie,
- logo PNG/JPG v LittleFS,
- externý odkaz z loga,
- heslom chránené nastavenia,
- záloha/obnova konfigurácie,
- výrobné nastavenia,
- automatická verzia + Git SHA + build number,
- light sleep,
- servisné Serial príkazy.

## Dôležitá poznámka k Bluetooth

V `platformio.ini` je deklarovaná závislosť NimBLE-Arduino, ale aktuálny `src/main.cpp` BLE rozhranie nepoužíva. Starší README uvádzal Bluetooth ako vlastnosť projektu; podľa aktuálneho zdrojového kódu ho preto nemožno považovať za hotovú funkciu.

## Webové rozhranie

Nastavenia sú rozdelené na:

1. **Logo**
2. **Zariadenie a Wi-Fi**
3. **Nastavenie skladieb**
4. **Údržba**

Predvolené heslo pre nastavenia je `12345`. Predvolené AP heslo je `password`. Web používa HTTP bez TLS, preto je pri nasadení vhodné predvolené heslá zmeniť.

## UART DY1703A

| Funkcia | ESP32 |
|---|---:|
| TX → DY RX | GPIO17 |
| RX ← DY TX | GPIO16 |
| Baudrate | 9600 |
| Formát | 8N1 |

Firmware používa rámce začínajúce `0xAA` a podporuje play, pause, stop, previous, next, play track, volume a play state.

## GPIO podľa aktuálneho firmvéru

| Funkcia | GPIO |
|---|---:|
| Motor | 4 |
| Track 1 | 25 |
| Track 2 | 33 |
| Track 3 | 32 |
| Track 4 | 35 |
| Track 5 | 13 |
| Track 6 | 14 |
| Track 7 | 27 |
| Track 8 | 26 |
| Battery ADC | 34 |
| Battery enable | 23 |
| UART RX | 16 |
| UART TX | 17 |

> `docs/wiring.md` obsahuje staršie poradie GPIO pre tlačidlá. Pred výrobou treba zosúladiť firmware, schému a skutočnú kabeláž.

## Batéria

Používa sa spínaný odporový delič:

- ADC GPIO34,
- enable GPIO23,
- R_TOP 100 kΩ,
- R_BOTTOM 100 kΩ.

Delič je mimo merania odpojený. Percentá sú lineárny orientačný odhad 3,0–4,2 V.

## KiCad

Projekt je v `docs/kicad/dy1703a/` a obsahuje:

- `dy1703a.kicad_pro`
- `dy1703a.kicad_sch`
- `dy1703a.kicad_pcb`
- `dy1703a.kicad_sym`
- `dy1703a.pretty/`
- `LX-LCBST/`
- `fp-lib-table`
- `sym-lib-table`

Schéma má titulný blok s názvom **Zapojenie Prehravaca s ESP32 A DY1703A**, dátumom 2026-09-16, revíziou 1.0 a autorom Stefan Polacik.

**PCB je zatiaľ prázdna základná PCB štruktúra; nejde ešte o výrobný návrh.**

## Štruktúra

```text
dy1703a-esp32-player/
├── include/
│   └── config.h
├── lib/
│   ├── DY1703A/
│   └── BatteryMonitor/
├── src/
│   └── main.cpp
├── docs/
│   ├── architecture.md
│   ├── hardware.md
│   ├── software.md
│   ├── kicad.md
│   ├── wiring.md
│   └── kicad/dy1703a/
├── extra_script.py
├── platformio.ini
└── README.md
```

## Build

V koreňovom adresári:

```bash
pio run
pio run -t upload
pio device monitor
```

Monitor používa 115200 baud, upload 921600 baud.

## Dokumentácia

- [Architektúra firmvéru](docs/architecture.md)
- [Hardvér a zapojenie](docs/hardware.md)
- [Webové rozhranie a konfigurácia](docs/software.md)
- [KiCad projekt](docs/kicad.md)
- [Pôvodný dokument zapojenia](docs/wiring.md)

## Stav projektu

Firmvér pokrýva základnú funkčnosť prehrávača, Wi-Fi/webové ovládanie, konfiguráciu, logo, batériu, vibrácie, LOOP, light sleep a servisné Serial príkazy.

Hardvérová časť má aktuálnu KiCad schému a projektové lokálne knižnice. PCB návrh ešte nie je vytvorený.
