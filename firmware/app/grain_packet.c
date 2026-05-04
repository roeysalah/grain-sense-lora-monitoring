/**
 * @file grain_packet.c
 * @brief CRC and finalize helpers for `GrainUplinkPacket_t`.
 */
#include "grain_packet.h"

#include <stddef.h>
#include <string.h>

uint16_t GrainPacket_Crc16(const uint8_t *data, size_t len)
{
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8u; ++b) {
      if (crc & 0x8000u) {
        crc = (uint16_t)((crc << 1) ^ 0x1021u);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void GrainPacket_Finalize(GrainUplinkPacket_t *pkt)
{
  if (pkt == NULL) {
    return;
  }
  /* CRC covers everything before the CRC field itself */
  pkt->crc = 0u;
  pkt->crc = GrainPacket_Crc16((const uint8_t *)pkt, sizeof(*pkt) - sizeof(pkt->crc));
}

int GrainPacket_Verify(const GrainUplinkPacket_t *pkt)
{
  uint16_t on_wire;
  uint16_t calc;

  if (pkt == NULL) {
    return 0;
  }

  on_wire = pkt->crc;
  /* Recompute with crc field cleared the same way as finalize */
  {
    GrainUplinkPacket_t tmp;
    (void)memcpy(&tmp, pkt, sizeof(tmp));
    tmp.crc = 0u;
    calc = GrainPacket_Crc16((const uint8_t *)&tmp, sizeof(tmp) - sizeof(tmp.crc));
  }
  return (on_wire == calc) ? 1 : 0;
}
