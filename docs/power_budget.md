# Power budget — disposable grain sensor node

Assumptions for **2+ year** operation on a **19 Ah** class bobbin cell at **3.6 V** nominal (Li-SOCl₂), with firmware behavior matching this repository’s reference design (12 h period, one RH/T sample burst, one short LoRa uplink, **STOP2** sleep between events).

> **Note:** Numbers are **order-of-magnitude engineering estimates** for a home assignment / interview discussion, not measured certification data. A production sign-off requires bench traces with calibrated ammeter and representative RF environment.

## 1. Duty cycle summary

| Event                     | Duration (typ.) | Current (typ.) | Charge per event |
|---------------------------|-----------------|----------------|------------------|
| STOP2 sleep + RTC         | 12 h            | ~1.2 µA        | 14.4 µAh         |
| Wake, RC cal, prep       | 50 ms           | 3 mA           | 0.042 µAh        |
| Sensor + ADC burst       | 15 ms           | 2 mA           | 0.008 µAh        |
| LoRa TX (14 dBm, ~20 B)  | ~40 ms          | 45 mA          | 0.50 µAh         |
| LoRa RX window (if any)  | 0 ms (disabled) | —              | 0 µAh            |

Airtime scales with spreading factor; reference uses **SF10** as compromise for grain attenuation vs energy (see `protocol_design.md`).

## 2. Average current (back-of-envelope)

Per 12 h cycle (single TX attempt, `retry_count = 0`):

- Sleep: `1.2 µA` average over interval dominates.  
- Active fractions are negligible when amortized over 12 h:

\[
I_{\text{avg}} \approx 1.2\,\mu\text{A} + \frac{Q_{\text{active}}}{12\,\text{h}}
\]

With \(Q_{\text{active}} \approx 0.55\,\mu\text{Ah}\) per cycle:

\[
\frac{0.55}{12}\,\mu\text{A} \approx 0.046\,\mu\text{A}
\Rightarrow I_{\text{avg}} \approx 1.25\,\mu\text{A}
\]

## 3. Two-year capacity check

Two years:

\[
Q_{\text{2y}} = 1.25\,\mu\text{A} \times 8760\,\text{h} \times 2 \approx 21.9\,\text{mAh}
\]

A **19 Ah** cell provides enormous margin; the real limiters are **self-discharge** (chemistry and temperature), **shelf life** before deployment, and **TX retries** / **cold temperature** derating. The large margin justifies occasional **2–3 retries** after link loss without redesigning the cell class.

## 4. Design tactics used to hit budget

1. **No continuous listening** — node is Class A–like: uplink-only for normal operation.  
2. **Sensor rail gating** — SHT4x VDD switched or I²C bus idle with pins low-leakage configured.  
3. **STOP2** not standby — SRAM retention for faster wake than standby, lower than run.  
4. **Minimal peripherals clocked** during sleep — GPIO analog mode where applicable.  
5. **Fixed small payload** — short ToA; no JSON on air.

## 5. Risk register

| Risk | Mitigation |
|------|------------|
| High grain loss | Lower SF / higher power / external antenna on gateway; more TX retries (costs energy). |
| Cell self-discharge | Supplier data at storage temperature; derate 2-year claim if deployed hot. |
| Brownout on TX | Bulk cap sizing; brownout flag in packet; reduce PA if needed. |
