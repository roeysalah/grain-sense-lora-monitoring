/**
 * @file grain_packet.h
 * @brief Packed uplink frame exchanged between sensor node and gateway (little-endian).
 *
 * Wire layout matches `protocol/packet_format.md`. CRC-16/CCITT-FALSE is computed
 * over all bytes preceding the `crc` field (host order in struct = wire order on LE MCU).
 */
#ifndef GRAIN_PACKET_H
#define GRAIN_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/** Total on-air application payload size */
#define GRAIN_UPLINK_WIRE_SIZE (19u)

/**
 * @note `__attribute__((packed))` avoids implicit padding before `crc`.
 *       Do not reorder fields without updating CRC range and gateway parser.
 */
typedef struct __attribute__((packed)) {
  uint16_t unit_id;
  uint16_t gateway_id;
  uint16_t warehouse_id;
  uint32_t sequence_number;
  int16_t temperature_c_x100; /**< Degrees Celsius * 100 (e.g. 2150 = 21.50 C) */
  uint16_t humidity_rh_x100;  /**< Relative humidity * 100 (e.g. 5825 = 58.25 %) */
  uint16_t battery_mv;
  uint8_t retry_count; /**< Retransmission index for this PHY attempt: 0..2 */
  uint16_t crc;        /**< CRC16 over bytes [0 .. offsetof(crc)) */
} GrainUplinkPacket_t;

#if defined(__GNUC__) || defined(__clang__)
_Static_assert(sizeof(GrainUplinkPacket_t) == GRAIN_UPLINK_WIRE_SIZE, "grain uplink wire size");
#endif

/** CRC-16/CCITT-FALSE over @p len bytes */
uint16_t GrainPacket_Crc16(const uint8_t *data, size_t len);

/** Write CRC field from all preceding struct bytes */
void GrainPacket_Finalize(GrainUplinkPacket_t *pkt);

/** @return 1 if CRC matches, 0 otherwise */
int GrainPacket_Verify(const GrainUplinkPacket_t *pkt);

#ifdef __cplusplus
}
#endif

#endif /* GRAIN_PACKET_H */
