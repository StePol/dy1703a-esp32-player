# Schéma zapojenia

Tento dokument zodpovedá aktuálnemu firmvéru. **Zdrojom pravdy pre GPIO a systémové hardvérové nastavenia je `include/config.h`.**

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

## 2. ESP32 ↔ DY1703A – UART

Firmware používa HardwareSerial UART2:

| ESP32 | GPIO | DY1703A | Poznámka |
|---|---:|---|---|
| TX2 | **GPIO17** | **RXD / IO1** | ESP32 → DY1703A |
| RX2 | **GPIO16** | **TXD / IO0** | DY1703A → ESP32 |
| GND | GND | GND | spoločná zem |
| 5V | VIN/5V | VCC | napájanie modulu |

Parametre UART:

- baudrate: **9600**
- formát: **8N1**
- UART: **UART2**

## 3. Osem tlačidiel

Každé tlačidlo je samostatne medzi príslušným GPIO a GND. Všetky tlačidlá sú aktívne LOW.

Aktuálne mapovanie podľa `include/config.h`:

| Tlačidlo | Skladba | ESP32 GPIO |
|---:|---:|---:|
| KEY1 | 1 | **GPIO25** |
| KEY2 | 2 | **GPIO33** |
| KEY3 | 3 | **GPIO32** |
| KEY4 | 4 | **GPIO35** |
| KEY5 | 5 | **GPIO13** |
| KEY6 | 6 | **GPIO14** |
| KEY7 | 7 | **GPIO27** |
| KEY8 | 8 | **GPIO26** |

### GPIO35

GPIO35 je vstup-only pin ESP32 a nemá interný pull-up. Pre tlačidlo KEY4 je preto potrebný **externý pull-up na 3.3 V**.

Ostatné tlačidlá používajú príslušné GPIO podľa firmvérovej konfigurácie.

## 4. Vibračný motorček

Firmware používa:

```text
MOTOR_PIN = GPIO4
MOTOR_ACTIVE_LEVEL = HIGH
```

GPIO4 ovláda externý tranzistorový stupeň. Motor sa teda nepripája priamo na GPIO ESP32.

Motor je aktívny počas prehrávania skladby, ak je pre danú skladbu zapnutá vibrácia.

## 5. Meranie batérie

| Funkcia | ESP32 |
|---|---:|
| BAT_IN / ADC | **GPIO34** |
| BAT_EN | **GPIO23** |
| R_TOP | **100 kΩ** |
| R_BOTTOM | **100 kΩ** |

Batériové napätie sa meria cez spínaný odporový delič. GPIO23 slúži na jeho povolenie iba počas merania, aby sa znížil odber batérie.

Firmware používa 12-bit ADC a 11 dB attenuáciu.

## 6. Prehľad GPIO

| GPIO | Funkcia |
|---:|---|
| 4 | vibračný motor |
| 13 | tlačidlo 5 |
| 14 | tlačidlo 6 |
| 16 | UART RX |
| 17 | UART TX |
| 23 | BAT_EN |
| 25 | tlačidlo 1 |
| 26 | tlačidlo 8 |
| 27 | tlačidlo 7 |
| 32 | tlačidlo 3 |
| 33 | tlačidlo 2 |
| 34 | BAT_IN / ADC |
| 35 | tlačidlo 4, input-only |

## 7. Dôležité upozornenie pred výrobou

Tento dokument popisuje **aktuálne GPIO mapovanie podľa `include/config.h`**. Pri zapájaní hardvéru treba použiť práve toto mapovanie.

Najmä poradie tlačidiel je:

```text
Skladba 1 → GPIO25
Skladba 2 → GPIO33
Skladba 3 → GPIO32
Skladba 4 → GPIO35
Skladba 5 → GPIO13
Skladba 6 → GPIO14
Skladba 7 → GPIO27
Skladba 8 → GPIO26
```

Pred výrobou finálnej dosky je potrebné overiť, že KiCad schéma a skutočné zapojenie zodpovedajú tomuto mapovaniu.
