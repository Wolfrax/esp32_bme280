# Battery Charging (Olimex ESP32-C3-DevKit-LiPo Rev B)

## Finding: charge current is capped at 100 mA on-board, not by the USB source

The board's onboard LiPo charger IC is a **BL4054B-42TPRN** (SOT23-5, linear charger).
Its charge current is set entirely by resistor `R5` on the `PROG` pin:

```
I(Bat) = 1000 / Rprog(kΩ)
```

`R5 = 10kΩ` on this board → **I(Bat) = 100 mA**, fixed in hardware.

This means:
- Charging speed is capped at 100 mA regardless of the power source — a laptop USB port,
  a wall charger, or a USB-PD brick all supply far more than 100 mA, so none of them
  change the charge rate. The bottleneck is `R5`, not the source.
- A 2000 mAh battery at 100 mA (0.05C) takes roughly **20+ hours** for the
  constant-current phase alone, plus additional time tapering off in constant-voltage
  mode near full charge.
- This matches the board's intended use: Olimex's own recommended battery for this
  charger is a 1400 mAh pack (`BATTERY-LIPO1400mAh`), not a 2000 mAh one.

## Speeding it up

The only way to charge faster is a hardware rework: replace `R5` with a lower-value
resistor. Per the IC's documented range on the schematic (`Rprog = 2k to 10k`,
absolute IC ceiling 800 mA):

- `R5 = 2kΩ` → ~500 mA (≈4-5 hours for the CC phase on a 2000 mAh pack)

This is SMD rework on a 0402 resistor and hasn't been done — flagged here for future
reference, not yet acted on.

## Source

- [ESP32-C3-DevKit-Lipo Rev B schematic](https://github.com/OLIMEX/ESP32-C3-DevKit-Lipo/blob/main/HARDWARE/ESP32-C3-DevKit-Lipo_Rev_B/ESP32-C3-DevKit-Lipo_Rev_B.pdf)
- [OLIMEX/ESP32-C3-DevKit-Lipo repo](https://github.com/OLIMEX/ESP32-C3-DevKit-Lipo)
