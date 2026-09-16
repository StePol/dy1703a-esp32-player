# LX-LCBST KiCad knižnica

Obsahuje symbol, footprint a jednoduchý 3D model pre modul
"TP4056 USB-C Li-ion nabíjačka + DC-DC boost" (LX-LCBST).

⚠️ POZOR: fyzické rozmery (doska ~20×17 mm, rozostup pätiek 4 mm,
výška konektora ~3,6 mm) sú ODHADNUTÉ z pravítka na fotke, nie
z presného merania. Pred výrobou plošného spoja si modul premeraj
posuvným meradlom a footprint podľa potreby uprav v KiCad
Footprint Editore.

## Obsah
- `LX-LCBST.kicad_sym` – schematická značka (6 pinov: IN+, IN-, B+, B-, VO+, VO-)
- `LX-LCBST.pretty/LX-LCBST_Module.kicad_mod` – footprint (8 pätiek, priechodzie)
- `LX-LCBST.pretty/LX-LCBST_Module.wrl` – 3D model (doska + blok konektora)

## Inštalácia (KiCad 7/8)

1. Rozbaľ ZIP niekam natrvalo, napr. `Dokumenty/KiCad/knižnice/LX-LCBST_KiCad_Library`.
2. V KiCad otvor **Preferences → Manage Symbol Libraries…**
   - záložka *Global* alebo *Project* → **Add existing library**
   - vyber súbor `LX-LCBST.kicad_sym`, nickname napr. `LX-LCBST`
3. V KiCad otvor **Preferences → Manage Footprint Libraries…**
   - **Add existing library** → vyber priečinok `LX-LCBST.pretty`
   - nickname napr. `LX-LCBST`
4. V schéme potom nájdeš symbol pod menom knižnice `LX-LCBST` →
   `LX-LCBST_Boost_Charger`, footprint sa napojí automaticky
   (pole Footprint je predvyplnené).

## 3D model
Cesta k modelu je v footprinte nastavená ako
`${KIPRJMOD}/LX-LCBST.pretty/LX-LCBST_Module.wrl`, čo funguje len ak
je knižnica priamo v priečinku KiCad projektu. Ak knižnicu držíš
inde, over/priprav si cestu v **Footprint Properties → 3D Models**:
buď oprav cestu ručne, alebo si over relatívnu cestu, alebo
klikni "Add 3D Shape" a vyber `LX-LCBST_Module.wrl` v priečinku
`LX-LCBST.pretty` – scale nechaj na 0.393701 / 0.393701 / 0.393701.

## Piny
| Pin | Popis                          |
|-----|---------------------------------|
| IN+ | vstup nabíjania (USB-C, +)      |
| IN- | vstup nabíjania (GND)           |
| B+  | Li-ion článok (+)               |
| B-  | Li-ion článok (-)               |
| VO+ | boost výstup (+), 2× vyvedené   |
| VO- | boost výstup (-), 2× vyvedené   |
