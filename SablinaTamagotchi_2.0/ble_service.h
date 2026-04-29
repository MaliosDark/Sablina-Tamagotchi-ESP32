#pragma once
// ═══════════════════════════════════════════════════════════════════
//  BLE GATT Service for Sablina Tamagotchi 2.0
//  Expo app (React Native) connects to this service to:
//    • Read / stream pet state  (notify)
//    • Send commands            (write without response)
//    • Configure WiFi / LLM    (write)
//    • Read / write personality (read + write)
//    • Receive LLM responses    (notify)
//
//  Library: BLE built into ESP32 Arduino core (esp32-arduino)
//           Install via Board Manager: esp32 by Espressif ≥ 3.x
// ═══════════════════════════════════════════════════════════════════
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ctype.h>
// esp_ble_gap_config_adv_data_raw is available via BLEDevice.h → esp_gap_ble_api.h

// Forward declaration of globals the BLE callbacks need to access
// (defined in main .ino)
extern int  Hun_mas, Fat_mas, Cle_mas, Exp_mas;
extern int  Hun, Fat, Cle, Exp;
extern char g_wifiSSID[64];
extern char g_wifiPass[64];
extern bool g_wifiChanged;

// Filled by BLE writes, consumed by loop()
struct BLECommand {
  bool  pending  = false;
  char  cmd[32]  = "";   // "feed", "clean", "sleep", "play", "pet"
};
volatile BLECommand g_bleCmd;

struct BLEPeerState {
  bool          visible     = false;
  int           rssi        = -127;
  unsigned long lastSeenMs  = 0;
  uint16_t      senderId    = 0;
  uint8_t       lastMsgSeq  = 0;
  uint8_t       lastReplyTo = 0;
  bool          lastWasReply= false;
  char          name[24]    = "";
};

struct BLEPeerMessage {
  bool     valid    = false;
  bool     isReply  = false;
  uint16_t senderId = 0;
  uint8_t  seq      = 0;
  uint8_t  replyTo  = 0;
  char     text[BLE_PEER_MESSAGE_MAX_TEXT + 1] = "";
};

class SablinaBLE {
public:
  bool deviceConnected   = false;
  bool prevConnected     = false;

  // Call BLE characteristic pointers (set after server init)
  BLECharacteristic* pStateChar      = nullptr;
  BLECharacteristic* pLlmRespChar    = nullptr;
  BLECharacteristic* pPersonalityChar= nullptr;
  BLEPeerState       peer;

  // ── BLE server callbacks ────────────────────────────────────────
  class ServerCB : public BLEServerCallbacks {
  public:
    SablinaBLE* parent;
    ServerCB(SablinaBLE* p) : parent(p) {}
    void onConnect(BLEServer*)    override { parent->deviceConnected = true; }
    void onDisconnect(BLEServer*) override { parent->deviceConnected = false; }
  };

  // ── Config characteristic write callback ────────────────────────
  //  Expects JSON:
  //    {"wifi_ssid":"...","wifi_pass":"...","llm_key":"...","llm_endpoint":"...","llm_model":"...","offline_force":true}
  class ConfigCB : public BLECharacteristicCallbacks {
  public:
    SablinaBLE* parent;
    ConfigCB(SablinaBLE* p) : parent(p) {}
    void onWrite(BLECharacteristic* ch) override {
      String val = ch->getValue();
      if (!val.length()) return;
      JsonDocument doc;
      if (deserializeJson(doc, val.c_str())) return;
      // WiFi
      if (doc["wifi_ssid"].is<const char*>()) {
        strlcpy(g_wifiSSID, doc["wifi_ssid"], sizeof(g_wifiSSID));
        strlcpy(g_wifiPass, doc["wifi_pass"] | "", sizeof(g_wifiPass));
        g_wifiChanged = true;
        extern Preferences g_prefs;
        g_prefs.putString(NVS_WIFI_SSID, g_wifiSSID);
        g_prefs.putString(NVS_WIFI_PASS,  g_wifiPass);
      }
      // LLM config delegated to personality engine via extern
      extern class LLMPersonality g_llm;
      if (doc["llm_key"].is<const char*>())
        g_llm.setApiKey(doc["llm_key"]);
      if (doc["llm_endpoint"].is<const char*>())
        g_llm.setEndpoint(doc["llm_endpoint"]);
      if (doc["llm_model"].is<const char*>())
        g_llm.setModel(doc["llm_model"]);
      if (doc["offline_force"].is<bool>())
        g_llm.setForceOffline(doc["offline_force"]);
      // Platform Canvas credentials
      extern char g_platformUrl[];
      extern char g_platformKey[];
      extern Preferences g_prefs;
      if (doc["platform_url"].is<const char*>()) {
        strlcpy(g_platformUrl, doc["platform_url"], 128);
        g_prefs.putString(NVS_PLATFORM_URL, g_platformUrl);
      }
      if (doc["platform_key"].is<const char*>()) {
        strlcpy(g_platformKey, doc["platform_key"], 64);
        g_prefs.putString(NVS_PLATFORM_KEY, g_platformKey);
      }
    }
  };

  // ── Command characteristic write callback ───────────────────────
  //  Payload: plain string
  //   "feed" | "clean" | "sleep" | "play" | "pet" | "heal" | "wake"
  class CmdCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* ch) override {
      String val = ch->getValue();
      if (!val.length()) return;
      g_bleCmd.cmd[0] = '\0';
      strlcpy((char*)g_bleCmd.cmd, val.c_str(), sizeof(g_bleCmd.cmd));
      g_bleCmd.pending = true;
    }
  };

  // ── Personality characteristic write callback ───────────────────
  class PersonalityCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* ch) override {
      String val = ch->getValue();
      if (!val.length()) return;
      extern class LLMPersonality g_llm;
      g_llm.setTraitsFromJson(val.c_str());
      // Echo back updated traits
      char buf[256];
      g_llm.traitsToJson(buf, sizeof(buf));
      ch->setValue(buf);
    }
  };

#if FEATURE_BLE_PEERS
  class PeerScanCB : public BLEAdvertisedDeviceCallbacks {
  public:
    SablinaBLE* parent;
    PeerScanCB(SablinaBLE* p) : parent(p) {}
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
      parent->rememberPeer(advertisedDevice);
    }
  };
#endif

  // ── Init ────────────────────────────────────────────────────────
  void begin(const char* preferredName = nullptr) {
    buildAdvertisedName(preferredName);
    BLEDevice::init(_advertisedName);
    BLEServer*  pServer  = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCB(this));

    BLEService* pService = pServer->createService(BLE_SERVICE_UUID);

    // State notify characteristic (read + notify)
    pStateChar = pService->createCharacteristic(
      BLE_STATE_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pStateChar->addDescriptor(new BLE2902());

    // Config write
    BLECharacteristic* pConfigChar = pService->createCharacteristic(
      BLE_CONFIG_UUID, BLECharacteristic::PROPERTY_WRITE);
    pConfigChar->setCallbacks(new ConfigCB(this));

    // Command write
    BLECharacteristic* pCmdChar = pService->createCharacteristic(
      BLE_CMD_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
    pCmdChar->setCallbacks(new CmdCB());

    // Personality RW
    pPersonalityChar = pService->createCharacteristic(
      BLE_PERSONALITY_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);
    pPersonalityChar->addDescriptor(new BLE2902());
    pPersonalityChar->setCallbacks(new PersonalityCB());

    // LLM response notify
    pLlmRespChar = pService->createCharacteristic(
      BLE_LLM_RESPONSE_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pLlmRespChar->addDescriptor(new BLE2902());

    pService->start();
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    _advertising = pAdv;
    // Use standard (non-custom) advertising start — Bluedroid builds the packet
    // automatically including the device name and service UUID without any
    // async race conditions from the custom BLEAdvertisementData path.
    pAdv->addServiceUUID(BLE_SERVICE_UUID);
    BLEDevice::startAdvertising();

#if FEATURE_BLE_PEERS
    _scan = BLEDevice::getScan();
    if (_scan) {
      _scan->setAdvertisedDeviceCallbacks(new PeerScanCB(this), true);
      _scan->setActiveScan(true);   // active scan: sends SCAN_REQ, gets full name + mfr data
      _scan->setInterval(100);
      _scan->setWindow(90);         // 90% duty cycle — wide window for better coverage
    }
    // Stagger first scan so two powered-on devices don't scan simultaneously.
    _lastPeerScanMs = millis() + (_deviceId & 0xFF);
    Serial.printf("[BLE] init done — device %s  id=0x%04X\n", _advertisedName, _deviceId);
    // NOTE: do NOT call refreshAdvertising() here — BLEDevice::startAdvertising()
    // runs an async chain (adv data config → scan resp config → start). Calling
    // esp_ble_gap_config_adv_data_raw() before that chain completes races with it
    // and can leave advertising silently broken. The standard adv packet already
    // contains the device name (in scan response) which is enough for detection.
#endif
  }

  // ── Notify pet state (call every BLE_NOTIFY_INTERVAL_MS) ────────
  void notifyState(int hun, int fat, int cle, int exp,
                   const char* mood, bool wifiOk, bool llmOk, bool offlineForced) {
    if (!deviceConnected) return;
    char buf[256];
    snprintf(buf, sizeof(buf),
      "{\"hun\":%d,\"fat\":%d,\"cle\":%d,\"exp\":%d,"
      "\"mood\":\"%s\",\"wifi\":%s,\"llm\":%s,\"offlineForced\":%s,"
      "\"peerVisible\":%s,\"peerName\":\"%s\",\"peerRssi\":%d}",
      hun, fat, cle, exp, mood,
      wifiOk ? "true" : "false",
      llmOk  ? "true" : "false",
      offlineForced ? "true" : "false",
      peerVisible() ? "true" : "false",
      peerVisible() ? peer.name : "",
      peerVisible() ? peer.rssi : -127);
    pStateChar->setValue(buf);
    pStateChar->notify();
  }

  // ── Notify LLM response text ─────────────────────────────────────
  void notifyLlmResponse(const char* text) {
    if (!deviceConnected || !pLlmRespChar) return;
    pLlmRespChar->setValue(text);
    pLlmRespChar->notify();
  }

  // ── Re-advertise after disconnect ───────────────────────────────
  void tick() {
    const unsigned long now = millis();
    if (!deviceConnected && prevConnected) {
      delay(500);
      BLEDevice::startAdvertising();
    }

#if FEATURE_BLE_PEERS
    if (peer.visible && (now - peer.lastSeenMs) > BLE_PEER_TIMEOUT_MS) {
      peer.visible = false;
      peer.senderId = 0;
      peer.name[0] = '\0';
      peer.rssi = -127;
      peer.lastSeenMs = 0;
    }
    if (_peerOutgoing.valid && _peerOutgoingUntilMs && now > _peerOutgoingUntilMs) {
      _peerOutgoing = BLEPeerMessage{};
      _peerOutgoingUntilMs = 0;
      refreshAdvertising();  // reverts beacon back to presence-only
    }
    // Async scan (non-blocking) — onResult fires per-device via PeerScanCB
    if (_scan && (now - _lastPeerScanMs) > BLE_PEER_SCAN_INTERVAL_MS) {
      _lastPeerScanMs = now;
      _scan->clearResults();
      Serial.printf("[BLE] scan start (peer=%s)\n", peer.visible ? peer.name : "none");
      _scan->start(BLE_PEER_SCAN_DURATION_S, _onScanDone, false);
    }
#endif

    prevConnected = deviceConnected;
  }

  bool peerVisible() const {
    return peer.visible;
  }

  const char* peerName() const {
    return peer.visible ? peer.name : "";
  }

  int peerRssi() const {
    return peer.visible ? peer.rssi : -127;
  }

  const char* advertisedName() const {
    return _advertisedName;
  }

  uint16_t deviceId() const {
    return _deviceId;
  }

  uint8_t lastQueuedPeerSeq() const {
    return _peerOutgoing.valid ? _peerOutgoing.seq : 0;
  }

  bool queuePeerMessage(const char* text, bool isReply = false, uint8_t replyTo = 0) {
#if !FEATURE_BLE_PEERS
    (void)text;
    (void)isReply;
    (void)replyTo;
    return false;
#else
    char cleaned[BLE_PEER_MESSAGE_MAX_TEXT + 1];
    sanitizePeerText(text, cleaned, sizeof(cleaned));
    if (!cleaned[0]) return false;

    _peerOutgoing.valid = true;
    _peerOutgoing.isReply = isReply;
    _peerOutgoing.senderId = _deviceId;
    _peerOutgoing.seq = ++_peerSeq;
    _peerOutgoing.replyTo = replyTo;
    strlcpy(_peerOutgoing.text, cleaned, sizeof(_peerOutgoing.text));
    _peerOutgoingUntilMs = millis() + BLE_PEER_MESSAGE_TTL_MS;
    refreshAdvertising();
    return true;
#endif
  }

  bool takeIncomingPeerMessage(BLEPeerMessage* out) {
#if !FEATURE_BLE_PEERS
    (void)out;
    return false;
#else
    if (!out || !_peerIncomingPending) return false;
    *out = _peerIncoming;
    _peerIncomingPending = false;
    return true;
#endif
  }

private:
  BLEAdvertising* _advertising    = nullptr;
  BLEScan*        _scan           = nullptr;
  unsigned long   _lastPeerScanMs     = 0;
  unsigned long   _peerOutgoingUntilMs = 0;
  uint16_t        _deviceId       = 0;
  uint8_t         _peerSeq        = 0;
  char            _advertisedName[24] = BLE_DEVICE_NAME;
  BLEPeerMessage  _peerOutgoing;
  BLEPeerMessage  _peerIncoming;
  volatile bool   _peerIncomingPending = false;

  static constexpr uint8_t PEER_COMPANY_ID_LO = 0xFF;
  static constexpr uint8_t PEER_COMPANY_ID_HI = 0xFF;
  static constexpr uint8_t PEER_MAGIC_0       = 'S';
  static constexpr uint8_t PEER_MAGIC_1       = 'B';
  static constexpr uint8_t PEER_VERSION       = 0x01;
  static constexpr uint8_t PEER_KIND_SAY      = 0x01;
  static constexpr uint8_t PEER_KIND_REPLY    = 0x02;

  // No-op callback for async scan — results handled per-device via PeerScanCB::onResult
  static void _onScanDone(BLEScanResults) {}

  static int hexNibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    return -1;
  }

  static bool parseHexByte(const char* src, uint8_t* out) {
    if (!src || !out) return false;
    const int hi = hexNibble(src[0]);
    const int lo = hexNibble(src[1]);
    if (hi < 0 || lo < 0) return false;
    *out = static_cast<uint8_t>((hi << 4) | lo);
    return true;
  }

  static bool parseHexWord(const char* src, uint16_t* out) {
    uint8_t hi = 0;
    uint8_t lo = 0;
    if (!src || !out) return false;
    if (!parseHexByte(src, &hi) || !parseHexByte(src + 2, &lo)) return false;
    *out = static_cast<uint16_t>((hi << 8) | lo);
    return true;
  }

  // Minimal presence beacon in ASCII to avoid String/binary truncation issues.
  String buildPresenceBeacon() const {
    char buf[12];
    snprintf(buf, sizeof(buf), "SB%04X", _deviceId);
    return String(buf);
  }

  static bool startsWith(const char* text, const char* prefix) {
    if (!text || !prefix) return false;
    const size_t prefixLen = strlen(prefix);
    return strncmp(text, prefix, prefixLen) == 0;
  }

  void buildAdvertisedName(const char* preferredName) {
    (void)preferredName;
    const uint64_t mac = ESP.getEfuseMac();
    // bytes 0-1 are the OUI (identical across same-manufacturer devices);
    // bytes 4-5 (bits 32-47) are the device-unique NIC portion of the MAC.
    const unsigned int suffix = (unsigned int)((mac >> 32) & 0xFFFF);
    _deviceId = (uint16_t)suffix;
    snprintf(_advertisedName, sizeof(_advertisedName), "%s-%04X", BLE_DEVICE_NAME_PREFIX, suffix);
  }

  static void sanitizePeerText(const char* src, char* dst, size_t dstLen) {
    if (!dst || dstLen == 0) return;
    dst[0] = '\0';
    if (!src || !src[0]) return;

    size_t outIdx = 0;
    bool lastWasSpace = false;
    for (size_t i = 0; src[i] && outIdx + 1 < dstLen; ++i) {
      const unsigned char ch = static_cast<unsigned char>(src[i]);
      if (ch < 32 || ch > 126) continue;
      if (isspace(ch)) {
        if (outIdx == 0 || lastWasSpace) continue;
        dst[outIdx++] = ' ';
        lastWasSpace = true;
        continue;
      }
      dst[outIdx++] = static_cast<char>(ch);
      lastWasSpace = false;
    }
    while (outIdx > 0 && dst[outIdx - 1] == ' ') outIdx--;
    dst[outIdx] = '\0';
  }

  String buildPeerPayload() const {
    char fixedText[BLE_PEER_MESSAGE_MAX_TEXT + 1];
    sanitizePeerText(_peerOutgoing.text, fixedText, sizeof(fixedText));

    char payload[40];
    snprintf(payload, sizeof(payload), "SB%04X%02X%02X%c%s",
             _deviceId,
             _peerOutgoing.seq,
             _peerOutgoing.replyTo,
             _peerOutgoing.isReply ? 'R' : 'S',
             fixedText);
    return String(payload);
  }

  static bool decodePeerPayload(const String& raw, BLEPeerMessage* out) {
    if (!out) return false;
    *out = BLEPeerMessage{};
    const char* text = raw.c_str();
    if (raw.length() < 11) return false;
    if (text[0] != 'S' || text[1] != 'B') return false;
    if (!parseHexWord(text + 2, &out->senderId)) return false;
    if (!parseHexByte(text + 6, &out->seq)) return false;
    if (!parseHexByte(text + 8, &out->replyTo)) return false;
    if (text[10] != 'S' && text[10] != 'R') return false;

    out->isReply = (text[10] == 'R');
    sanitizePeerText(text + 11, out->text, sizeof(out->text));
    out->valid = out->text[0] != '\0';
    return out->valid;
  }

  // Update advertising payload in-place using the raw Bluedroid API.
  // This avoids the stop→setData→start race condition where ADV_STOP_COMPLETE_EVT
  // can restart advertising with stale data before the new data is configured.
  // esp_ble_gap_config_adv_data_raw() is safe to call while advertising is running;
  // the controller uses the new payload on the next advertising event.
  void refreshAdvertising() {
    const bool hasMsg = _peerOutgoing.valid && _peerOutgoing.text[0];
    const String payload = hasMsg ? buildPeerPayload() : buildPresenceBeacon();

    // Build raw AD bytes: Flags (3B) + Manufacturer Specific (2+N B)
    uint8_t buf[31];
    uint8_t i = 0;
    buf[i++] = 0x02; buf[i++] = 0x01; buf[i++] = 0x06;  // Flags: LE General Discoverable
    const uint8_t dlen = (uint8_t)payload.length();
    buf[i++] = (uint8_t)(dlen + 1);  // length field includes type byte
    buf[i++] = 0xFF;                 // AD type: Manufacturer Specific
    for (uint8_t j = 0; j < dlen && i < 31; j++) buf[i++] = (uint8_t)payload[j];
#ifdef CONFIG_BLUEDROID_ENABLED
    esp_ble_gap_config_adv_data_raw(buf, i);
#endif
  }

  void rememberIncomingMessage(const BLEPeerMessage& msg) {
    if (!msg.valid || msg.senderId == _deviceId) return;
    if (peer.senderId == msg.senderId &&
        peer.lastMsgSeq == msg.seq &&
        peer.lastReplyTo == msg.replyTo &&
        peer.lastWasReply == msg.isReply) {
      return;
    }

    peer.senderId = msg.senderId;
    peer.lastMsgSeq = msg.seq;
    peer.lastReplyTo = msg.replyTo;
    peer.lastWasReply = msg.isReply;
    _peerIncoming = msg;
    _peerIncomingPending = true;
  }

  void rememberPeer(BLEAdvertisedDevice& advertisedDevice) {
    // ── Manufacturer data path (message exchange) ──────────────────
    if (advertisedDevice.haveManufacturerData()) {
      const String& mfr = advertisedDevice.getManufacturerData();
      uint16_t sid = 0;
      if (mfr.length() >= 6 && mfr[0] == 'S' && mfr[1] == 'B' &&
          parseHexWord(mfr.c_str() + 2, &sid) && sid != _deviceId) {
        if (peer.senderId != sid) {
          peer.senderId = sid;
          peer.name[0] = '\0';
          snprintf(peer.name, sizeof(peer.name), "%s-%04X", BLE_DEVICE_NAME_PREFIX, sid);
        }
        peer.rssi = advertisedDevice.getRSSI();
        peer.lastSeenMs = millis();
        peer.visible = true;
        Serial.printf("[BLE] peer via mfr: %s rssi=%d\n", peer.name, peer.rssi);
        BLEPeerMessage incoming;
        if (decodePeerPayload(mfr, &incoming)) {
          rememberIncomingMessage(incoming);
        }
        return;
      }
    }

    // ── Name-based detection (primary presence via scan response) ──
    // Active scan sends SCAN_REQ; advertiser responds with SCAN_RSP
    // containing the device name set at BLEDevice::init() time.
    if (!advertisedDevice.haveName()) return;
    const String peerNameStr = advertisedDevice.getName();
    if (!peerNameStr.length()) return;
    const char* foundName = peerNameStr.c_str();
    if (strcmp(foundName, _advertisedName) == 0) return;   // skip self
    if (!startsWith(foundName, BLE_DEVICE_NAME_PREFIX)) return;  // must be a Sablina

    // Extract 4-hex device ID from suffix (e.g. "Sablina-4518" → 0x4518)
    const size_t prefixDashLen = strlen(BLE_DEVICE_NAME_PREFIX) + 1;  // "Sablina-"
    uint16_t extractedId = 0;
    if (strlen(foundName) >= prefixDashLen + 4) {
      parseHexWord(foundName + prefixDashLen, &extractedId);
    }
    if (extractedId != 0 && extractedId != _deviceId) {
      peer.senderId = extractedId;
    }
    strlcpy(peer.name, foundName, sizeof(peer.name));
    peer.rssi = advertisedDevice.getRSSI();
    peer.lastSeenMs = millis();
    peer.visible = true;
    Serial.printf("[BLE] peer via name: %s rssi=%d\n", peer.name, peer.rssi);
  }
};
