# Hardvér a zapojenie

## Základné časti

Aktuálny KiCad projekt obsahuje ESP32 DevKit, DY1703A, Li-ion/LiPo článok, LX-LCBST nabíjací/boost modul, reproduktor, 8 tlačidiel, vibračný motor, tranzistorové riadenie, batériový merací obvod, LED a pasívne súčiastky.

## ESP32 ↔ DY1703A

| ESP32 | GPIO | DY1703A |
|---|---:|---|
| TX | GPIO17 | RX |
| RX | GPIO16 | TX |
| GND | GND | GND |

UART: `9600, 8N1`.

## Tlačidlá

**Zdrojová pravda firmvéru je `include/config.h`:**

| Skladba | GPIO | Poznámka |
|---:|---:|---|
| 1 | 25 | aktívne LOW |
| 2 | 33 | aktívne LOW |
| 3 | 32 | aktívne LOW |
| 4 | 35 | aktívne LOW, input-only |
| 5 | 13 | aktívne LOW |
| 6 | 14 | aktívne LOW |
| 7 | 27 | aktívne LOW |
| 8 | 26 | aktívne LOW |

GPIO35 nemá interný pull-up; vyžaduje externé riešenie.

> `docs/wiring.md` obsahuje staršie mapovanie tlačidiel. Pred výrobou treba zosúladiť firmware, schému a skutočnú kabeláž.

## Batéria

| Signál | Hodnota |
|---|---:|
| ADC | GPIO34 |
| ENABLE | GPIO23 |
| R_TOP | 100 kΩ |
| R_BOTTOM | 100 kΩ |

Delič sa pripája iba počas merania. Firmware používa 12-bit ADC a 11 dB attenuáciu.

## Vibračný motor

- GPIO4
- aktívna úroveň HIGH
- motor je riadený cez externý tranzistorový stupeň.

## Napájanie

Zámer podľa aktuálnej dokumentácie:

```text
Li-ion/LiPo
     |
     v
LX-LCBST / nabíjanie + boost
     |
     +----> 5 V -> ESP32
     |
     +----> 5 V -> DY1703A
```

Všetky časti musia mať spoločnú GND.

## KiCad schéma

`docs/kicad/dy1703a/dy1703a.kicad_sch`

Titulný blok:
- názov: **Zapojenie Prehravaca s ESP32 A DY1703A**
- dátum: **2026-09-16**
- revízia: **1.0**
- autor: **Stefan Polacik**
- GitHub: **StePol/dy1703a-esp32-player**

## PCB

`dy1703a.kicad_pcb` je v aktuálnom repozitári iba základná prázdna PCB štruktúra bez rozmiestnených footprintov. Výrobná PCB dokumentácia teda ešte nie je hotová.

## Lokálne knižnice

Projekt obsahuje vlastný symbol/footprint DY1703A a lokálnu knižnicu LX-LCBST vrátane 3D modelu.

Footprint LX-LCBST uvádza rozmery ako odhad podľa fotografie; pred výrobou ich treba overiť.

