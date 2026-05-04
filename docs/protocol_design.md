# LoRa / Sub-GHz protocol design notes

## 1. Regional compliance

STM32WL supports multiple regional stacks (ETSI, FCC, ARIB). A deployed product must respect **duty cycle** (EU sub-bands), **dwell time**, and **channel masks**. This assignment uses **868 MHz EU-type ISM** as a narrative default; parameters in code are **illustrative**.

## 2. Air interface parameters (reference)

| Parameter        | Reference value | Comment |
|------------------|-----------------|---------|
| Frequency        | 868.1 MHz       | Single-channel demo; production uses multichannel gateway. |
| Bandwidth        | 125 kHz         | Common LoRa BW. |
| Spreading factor | SF10            | Tradeoff: better sensitivity through damp grain vs longer ToA. |
| Coding rate      | 4/5             | LoRa CR. |
| Preamble           | 8 symbols       | Short for small payload; gateway tolerates standard sync. |
| TX power           | 14 dBm EIRP     | Regulatory cap depends on antenna gain and region. |
| CRC (physical)     | On              | Radio CRC + application CRC16 in payload (defense in depth). |

## 3. MAC mode

Nodes operate in a **simple uplink-only** profile:

- No confirmed Class A downlink in the happy path (saves gateway downlink duty and node RX energy).  
- Application-layer **retry_count** records retransmissions when the application decides to repeat after internal error (sensor fault) or optional **missing ACK** in a productized confirmed mode.

## 4. Anti-collision and scalability

At 12 h intervals and thousands of nodes, **pure ALOHA** is often acceptable. For denser deployments:

- **Randomized offset** per unit derived from `unit_id` to desynchronize bursts.  
- Gateway **multi-SF** demodulation (SX1302 class) or dual gateways for spatial diversity.

## 5. Association model

Each node is provisioned with:

- `unit_id`  
- `warehouse_id`  
- `gateway_id` (expected)

The uplink carries all three identifiers. The gateway:

1. Verifies CRC and roster.  
2. Confirms `warehouse_id` matches the site.  
3. Confirms `gateway_id` matches itself — rejects packets intended for a neighbor gateway when RF leakage occurs.

This is demonstrated in Python in `gateway_simulator/packet_parser.py` (`validate_uplink_packet`) and the demo scenarios in `gateway_simulator/main.py`.
