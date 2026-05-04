"""
Adaptive RF link quality analysis for the Grain Sense gateway simulator.

Purpose of this module:
    Encapsulate RSSI/SNR classification and recommended RF actions (spreading
    factor, TX power, infrastructure) so gateway simulation reads like a small
    product feature, not ad-hoc prints inside main.

This module keeps RF decision logic out of `main.py` so simulation flow remains
simple and testable.
"""

from __future__ import annotations


def classify_link(rssi: float, snr: float) -> str:
    """
    Classify RF link quality based on RSSI (dBm) and SNR (dB).

    Thresholds (assignment):
    - GOOD: RSSI > -90 dBm and SNR > 10 dB
    - WEAK: RSSI between -110 and -90 (inclusive) and SNR between 0 and 10 (inclusive)
    - CRITICAL: RSSI < -110 dBm and SNR < 0 dB

    Any other combination is treated as WEAK (conservative default for grain loss).
    """
    if rssi > -90 and snr > 10:
        return "GOOD"
    if rssi < -110 and snr < 0:
        return "CRITICAL"
    if -110 <= rssi <= -90 and 0 <= snr <= 10:
        return "WEAK"
    return "WEAK"


def recommend_action(rssi: float, snr: float) -> dict[str, str]:
    """
    Return adaptive RF recommendations based on link quality.

    Returned keys:
    - link_quality
    - recommendation
    - suggested_spreading_factor
    - suggested_tx_power
    """
    quality = classify_link(rssi, snr)

    if quality == "GOOD":
        return {
            "link_quality": quality,
            "recommendation": "Keep current settings; link margin is healthy.",
            "suggested_spreading_factor": "SF7-SF9",
            "suggested_tx_power": "Keep current TX power",
        }

    if quality == "WEAK":
        return {
            "link_quality": quality,
            "recommendation": "Increase spreading factor (SF10-SF11); consider increasing TX power.",
            "suggested_spreading_factor": "SF10-SF11",
            "suggested_tx_power": "Increase TX power if battery budget allows",
        }

    return {
        "link_quality": quality,
        "recommendation": (
            "Use SF12, increase TX power, and consider relocating the gateway "
            "or adding an additional gateway."
        ),
        "suggested_spreading_factor": "SF12",
        "suggested_tx_power": "Use highest allowed TX power",
    }
