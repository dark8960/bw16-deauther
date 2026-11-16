// ============================================================
// Raw 802.11 frame TX for the RTL8720DN (AmebaD) driver.
// Adapted from Marcel Balser's RTL8720dn-Deauther
// (packet-injection), which itself relies on internal symbols
// of the AmebaD SDK mgmt-frame TX path.
//   rltk_wlan_info + 0x10  -> adapter ptr (pv_x)
//   alloc_mgtxmitframe(pv_x + 0xa80)  -> tx frame desc
//   frame payload lives at *(uint32_t*)(fc + 0x80) + 0x28
//   frame length goes into fc + 0x14 / fc + 0x18
// The FCS is appended by hardware; sequence number 0 is
// replaced by the driver automatically.
// ============================================================
#ifndef WIFI_TX_H
#define WIFI_TX_H

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>

// --- 802.11 management frames we inject -----------------------
typedef struct {
  uint16_t frame_control = 0xC0;      // type=mgmt, subtype=12 (deauth)
  uint16_t duration = 0xFFFF;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0; // driver fills real seq
  uint16_t reason = 0x02;
} DeauthFrame;

typedef struct {
  uint16_t frame_control = 0xA0;      // type=mgmt, subtype=10 (disassoc)
  uint16_t duration = 0xFFFF;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x07;
} DisassocFrame;

// Internal driver symbols (closed-source lib objects)
extern uint8_t* rltk_wlan_info;
extern "C" void* alloc_mgtxmitframe(void* ptr);
extern "C" void update_mgntframe_attrib(void* ptr, void* frame_control);
extern "C" int dump_mgntframe(void* ptr, void* frame_control);

// Return false when the driver has no free mgmt TX buffer (alloc failed):
// under a too-fast injection rate the pool runs dry and frames silently stop.
bool wifi_tx_raw_frame(void* frame, size_t length);
bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason = 0x02);
bool wifi_tx_disassoc_frame(void* src_mac, void* dst_mac, uint16_t reason = 0x07);

#endif
