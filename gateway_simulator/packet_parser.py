"""
Packet validation for the Grain Sense gateway simulator.

Purpose of this module:
    Pure policy + framing logic shared by the simulator: serialize payloads for
    CRC, validate whitelist and site IDs, and build demo packet dictionaries.
    Keeps validation testable without hardware or radio stacks.

Incoming data is modeled as a **dictionary** (as if the LoRa MAC layer already
decoded the binary uplink into fields). This module checks CRC and policy rules,
then returns ACCEPT or REJECT with a short machine-readable reason.

The binary layout matches firmware ``GrainUplinkPacket_t`` (19 bytes on the wire);
CRC is CCITT-FALSE over the first 17 bytes (all fields through ``retry_count``).
"""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Final

# --- Keys expected in each uplink dictionary (after "demodulation") ---

REQUIRED_FIELDS: Final[tuple[str, ...]] = (
    "unit_id",
    "gateway_id",
    "warehouse_id",
    "sequence_number",
    "temperature_c_x100",
    "humidity_rh_x100",
    "battery_mv",
    "retry_count",
    "crc",
)


@dataclass(frozen=True)
class GatewayAuth:
    """
    Who this gateway is and which sensor units it will accept.

    Attributes:
        gateway_id: This gateway's identity (must match packet ``gateway_id``).
        warehouse_id: This site's warehouse scope (must match packet).
        authorized_unit_ids: Units allowed to send through this gateway.
    """

    gateway_id: int
    warehouse_id: int
    authorized_unit_ids: frozenset[int]


@dataclass(frozen=True)
class ValidationResult:
    """Outcome of ``validate_uplink_packet``."""

    decision: str  # "ACCEPT" or "REJECT"
    reason: str  # empty when ACCEPT; otherwise e.g. "invalid CRC"


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (same polynomial as the embedded C code)."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def pack_packet_body(packet: dict[str, Any]) -> bytes:
    """
    Serialize the 17-byte payload used for CRC (little-endian, same as STM32).

    This is the canonical binary image of the packet **excluding** the final
    ``crc`` field (CRC is computed over these bytes only).
    """
    return struct.pack(
        "<HHHIhHHB",
        int(packet["unit_id"]) & 0xFFFF,
        int(packet["gateway_id"]) & 0xFFFF,
        int(packet["warehouse_id"]) & 0xFFFF,
        int(packet["sequence_number"]) & 0xFFFFFFFF,
        int(packet["temperature_c_x100"]),
        int(packet["humidity_rh_x100"]) & 0xFFFF,
        int(packet["battery_mv"]) & 0xFFFF,
        int(packet["retry_count"]) & 0xFF,
    )


def compute_crc(packet: dict[str, Any]) -> int:
    """Return the CRC value that should accompany these fields."""
    return crc16_ccitt_false(pack_packet_body(packet))


def format_unit_label(unit_id: int) -> str:
    """
    Human-friendly label for console lines (e.g. UNIT_003, UNIT_42241).

    Small numeric IDs use three digits with leading zeros, matching the
    assignment examples; larger provisioning IDs print in full.
    """
    if unit_id < 1000:
        return f"UNIT_{unit_id:03d}"
    return f"UNIT_{unit_id}"


def load_gateway_auth(config_path: Path) -> GatewayAuth:
    """Load ``authorized_units.json`` into a ``GatewayAuth`` object."""
    raw = json.loads(config_path.read_text(encoding="utf-8"))
    return GatewayAuth(
        gateway_id=int(raw["gateway_id"]),
        warehouse_id=int(raw["warehouse_id"]),
        authorized_unit_ids=frozenset(int(x) for x in raw["authorized_unit_ids"]),
    )


def _missing_fields(packet: dict[str, Any]) -> list[str]:
    return [k for k in REQUIRED_FIELDS if k not in packet]


def validate_uplink_packet(packet: dict[str, Any], auth: GatewayAuth) -> ValidationResult:
    """
    Validate one uplink packet against gateway policy and CRC.

    Checks (in order):
        1. Required keys present and integral types usable
        2. CRC matches payload
        3. ``gateway_id`` matches this gateway
        4. ``warehouse_id`` matches this warehouse
        5. ``unit_id`` is in the whitelist

    Returns:
        ValidationResult with decision "ACCEPT" or "REJECT" and a short reason.

    Note:
        CRC is checked before gateway/warehouse so corrupted bits are not
        mis-attributed to a policy violation.
    """
    missing = _missing_fields(packet)
    if missing:
        return ValidationResult("REJECT", f"missing fields: {', '.join(missing)}")

    try:
        body = pack_packet_body(packet)
        wire_crc = int(packet["crc"]) & 0xFFFF
    except (TypeError, ValueError, struct.error) as exc:
        return ValidationResult("REJECT", f"malformed field: {exc}")

    if crc16_ccitt_false(body) != wire_crc:
        return ValidationResult("REJECT", "invalid CRC")

    gid = int(packet["gateway_id"])
    if gid != auth.gateway_id:
        return ValidationResult("REJECT", "wrong gateway_id")

    wid = int(packet["warehouse_id"])
    if wid != auth.warehouse_id:
        return ValidationResult("REJECT", "wrong warehouse_id")

    uid = int(packet["unit_id"])
    if uid not in auth.authorized_unit_ids:
        return ValidationResult("REJECT", "unknown unit")

    return ValidationResult("ACCEPT", "")


def make_packet_dict(
    *,
    unit_id: int,
    gateway_id: int,
    warehouse_id: int,
    sequence_number: int,
    temperature_c_x100: int,
    humidity_rh_x100: int,
    battery_mv: int,
    retry_count: int,
    crc: int | None = None,
    rssi_dbm: float | None = None,
    snr_db: float | None = None,
) -> dict[str, Any]:
    """
    Build a packet dictionary and optionally fill a correct CRC.

    Extra keys ``rssi_dbm`` / ``snr_db`` simulate RF metadata added by the
    receiver driver (not part of the signed uplink payload).
    """
    d: dict[str, Any] = {
        "unit_id": unit_id,
        "gateway_id": gateway_id,
        "warehouse_id": warehouse_id,
        "sequence_number": sequence_number,
        "temperature_c_x100": temperature_c_x100,
        "humidity_rh_x100": humidity_rh_x100,
        "battery_mv": battery_mv,
        "retry_count": retry_count,
        "crc": 0,
    }
    d["crc"] = crc if crc is not None else compute_crc(d)
    if rssi_dbm is not None:
        d["rssi_dbm"] = rssi_dbm
    if snr_db is not None:
        d["snr_db"] = snr_db
    return d
