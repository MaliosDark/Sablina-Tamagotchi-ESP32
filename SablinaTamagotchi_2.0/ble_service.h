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
      std::string val = ch->getValue();
      if (val.empty()) return;
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
    }
  };

  // ── Command characteristic write callback ───────────────────────
  //  Payload: plain string
  //   "feed" | "clean" | "sleep" | "play" | "pet" | "heal" | "wake"
  class CmdCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* ch) override {
      std::string val = ch->getValue();
      if (val.empty()) return;
      g_bleCmd.cmd[0] = '\0';
      strlcpy(g_bleCmd.cmd, val.c_str(), sizeof(g_bleCmd.cmd));
      g_bleCmd.pending = true;
    }
  };

  // ── Personality characteristic write callback ───────────────────
  class PersonalityCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* ch) override {
      std::string val = ch->getValue();
      if (val.empty()) return;
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
    refreshAdvertising();

#if FEATURE_BLE_PEERS
    _scan = BLEDevice::getScan();
    if (_scan) {
      _scan->setAdvertisedDeviceCallbacks(new PeerScanCB(this), true);
      _scan->setActiveScan(true);
      _scan->setInterval(160);
      _scan->setWindow(80);
    }
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
      refreshAdvertising();
    }
    if (_scan && (now - _lastPeerScanMs) > BLE_PEER_SCAN_INTERVAL_MS) {
      _lastPeerScanMs = now;
      _scan->start(BLE_PEER_SCAN_DURATION_S, false);
      _scan->clearResults();
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
  unsigned long   _lastPeerScanMs = 0;
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

  static bool startsWith(const char* text, const char* prefix) {
    if (!text || !prefix) return false;
    const size_t prefixLen = strlen(prefix);
    return strncmp(text, prefix, prefixLen) == 0;
  }

  void buildAdvertisedName(const char* preferredName) {
    (void)preferredName;
    const unsigned int suffix = (unsigned int)(ESP.getEfuseMac() & 0xFFFF);
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

  std::string buildPeerPayload() const {
    char fixedText[BLE_PEER_MESSAGE_MAX_TEXT + 1];
    sanitizePeerText(_peerOutgoing.text, fixedText, sizeof(fixedText));

    std::string payload;
    payload.reserve(10 + BLE_PEER_MESSAGE_MAX_TEXT);
    payload.push_back(static_cast<char>(PEER_COMPANY_ID_LO));
    payload.push_back(static_cast<char>(PEER_COMPANY_ID_HI));
    payload.push_back(static_cast<char>(PEER_MAGIC_0));
    payload.push_back(static_cast<char>(PEER_MAGIC_1));
    payload.push_back(static_cast<char>(PEER_VERSION));
    payload.push_back(static_cast<char>(_deviceId & 0xFF));
    payload.push_back(static_cast<char>((_deviceId >> 8) & 0xFF));
    payload.push_back(static_cast<char>(_peerOutgoing.seq));
    payload.push_back(static_cast<char>(_peerOutgoing.replyTo));
    payload.push_back(static_cast<char>(_peerOutgoing.isReply ? PEER_KIND_REPLY : PEER_KIND_SAY));
    for (size_t i = 0; i < BLE_PEER_MESSAGE_MAX_TEXT; ++i) {
      payload.push_back(fixedText[i] ? fixedText[i] : '\0');
    }
    return payload;
  }

  static bool decodePeerPayload(const std::string& raw, BLEPeerMessage* out) {
    if (!out) return false;
    *out = BLEPeerMessage{};
    if (raw.size() < (10 + BLE_PEER_MESSAGE_MAX_TEXT)) return false;

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(raw.data());
    if (bytes[0] != PEER_COMPANY_ID_LO || bytes[1] != PEER_COMPANY_ID_HI) return false;
    if (bytes[2] != PEER_MAGIC_0 || bytes[3] != PEER_MAGIC_1) return false;
    if (bytes[4] != PEER_VERSION) return false;

    out->senderId = static_cast<uint16_t>(bytes[5] | (bytes[6] << 8));
    out->seq = bytes[7];
    out->replyTo = bytes[8];
    out->isReply = (bytes[9] == PEER_KIND_REPLY);

    size_t textIdx = 0;
    for (size_t i = 10; i < raw.size() && textIdx < BLE_PEER_MESSAGE_MAX_TEXT; ++i) {
      const char ch = raw[i];
      if (ch == '\0') break;
      if (static_cast<unsigned char>(ch) < 32 || static_cast<unsigned char>(ch) > 126) continue;
      out->text[textIdx++] = ch;
    }
    out->text[textIdx] = '\0';
    out->valid = textIdx > 0;
    return out->valid;
  }

  void refreshAdvertising() {
    if (!_advertising) return;

    BLEAdvertisementData advData;
    advData.setFlags(0x06);
    if (_peerOutgoing.valid && _peerOutgoing.text[0]) {
      advData.setManufacturerData(buildPeerPayload());
    }

    BLEAdvertisementData scanData;
    scanData.setName(_advertisedName);
    scanData.setCompleteServices(BLEUUID(BLE_SERVICE_UUID));

    _advertising->stop();
    _advertising->setAdvertisementData(advData);
    _advertising->setScanResponseData(scanData);
    _advertising->start();
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

  void rememberPeer(const BLEAdvertisedDevice& advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      BLEPeerMessage incoming;
      if (decodePeerPayload(advertisedDevice.getManufacturerData(), &incoming)) {
        rememberIncomingMessage(incoming);
      }
    }

    if (!advertisedDevice.haveName()) return;

    std::string peerNameValue = advertisedDevice.getName();
    if (peerNameValue.empty()) return;

    char foundName[sizeof(peer.name)];
    strlcpy(foundName, peerNameValue.c_str(), sizeof(foundName));
    if (strcmp(foundName, _advertisedName) == 0) return;
    if (!startsWith(foundName, BLE_DEVICE_NAME_PREFIX)) return;

    strlcpy(peer.name, foundName, sizeof(peer.name));
    peer.rssi = advertisedDevice.getRSSI();
    peer.lastSeenMs = millis();
    peer.visible = true;
  }
};
