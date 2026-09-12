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

## 2. ESP32 ↔ DY1703A – UART

Firmware používa HardwareSerial UART2:

| ESP32 | GPIO | DY1703A | Poznámka |
|---|---:|---|---|
| TX2 | **GPIO17** | **RXD / IO1** | ESP32 → DY1703A |
| RX2 | **GPIO16** | **TXD / IO0** | DY1703A → ESP32 |
| GND | GND | GND | spoločná zem |
| 5V | VIN/5V | VCC | napájanie modulu |

## 3. Osem tlačidiel

Každé tlačidlo je samostatne medzi GPIO a GND. Všetky tlačidlá sú aktívne LOW.

| Skladba | ESP32 GPIO |
|---:|---:|
| 1 | **GPIO13** |
| 2 | **GPIO14** |
| 3 | **GPIO27** |
| 4 | **GPIO26** |
| 5 | **GPIO25** |
| 6 | **GPIO33** |
| 7 | **GPIO32** |
| 8 | **GPIO35** |

GPIO35 vyžaduje externý pull-up na 3.3 V.

## 4. Vibračný motorček

Firmware používa:

```text
MOTOR_PIN = GPIO4
MOTOR_ACTIVE_LEVEL = HIGH
```

Motor je riadený cez pôvodný tranzistorový obvod.

## 5. Meranie batérie

| Funkcia | ESP32 |
|---|---:|
| BAT_IN / ADC | **GPIO34** |
| BAT_EN | **GPIO23** |
| R_TOP | **100 kΩ** |
| R_BOTTOM | **100 kΩ** |

## 6. Prehľad GPIO

| GPIO | Funkcia |
|---:|---|
| 4 | vibračný motor |
| 13 | tlačidlo 1 |
| 14 | tlačidlo 2 |
| 16 | UART RX |
| 17 | UART TX |
| 23 | BAT_EN |
| 25 | tlačidlo 5 |
| 26 | tlačidlo 4 |
| 27 | tlačidlo 3 |
| 32 | tlačidlo 7 |
| 33 | tlačidlo 6 |
| 34 | BAT_IN |
| 35 | tlačidlo 8 |
