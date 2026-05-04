# Grain Sense uplink packet format

**Repository role:** Single source of truth for the 19-byte application payload (`GrainUplinkPacket_t`). Any change here must stay aligned with `firmware/app/grain_packet.*` and `gateway_simulator/packet_parser.py`.

Application payload from sensor node to gateway (inside the LoRa MAC payload). All multi-byte fields are **little-endian** on the STM32WL host.

## Version

Single layout revision **v1** (no separate version byte — length + CRC scope identify the frame).

## Packed C layout (`GrainUplinkPacket_t`)

Total size: **19 bytes**.

| Offset | Size | Field                    | Type     | Description |
|--------|------|--------------------------|----------|-------------|
| 0      | 2    | `unit_id`                | `uint16` | Factory / provisioning ID |
| 2      | 2    | `gateway_id`             | `uint16` | Expected gateway (filtering) |
| 4      | 2    | `warehouse_id`           | `uint16` | Site / tenant scope |
| 6      | 4    | `sequence_number`        | `uint32` | Monotonic per device lifetime (backup registers in production) |
| 10     | 2    | `temperature_c_x100`   | `int16`  | °C × 100 (e.g. `2150` → 21.50 °C) |
| 12     | 2    | `humidity_rh_x100`       | `uint16` | %RH × 100 (e.g. `5825` → 58.25 %) |
| 14     | 2    | `battery_mv`             | `uint16` | Battery terminal millivolts |
| 16     | 1    | `retry_count`            | `uint8`  | PHY attempt index for this frame (`0` first try, `1` first retry, `2` second retry) |
| 17     | 2    | `crc`                    | `uint16` | CRC-16/CCITT-FALSE over **bytes 0–16** inclusive |

## CRC

- Polynomial `0x1021`, initial `0xFFFF`, MSB-first, no input/output reflection (CCITT-FALSE).  
- Compute over the **17** bytes starting at offset 0 (all fields through `retry_count`). The `crc` field itself is **not** included in the checksum input.  
- Store `crc` little-endian at offset 17.

## Gateway filtering

Same rules as before: roster membership, `warehouse_id` match, `gateway_id` match (see `gateway_simulator/packet_parser.py`).
