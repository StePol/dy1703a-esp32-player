# Schéma zapojenia

## Napájací reťazec

```
LiPo batéria (3.7V) → TP4056 (nabíjanie + ochrana) → Boost prevodník (5V)
                                                          ├─→ ESP32 (5V / VIN)
                                                          └─→ DY1703A (VCC)
```

- Boost prevodník (napr. MT3608) nastav presne na 5.0V pred prvým pripojením modulov.
- Spoločná zem (GND) pre všetky bloky.
- Odporúčaná kapacita batérie: podľa plánovanej výdrže, min. 1000mAh pri použití reproduktora nad 1W.
- **2x 18650 zapojené paralelne** (+ na +, − na −) na B+/B− piny nabíjacieho modulu.
  Nabíjací čip **CSM4056T** je 1-článkový (1S) Li-ion charger (rodina TP4056),
  paralelné články sa navzájom napäťovo vyrovnávajú bez potreby BMS balancingu.
  Použi rovnaké články (rovnaká značka/kapacita/šarža) — sériové (2S) zapojenie
  s týmto modulom nepoužívaj, nabíjal by len na 4.2V a druhý článok by sa
  nedobíjal správne.

## ESP32 ↔ DY1703A (UART)

| ESP32 (UART2) | DY1703A |
|----------------|----------|
| TX2 (GPIO17)    | IO1 (RXD)* |
| RX2 (GPIO16)    | IO0 (TXD)* |
| GND             | GND       |
| 5V              | VCC       |

\* Na niektorých kusoch DY1703A je potlač IO0/IO1 vnútorne prehodená — ak
komunikácia nefunguje pri zapojení kríženo (TX↔RX), skús priame zapojenie
TX–TX a RX–RX.

### Nastavenie CON pinov pre UART mód (tvoje 3 rezistory)

Presne tie 3 rezistory z fotky slúžia na toto nastavenie módu:

| Pin  | Úroveň |
|-------|---------|
| CON1 | GND (cez rezistor alebo priamo) |
| CON2 | GND (cez rezistor alebo priamo) |
| CON3 | 5V — **odporúča sa cez ~10kΩ rezistor** (priame pripojenie na 5V spôsobovalo problémy viacerým používateľom) |

## Tlačidlá (2x 4-tlačidlová klávesnica = 8 skladieb)

Každé tlačidlo je zapojené samostatným vodičom priamo na GPIO ESP32 (nie na
piny DY1703A) — spoločná je len GND. Riadenie prehrávania robí firmvér cez
UART príkazy (`playTrack(N)`), preto rovnaké skladby vieš spustiť aj cez
web/BLE bez konfliktu s tlačidlami.

| Klávesnica | Tlačidlo | Skladba | GPIO |
|-------------|-----------|----------|------|
| 1           | 1         | 1        | 13   |
| 1           | 2         | 2        | 14   |
| 1           | 3         | 3        | 27   |
| 1           | 4         | 4        | 26   |
| 2           | 1         | 5        | 25   |
| 2           | 2         | 6        | 33   |
| 2           | 3         | 7        | 32   |
| 2           | 4         | 8        | 15   |

Každé tlačidlo je zapojené medzi príslušný GPIO a GND (aktívne v LOW,
využíva sa interný `INPUT_PULLUP` v ESP32 — netreba externé rezistory).

> GPIO12 je zámerne vynechané — je to boot-strapping pin (MTDI), ktorý pri
> nesprávnom stave počas štartu môže zmeniť napäťovú úroveň flash pamäte a
> spôsobiť neštartovanie ESP32.

## Reproduktor

Pripája sa priamo na výstup SPK+/SPK- modulu DY1703A (vstavaný 5W Class D
zosilňovač, 4-8Ω reproduktor).

## Voliteľné: meranie napätia batérie

Odporúčaný odporový delič na ADC pin ESP32 (napr. GPIO34) pre monitorovanie
stavu nabitia batérie — zatiaľ nezapojené, plánované rozšírenie.
