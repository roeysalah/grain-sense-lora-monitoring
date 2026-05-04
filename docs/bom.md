# Indicative bill of materials — grain sensor puck (Qty 1)

> **Disclaimer:** Part numbers are representative for discussion and coursework; a production BOM requires mechanical, regulatory, and supply-chain review.

| Ref  | Description                    | Example part / family   | Notes |
|------|--------------------------------|-------------------------|-------|
| U1   | MCU + Sub-GHz SoC             | STM32WLE5xx           | Integrated radio reduces interconnect leakage. |
| U2   | RH / temperature sensor       | Sensirion SHT40       | I²C, low power one-shot measurement. |
| U3   | LDO, ultra-low Iq             | TPS7A02 / AP7354 class | Iq < 1 µA target when node sleeps. |
| C1–C4 | Bulk + decoupling            | X7R ceramics          | Local 100 nF + 10 µF at SoC; bulk for TX pulse. |
| R1–R2 | I²C pull-ups (if not int.)  | 4.7 kΩ                | Sized for bus capacitance; may disable between samples. |
| ANT  | PCB or wire antenna           | Matched network per ref design | Grain detunes; keep conservative matching budget. |
| BAT1 | Primary lithium cell          | Saft LS / equivalent   | Energy density vs transport regulations. |
| ENC  | Sealed enclosure              | Custom injection mold | Vent for RH with dust filter. |

## Gateway (separate product, not fully costed here)

- STM32WL module or SX1302-based concentrator  
- LTE-M modem (u-blox SARA / Quectel class)  
- Main supply: 5 V adapter + LiFePO₄ UPS optional for audit trail during outages
