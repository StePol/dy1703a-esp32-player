# DY1703A ESP32 Audio Player

Prehrávač zvukov postavený na module **DY1703A** (DY-SV17F), riadený cez **ESP32**.
Zariadenie je napájané z Li-ion/LiPo batérie a ponúka tri spôsoby ovládania:

- **Fyzické tlačidlá** priamo na zariadení
- **Web rozhranie** (ESP32 ako WiFi AP alebo klient, jednoduchý webserver)
- **Bluetooth** (BLE) rozhranie pre ovládanie z mobilu

## Hardvér

| Komponent            | Poznámka                                  |
|-----------------------|--------------------------------------------|
| ESP32 DevKit          | riadiaca jednotka                          |
| DY1703A / DY-SV17F     | prehrávanie zvukov, UART mód               |
| LiPo batéria           | 3.7V                                       |
| TP4056                | nabíjací modul s ochranou                  |
| Boost prevodník 5V     | napr. MT3608                               |
| Reproduktor            | 4-8Ω, do 5W                                |
| Tlačidlá               | 2x 4-tlačidlová klávesnica = 8 skladieb, každé tlačidlo vlastný GPIO |
| Vibračný motorček       | 3V DC, spínaný hardvérovo cez BUSY výstup (BC547+BC557), bez zásahu ESP32 |

Schéma zapojenia: pozri [`docs/wiring.md`](docs/wiring.md).

### Nastavenie DY1703A (UART mód)

CON1 → GND, CON2 → GND, CON3 → 5V (odporúčané cez 3.3V výstup modulu alebo cez 10kΩ rezistor).
V tomto móde IO0 = TXD, IO1 = RXD modulu.

> Poznámka: na niektorých kusoch je potlač IO0/IO1 vnútorne prehodená — ak komunikácia
> nefunguje pri zapojení TX↔RX kríženo, skús TX–TX a RX–RX.

## Firmvér

Firmvér je postavený na **Arduino frameworku cez PlatformIO**.

### Build

```bash
pio run
pio run -t upload
pio device monitor
```

## Stav projektu

Rozpracované — základná kostra firmvéru a HW schéma.

## Licencia

MIT (uprav podľa potreby).
