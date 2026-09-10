# Schéma zapojenia

Tento dokument zodpovedá aktuálnemu firmvéru v `main` a zapojeniu podľa aktuálnej ručne kreslenej schémy.

## 1. Napájanie

```text
Li-ion/LiPo batéria
        │
        ▼
   nabíjací modul
        │
        ▼
   Boost 5 V
     ├──► ESP32 – VIN/5V
     └──► DY1703A – VCC

GND všetkých častí musí byť spoločná.
```

- DY1703A je napájaný z 5 V.
- ESP32 je napájaný na VIN/5V.
- Logická časť ESP32 pracuje s 3.3 V.
- Reproduktor sa pripája na `SPK+` a `SPK-` modulu DY1703A.
- Výstup zosilňovača `SPK+ / SPK-` nie je GND-referenčný výstup; reproduktor nepripájaj jedným vodičom na GND.

## 2. ESP32 ↔ DY1703A – UART

Firmware používa `HardwareSerial` UART2:

| ESP32 | GPIO | DY1703A | Poznámka |
|---|---:|---|---|
| TX2 | **GPIO17** | **RXD / IO1** | ESP32 → DY1703A |
| RX2 | **GPIO16** | **TXD / IO0** | DY1703A → ESP32 |
| GND | GND | GND | spoločná zem |
| 5V | VIN/5V | VCC | napájanie modulu |

Na aktuálnej schéme sú medzi ESP32 a UART vodičmi zakreslené **2× 1 kΩ sériové rezistory** – jeden v TX vetve a jeden v RX vetve. Toto zapojenie ponechaj:

```text
ESP32 GPIO17 (TX2) ──[1kΩ]──► DY1703A RXD / IO1
ESP32 GPIO16 (RX2) ◄─[1kΩ]─── DY1703A TXD / IO0
ESP32 GND ──────────────────── DY1703A GND
```

### CON piny DY1703A

Podľa aktuálneho zapojenia modulu sú CON piny nastavené:

| DY1703A | Zapojenie |
|---|---|
| CON1 | 10 kΩ → GND |
| CON2 | 10 kΩ → +5 V |
| CON3 | 10 kΩ → +5 V |

**Dôležité:** motor už nie je riadený cez CON3/BUSY. Motor je pripojený na **GPIO4 ESP32**. CON3 preto v aktuálnom zapojení nesmie byť v dokumentácii uvádzaný ako vstup do motorového obvodu.

## 3. Osem tlačidiel

Každé tlačidlo je samostatne medzi GPIO a GND. Všetky tlačidlá sú **aktívne LOW**.

| Tlačidlo | Skladba | ESP32 GPIO | Režim |
|---:|---:|---:|---|
| KEY 1 / tlačidlo 1 | 1 | **GPIO13** | INPUT_PULLUP |
| KEY 1 / tlačidlo 2 | 2 | **GPIO14** | INPUT_PULLUP |
| KEY 1 / tlačidlo 3 | 3 | **GPIO27** | INPUT_PULLUP |
| KEY 1 / tlačidlo 4 | 4 | **GPIO26** | INPUT_PULLUP |
| KEY 2 / tlačidlo 1 | 5 | **GPIO25** | INPUT_PULLUP |
| KEY 2 / tlačidlo 2 | 6 | **GPIO33** | INPUT_PULLUP |
| KEY 2 / tlačidlo 3 | 7 | **GPIO32** | INPUT_PULLUP |
| KEY 2 / tlačidlo 4 | 8 | **GPIO35** | externý pull-up |

Zapojenie prvých siedmich tlačidiel:

```text
GPIO13 ── tlačidlo 1 ── GND
GPIO14 ── tlačidlo 2 ── GND
GPIO27 ── tlačidlo 3 ── GND
GPIO26 ── tlačidlo 4 ── GND
GPIO25 ── tlačidlo 5 ── GND
GPIO33 ── tlačidlo 6 ── GND
GPIO32 ── tlačidlo 7 ── GND
```

GPIO35 nemá interný pull-up, preto:

```text
3.3V ──[10kΩ]──┬── GPIO35
               │
            tlačidlo 8
               │
              GND
```

GPIO12 sa pre tlačidlo nepoužíva.

## 4. Vibračný motorček – aktuálne zapojenie

**Toto je hlavná zmena oproti starej dokumentácii.** Firmware používa:

```text
MOTOR_PIN = GPIO4
MOTOR_ACTIVE_LEVEL = HIGH
```

Motor teda **nie je pripojený na CON3 DY1703A**. Ovláda ho GPIO4 cez pôvodný dvojtranzistorový obvod zakreslený na aktuálnej schéme.

### Signálová časť

```text
ESP32 GPIO4
    │
    ▼
  1N4148
    │
  10 kΩ
    │
    ▼
  báza BC546 (NPN)
    │
    ├── emitor → GND
    │
    └── kolektor ──[1 kΩ]──► báza BC557 (PNP)
```

### Výkonová časť

```text
                 +3.3V
                   │
                emitor
                 BC557
                kolektor
                   │
                 [10Ω]
                   │
                 MOTOR
                   │
                  GND
```

Flyback dióda **1N4148** je zapojená paralelne k motoru:

```text
             ┌─────|<|─────┐
             │   1N4148    │
             │             │
          [10Ω]           GND
             │
           MOTOR
             │
            GND
```

Dióda musí byť zapojená tak, aby pri normálnom napájaní motora nebola vodivo otvorená a pri vypnutí motora odvádzala indukovanú špičku.

### Funkcia

```text
GPIO4 = LOW   → BC546 vypnutý → BC557 vypnutý → motor STOP
GPIO4 = HIGH  → BC546 zapnutý → stiahne bázu BC557 → BC557 zapnutý → motor ON
```

Firmware navyše povoľuje motor iba vtedy, keď:

1. DY1703A hlási prehrávanie,
2. je zvolená platná skladba 1–8,
3. pre danú skladbu je v nastaveniach zapnutá vibrácia.

Motor sa pri STOP alebo PAUSE vypína.

> Na ručnej schéme je NPN tranzistor označený **BC546**. Dokumentácia preto používa toto označenie. Pri osádzaní vždy over konkrétny typ tranzistora a jeho pinout podľa datasheetu výrobcu.

## 5. Meranie batérie

Firmware používa dva samostatné signály:

| Funkcia | ESP32 |
|---|---:|
| BAT_IN / ADC meranie | **GPIO34** |
| BAT_EN / zapnutie meracieho obvodu | **GPIO23** |

Aktuálne firmware má:

```text
BATTERY_ADC_PIN    = GPIO34
BATTERY_ENABLE_PIN = GPIO23
R_TOP              = 100 kΩ
R_BOTTOM           = 100 kΩ
```

Logické zapojenie meracieho bodu:

```text
BATTERY / merací delič
        │
        ▼
     BATT_IN
        │
        ▼
     GPIO34
```

Ovládanie meracieho obvodu:

```text
GPIO23 = BAT_EN
```

Firmware nastaví `GPIO23 = HIGH`, počká na ustálenie meracieho obvodu, vykoná 8 ADC meraní a potom nastaví `GPIO23 = LOW`.

**Poznámka:** aktuálna ručná schéma jasne označuje body `BATT_IN` a `BATT_EN`, ale nezobrazuje dostatočne jednoznačne celý spínací obvod batériového deliča. Preto tu nie je uvedené neoverené zapojenie MOSFETov z predchádzajúcej verzie dokumentácie.

## 6. Prehľad všetkých GPIO

| GPIO | Funkcia |
|---:|---|
| **4** | vibračný motor – riadiaci signál |
| **13** | tlačidlo skladby 1 |
| **14** | tlačidlo skladby 2 |
| **16** | UART2 RX z DY1703A |
| **17** | UART2 TX do DY1703A |
| **23** | BAT_EN – povolenie merania batérie |
| **25** | tlačidlo skladby 5 |
| **26** | tlačidlo skladby 4 |
| **27** | tlačidlo skladby 3 |
| **32** | tlačidlo skladby 7 |
| **33** | tlačidlo skladby 6 |
| **34** | BAT_IN – ADC batérie, input only |
| **35** | tlačidlo skladby 8, input only + externý 10 kΩ pull-up |

## 7. Dôležité rozdiely oproti starej wiring dokumentácii

- **GPIO4 → motor** je správne aktuálne zapojenie.
- **CON3 → motor** už neplatí.
- NPN v motorovom obvode je podľa aktuálnej schémy **BC546**.
- UART používa **GPIO17 TX2** a **GPIO16 RX2**, v schéme sú 1 kΩ sériové rezistory.
- Batéria používa **GPIO34 = BATT_IN** a **GPIO23 = BATT_EN**.
- Tlačidlá používajú presne GPIO `{13, 14, 27, 26, 25, 33, 32, 35}`.
- GPIO35 vyžaduje externý 10 kΩ pull-up na 3.3 V.

Tento dokument je referenčný wiring pre aktuálny firmware projektu.