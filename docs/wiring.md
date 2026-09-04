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

> Protokol a pinout overené podľa oficiálneho datasheetu:
> [`docs/SEN-17-096-DataSheet.pdf`](SEN-17-096-DataSheet.pdf)

| ESP32 (UART2) | DY1703A |
|----------------|----------|
| TX2 (GPIO17)    | IO1 (RXD)* |
| RX2 (GPIO16)    | IO0 (TXD)* |
| GND             | GND       |
| 5V              | VCC       |

\* Podľa datasheetu je pin1 = TXD/IO0 (modul vysiela), pin2 = RXD/IO1
(modul prijíma) — presne toto zapojenie. Ak by fyzická potlač na tvojom
konkrétnom kuse bola napriek tomu prehodená, over multimetrom.

### Nastavenie CON pinov pre UART mód s funkčným BUSY (odskúšané na reálnom HW)

Presne tie 3 rezistory z fotky slúžia na toto nastavenie módu — **overené
priamo na tvojom kuse modulu**, každý cez vlastný 10kΩ rezistor:

| Pin  | Zapojenie |
|-------|---------|
| CON1 | 10kΩ na GND |
| CON2 | 10kΩ na +5V |
| CON3 | 10kΩ na +5V |

> Táto kombinácia (CON1=GND, CON2=5V, CON3=5V) sa líši od kombinácie
> uvedenej v datasheete pre čistý "UART Mode" (CON1=GND, CON2=GND,
> CON3=5V) — datasheet túto konkrétnu kombináciu explicitne nepopisuje,
> ale reálne testovanie potvrdilo, že UART komunikácia aj BUSY výstup na
> CON3 fungujú spoľahlivo práve s CON2 na 5V. Drž sa preto tejto
> odskúšanej konfigurácie, nie čisto textu datasheetu.

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

## Vibračný motorček (hardvérovo cez BUSY výstup, bez zásahu ESP32)

Motor sa spína **čisto hardvérovo** cez pin CON3, ktorý pri tejto
konfigurácii CON pinov (viď vyššie) funguje ako **BUSY výstup, aktívny
HIGH počas prehrávania** (LOW keď nehrá) — overené priamo meraním na
reálnom module. ESP32 do tohto obvodu vôbec nezasahuje.

> Poznámka k datasheetu: text datasheetu opisuje BUSY ako aktívne LOW
> počas hrania, čo je opačne — pravdepodobne preto, že sa to týka inej
> kombinácie CON pinov než tej, ktorú tu reálne používaš (datasheet
> kombináciu CON1=GND/CON2=5V/CON3=5V explicitne nepopisuje). Riaď sa
> zmeraným správaním na svojom HW, nie doslovným textom.

### Spínací obvod (BUSY → motor)

```
CON3 (BUSY) ──┬──────────────►|── [10kΩ] ──┐
              │   (dióda)                  │
          [10kΩ]                      Báza BC547 (NPN)
              │                             │
             GND                       Emitor BC547 ── GND
                                             │
                                        Kolektor BC547
                                             │
                                        [1kΩ]
                                             │
                                        Báza BC557 (PNP)
                                             │
                              +3V3 ── Emitor BC557
                                        Kolektor BC557
                                             │
                                        [10Ω]
                                             │
                                    ┌────────┴────────┐
                                   Motor(M)      Flyback dióda
                                    │                  │
                                   GND ──────────────  GND
```

- **BUSY (CON3) = LOW (nehrá)** → dióda + 10kΩ nedokážu otvoriť BC547
  (báza držaná na GND cez 10kΩ pulldown) → BC547 zatvorený → báza BC557
  nie je stiahnutá k zemi → BC557 (PNP) zostáva zatvorený → motor stojí.
- **BUSY (CON3) = HIGH (hrá skladba)** → cez diódu a 10kΩ sa otvorí BC547
  → jeho kolektor stiahne bázu BC557 smerom ku GND → BC557 sa otvorí →
  prúd z +3.3V tečie cez BC557 a 10Ω rezistor do motora → motor vibruje.
- Dióda pri BUSY vstupe chráni bázu BC547 pred spätným prúdom.
- Flyback dióda cez motor chráni BC557 pred napäťovými špičkami pri vypnutí.
- 10kΩ z BUSY na GND drží uzol v definovanom stave, keď je BUSY neaktívne.

### Súčiastky

- 2x tranzistor: **BC547** (NPN) a **BC557** (PNP)
- 1x dióda (na BUSY vstupe, napr. 1N4148)
- 1x flyback dióda cez motor (1N4148/1N4001)
- Rezistory: 3× 10kΩ (CON piny), 1× 10kΩ (BUSY pulldown + báza BC547),
  1× 1kΩ (báza BC557), 1× 10Ω (v sérii s motorom)

> Poznámka: `DY1703A::isPlaying()` vo firmvéri sa momentálne nepoužíva na
> motor (ten je čisto hardvérový) — necháva sa v knižnici pre prípadné
> budúce zobrazenie stavu prehrávania cez web/BLE rozhranie.

## Meranie napätia batérie

Odporový delič napätia na ADC pin ESP32 (GPIO34) meria napätie priamo na
batérii (pred boost prevodníkom), aby firmvér vedel odhadnúť stav nabitia.

```
BAT+ ---[R_TOP 100kΩ]---+---[R_BOTTOM 100kΩ]--- GND
                          |
                        GPIO34 (ADC1)
```

- Delič zníži max. napätie batérie (~4.2V) na ~2.1V na ADC pine — bezpečne
  v rozsahu, ktorý ESP32 ADC dokáže presne merať.
- **Potrebuješ 2 ďalšie rezistory** (100kΩ + 100kΩ) — tie 3, čo už máš,
  sú vyhradené na CON piny DY1703A modulu (viď vyššie).
- GPIO34 je input-only pin (žiadny interný pull-up/down), preto sa sem
  hodí presne na analógové meranie — na tlačidlo by nebol vhodný.
- Delič mierne priebežne odoberá prúd z batérie (~21µA pri 4.2V, 200kΩ
  celkovo) aj keď je zariadenie vypnuté — zanedbateľné, ale ak by ti to
  prekážalo, dá sa doplniť vysokoúrovňový spínač (napr. P-MOSFET), ktorý
  delič odpojí, keď zariadenie nemeria.
- Firmvér (`lib/BatteryMonitor`) číta napätie každých 10s a prepočíta ho
  na orientačné percento (lineárna aproximácia 3.0V=0% až 4.2V=100% —
  skutočná vybíjacia krivka Li-ion nie je lineárna, takže presnosť je len
  orientačná).
