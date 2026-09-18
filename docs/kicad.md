# KiCad projekt

## Umiestnenie

`docs/kicad/dy1703a/`

## Súbory

```text
dy1703a.kicad_pro
dy1703a.kicad_sch
dy1703a.kicad_pcb
dy1703a.kicad_sym
dy1703a.pretty/
fp-lib-table
sym-lib-table
LX-LCBST/
```

## Schéma

`dy1703a.kicad_sch` obsahuje lokálne symboly pre DY1703A, ESP32 DevKit, LX-LCBST, batériu, reproduktor, motor, tlačidlá, LED, diódy, tranzistory a pasívne súčiastky.

Titulný blok:
| Položka | Hodnota |
|---|---|
| Názov | Zapojenie Prehravaca s ESP32 A DY1703A |
| Dátum | 2026-09-16 |
| Revízia | 1.0 |
| Autor | Stefan Polacik |
| Firma | TRIX Lubomir Drgan |
| GitHub | StePol/dy1703a-esp32-player |

## Lokálne symboly

`dy1703a.kicad_sym` obsahuje okrem iných DY1703A, ESP32_DevKit_V1_DOIT_30GPIOs, LX-LCBST_Boost_Charger, Membrane_Keypad_1x4, Motor_DC, Speaker, Battery_Cell, BSS84, BS170, BC546, BC557, STPS1150, 1N4148 a napájacie symboly.

## DY1703A footprint

`dy1703a.pretty/DY1703A.kicad_mod` obsahuje 18 vývodov, pin-1 marker, obrys a označenie DY1703A.

## LX-LCBST

Adresár `LX-LCBST/` obsahuje symbol, footprint a 3D model. Symbol definuje IN+, IN-, B+, B-, VO+, VO-. Footprint obsahuje kontakty na oboch stranách modulu.

## Knižničné tabuľky

`fp-lib-table` používa projektové cesty:
```text
${KIPRJMOD}/dy1703a.pretty
${KIPRJMOD}/LX-LCBST/LX-LCBST.pretty
```

`sym-lib-table` používa:
```text
${KIPRJMOD}/dy1703a.kicad_sym
${KIPRJMOD}/LX-LCBST/LX-LCBST.kicad_sym
```

## PCB

`dy1703a.kicad_pcb` je zatiaľ prázdna základná PCB štruktúra. Nie je to hotová doska pripravená na výrobu.

## Prenos projektu

Treba zachovať celú štruktúru `docs/kicad/dy1703a/`, vrátane lokálnych knižníc a oboch tabuliek knižníc.
