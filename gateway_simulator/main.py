"""
Grain Sense warehouse gateway simulator (Python).

Purpose of this file:
    CLI entry point that loads gateway provisioning, simulates a batch of
    uplink receptions (decoded as dicts), validates each frame, classifies RF
    link quality (RSSI/SNR), prints human-readable reports, logs accepted
    telemetry to CSV, and prints a run summary.

Simulates gateway-side packet validation plus adaptive RF quality decisions for
harsh grain storage attenuation scenarios.
"""

from __future__ import annotations

import argparse
import csv
import random
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from link_quality import classify_link, recommend_action
from packet_parser import (
    GatewayAuth,
    ValidationResult,
    format_unit_label,
    load_gateway_auth,
    make_packet_dict,
    validate_uplink_packet,
)

# CSV columns for accepted packets
LOG_COLUMNS: list[str] = [
    "timestamp",
    "unit_id",
    "temperature",
    "humidity",
    "battery_mv",
    "sequence_number",
    "rssi",
    "snr",
    "link_quality",
]


def simulate_rf_metrics() -> tuple[float, float]:
    """Generate realistic RF metrics for indoor grain attenuation conditions."""
    rssi = round(random.uniform(-120.0, -70.0), 1)
    snr = round(random.uniform(-10.0, 12.0), 1)
    return rssi, snr


def log_accepted_packet(log_path: Path, packet: dict[str, Any], link_quality: str) -> None:
    """Append one accepted uplink row to the CSV log."""
    row = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "unit_id": packet["unit_id"],
        "temperature": int(packet["temperature_c_x100"]) / 100.0,
        "humidity": int(packet["humidity_rh_x100"]) / 100.0,
        "battery_mv": packet["battery_mv"],
        "sequence_number": packet["sequence_number"],
        "rssi": packet.get("rssi_dbm", ""),
        "snr": packet.get("snr_db", ""),
        "link_quality": link_quality,
    }
    file_exists = log_path.is_file() and log_path.stat().st_size > 0
    with log_path.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=LOG_COLUMNS, extrasaction="ignore")
        if not file_exists:
            writer.writeheader()
        writer.writerow(row)


def print_packet_report(
    result: ValidationResult,
    packet: dict[str, Any],
    rf_recommendation: dict[str, str],
) -> None:
    """Print a clean block report for each packet, accepted or rejected."""
    unit_id = int(packet["unit_id"])
    label = format_unit_label(unit_id)
    rssi = float(packet["rssi_dbm"])
    snr = float(packet["snr_db"])
    status = "ACCEPTED" if result.decision == "ACCEPT" else f"REJECTED ({result.reason})"

    print("--------------------------------")
    print(label)
    print(f"Status: {status}")
    print(f"RSSI: {rssi:.1f} dBm")
    print(f"SNR: {snr:.1f} dB")
    print(f"Link Quality: {rf_recommendation['link_quality']}")
    print(f"Recommendation: {rf_recommendation['recommendation']}")


def print_summary(total: int, accepted: int, rejected: int, quality_counts: Counter[str]) -> None:
    """Print end-of-run simulation summary."""
    print("")
    print(f"Total packets: {total}")
    print(f"Accepted: {accepted}")
    print(f"Rejected: {rejected}")
    print("")
    print("Link quality distribution:")
    print(f"GOOD: {quality_counts.get('GOOD', 0)}")
    print(f"WEAK: {quality_counts.get('WEAK', 0)}")
    print(f"CRITICAL: {quality_counts.get('CRITICAL', 0)}")


def build_demo_packets(auth: GatewayAuth) -> list[dict[str, Any]]:
    """
    Build a small list of simulated receptions covering success and failure modes.

    Order is chosen so beginners can read top-to-bottom and see each reject reason.
    """
    g, w = auth.gateway_id, auth.warehouse_id

    packets: list[dict[str, Any]] = []

    # 1) Valid packet from an authorized unit
    packets.append(
        make_packet_dict(
            unit_id=3,
            gateway_id=g,
            warehouse_id=w,
            sequence_number=101,
            temperature_c_x100=2150,
            humidity_rh_x100=5800,
            battery_mv=3600,
            retry_count=0,
        )
    )

    # 2) Authorized unit but wrong gateway_id in the frame
    packets.append(
        make_packet_dict(
            unit_id=22,
            gateway_id=60000,
            warehouse_id=w,
            sequence_number=102,
            temperature_c_x100=2100,
            humidity_rh_x100=5700,
            battery_mv=3550,
            retry_count=0,
        )
    )

    # 3) Authorized unit but warehouse does not match this gateway's site
    packets.append(
        make_packet_dict(
            unit_id=91,
            gateway_id=g,
            warehouse_id=99999,
            sequence_number=103,
            temperature_c_x100=2050,
            humidity_rh_x100=5600,
            battery_mv=3580,
            retry_count=0,
        )
    )

    # 4) Valid CRC and IDs for *some* site, but unit_id not on this gateway's whitelist
    packets.append(
        make_packet_dict(
            unit_id=999,
            gateway_id=g,
            warehouse_id=w,
            sequence_number=104,
            temperature_c_x100=2000,
            humidity_rh_x100=5000,
            battery_mv=3700,
            retry_count=0,
        )
    )

    # 5) Authorized unit with deliberately wrong CRC (tampered or noisy air)
    bad_crc = make_packet_dict(
        unit_id=42241,
        gateway_id=g,
        warehouse_id=w,
        sequence_number=105,
        temperature_c_x100=1980,
        humidity_rh_x100=5100,
        battery_mv=3620,
        retry_count=1,
    )
    bad_crc["crc"] = int(bad_crc["crc"]) ^ 0xFFFF
    packets.append(bad_crc)

    # 6) Another valid packet (different authorized unit)
    packets.append(
        make_packet_dict(
            unit_id=42242,
            gateway_id=g,
            warehouse_id=w,
            sequence_number=106,
            temperature_c_x100=2200,
            humidity_rh_x100=6000,
            battery_mv=3650,
            retry_count=0,
        )
    )

    return packets


def run_simulation(auth: GatewayAuth, log_path: Path) -> None:
    """Process each demo packet: validate, analyze RF quality, print, and log."""
    total_packets = 0
    accepted_packets = 0
    rejected_packets = 0
    quality_counts: Counter[str] = Counter()

    for pkt in build_demo_packets(auth):
        total_packets += 1
        # Simulated receiver-side RF metadata for this packet reception.
        rssi, snr = simulate_rf_metrics()
        pkt["rssi_dbm"] = rssi
        pkt["snr_db"] = snr

        result = validate_uplink_packet(pkt, auth)
        quality = classify_link(rssi, snr)
        quality_counts[quality] += 1
        recommendation = recommend_action(rssi, snr)

        print_packet_report(result, pkt, recommendation)
        print()

        if result.decision == "ACCEPT":
            accepted_packets += 1
            # Persist only accepted telemetry rows to mirror gateway forwarding policy.
            log_accepted_packet(log_path, pkt, quality)
        else:
            rejected_packets += 1

    print_summary(total_packets, accepted_packets, rejected_packets, quality_counts)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Simulate a warehouse gateway receiving LoRa uplink dictionaries.",
    )
    here = Path(__file__).resolve().parent
    parser.add_argument(
        "--auth",
        type=Path,
        default=here / "authorized_units.json",
        help="Path to authorized_units.json",
    )
    parser.add_argument(
        "--log",
        type=Path,
        default=here / "received_packets_log.csv",
        help="CSV path for accepted packets",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Optional RNG seed for reproducible RSSI/SNR simulation",
    )
    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    auth = load_gateway_auth(args.auth)
    run_simulation(auth, args.log)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
