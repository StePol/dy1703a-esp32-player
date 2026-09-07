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
| 2           | 4         | 8        | 35   |

Každé tlačidlo je zapojené medzi príslušný GPIO a GND (aktívne v LOW).
Sedem z ôsmich tlačidiel využíva interný `INPUT_PULLUP` v ESP32 (netreba
externé rezistory). **Výnimka: GPIO35** je input-only pin (rovnako ako
GPIO34 na batériu) — nemá interný pull-up, preto potrebuje **externý
pull-up rezistor 10kΩ medzi GPIO35 a 3.3V**.

```
3.3V ---[10kΩ]---+--- GPIO35
                  |
              Tlačidlo 8
                  |
                 GND
```

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

## Meranie napätia batérie (spínaný delič, nulový pokojový odber)

Namiesto trvalo pripojeného deliča (ktorý by neustále, aj vo vypnutom
stave, mierne vyťahoval prúd z batérie) používame **vysokostranový
spínač** — delič sa pripojí na batériu len na pár milisekúnd počas
merania, inak je odpojený.

### Potrebné súčiastky

- 1x **P-MOSFET** — BSS84 (Q1)
- 1x **N-MOSFET** — BS170 (Q2)
- 2x rezistor **100kΩ** (R_TOP, R_BOTTOM — samotný delič)
- 1x rezistor **10kΩ** (R_GATE_PULLUP — pull-up gate Q1 na BAT+)
- 1x rezistor **1kΩ** (sériový rezistor GPIO → gate Q2)
- 1x rezistor **10kΩ** (pull-down gate Q2 na GND)

### Zapojenie

```
BAT+ ---[S  Q1 (BSS84)  D]---[R_TOP 100kΩ]---+---[R_BOTTOM 100kΩ]--- GND
          |                                   |
          |                                 GPIO34 (ADC1, meranie)
     [R_GATE_PULLUP 10kΩ]
          |
        gate Q1 -------------------------[D  Q2 (BS170)  S]--- GND
                                                |
                                          [R_Q2_PULLDOWN 10kΩ]
                                                |
                                               GND
                                                |
                                          gate Q2
                                                |
                                          [1kΩ sériový]
                                                |
                                          GPIO23 (BATTERY_ENABLE_PIN)
```

**Princíp:**
- Pokoj (GPIO23 = LOW): Q2 vypnutý → gate Q1 ťahaný cez R_GATE_PULLUP
  na BAT+ → Vgs(Q1)=0 → Q1 vypnutý → delič úplne odpojený, **nulový
  pokojový prúd**.
- Meranie (GPIO23 = HIGH na pár ms): Q2 zopne → stiahne gate Q1 na GND
  → Q1 zopne → delič pripojený na batériu → ADC na GPIO34 prečíta
  napätie → firmvér hneď vypne GPIO23 späť na LOW.

Firmvér (`lib/BatteryMonitor::readVoltageMv()`) toto spínanie robí
automaticky pri každom meraní (5ms na ustálenie, potom priemer z 8
vzoriek ADC, potom vypnutie) — netreba nič riadiť ručne.

> Toto zapojenie (BSS84 + BS170) je overený vzor z iného projektu
> (Raspberry Pi Pico), len prepojený na iné GPIO čísla kvôli inému
> rozloženiu pinov na ESP32. Presné hodnoty rezistorov v pôvodnej
> schéme sa môžu mierne líšiť — vyššie uvedené hodnoty (100k/100k pre
> samotný delič) sú odvodené z nášho pôvodného jednoduchého návrhu a
> na funkčnosť princípu nemajú vplyv, len na presný pomer merania.

## Light sleep pri nečinnosti

Žiadne dodatočné súčiastky netreba — ide čisto o firmvér. Ak sa 30s
(`INACTIVITY_TIMEOUT_MS`) nestlačí žiadne tlačidlo a modul nič nehrá,
ESP32 prejde do **light sleep** (CPU zastavený, RAM aj stav programu
zachované, spotreba rádovo desiatky až stovky µA namiesto desiatok mA).

**Prebudenie:**
- Stlačením ktoréhokoľvek z 8 tlačidiel (`gpio_wakeup_enable()` na LOW
  úroveň — funguje priamo na existujúcom aktívne-LOW zapojení, netreba
  meniť HW ani pridávať diódy).
- Alebo periodicky každých 5s (`LIGHT_SLEEP_CHECK_INTERVAL_MS`) ako
  poistka — po takomto "kontrolnom" prebudení sa zariadenie hneď vráti
  späť do spánku, ak stále nie je čo robiť.

**Prečo light sleep, nie deep sleep:** klasický ESP32 (nie S2/S3) pri
deep sleep podporuje budenie na viacerých pinoch len v režime "všetky
musia byť LOW súčasne" alebo "ktorýkoľvek HIGH" — ani jedno nesedí na
"ktorékoľvek z 8 aktívne-LOW tlačidiel" bez dodatočného HW (diódy).
Light sleep toto obmedzenie nemá (funguje na úrovni GPIO matrixu, nie
len RTC), takže je to jednoduchšie riešenie bez zásahu do zapojenia
tlačidiel — za cenu vyššej spotreby v spánku než pri deep sleep.

