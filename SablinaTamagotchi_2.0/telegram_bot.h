#pragma once
// ═══════════════════════════════════════════════════════════════════
//  Tiny-Sable Telegram Bot, Interactive Edition
//  ─────────────────────────────────────────────────────────────────
//  Features
//    • Chat with Sablina via LLM (any OpenAI-compatible API)
//    • Interactive inline-keyboard menus (no slash commands needed)
//    • Pet menu: Feed / Clean / Sleep / Play / Pet / Heal / Wake
//    • WiFi Audit menu: Scan / Monitor / Deauth / Handshake / PMKID / Stop
//    • AP list from scan results, pick target by number
//    • LLM WiFi advisor: asks AI for attack strategy (rate-limited)
//    • Proactive push alerts when pet vitals are critical
//    • chat_id lock: only the registered owner can control the device
//    • Persists update_id offset in NVS (no replay on reboot)
//  ─────────────────────────────────────────────────────────────────
//  Security
//    • api.telegram.org calls use TLS encryption (setInsecure, ESP32
//      does not have a system root store; DigiCert cert not pre-bundled)
//    • User LLM endpoints use setInsecure(), self-signed certs are
//      common on home servers; set TG_LLM_VERIFY_TLS 1 to enable
//    • LLM endpoint validated: must be https:// or http://localhost
//    • Incoming messages capped at TG_MAX_MSG_LEN to prevent DoS
//    • chat_id is locked after first message; ignored IDs are dropped
//  ─────────────────────────────────────────────────────────────────
//  tick() returns a TgCommand for the main loop to execute.
//  Wrap all call sites with #if FEATURE_TELEGRAM.
//  Dependencies: ArduinoJson, WiFiClientSecure (built-in ESP32)
// ═══════════════════════════════════════════════════════════════════
#include "config.h"
#include "llm_personality.h"   // for PersonalityTraits only
#if FEATURE_WIFI_AUDIT
#include "wifi_audit.h"
#endif
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ── Compile-time defaults (override in secrets.h) ─────────────────
#if __has_include("secrets.h")
  #include "secrets.h"
#endif
#ifndef TG_BOT_TOKEN_DEFAULT
  #define TG_BOT_TOKEN_DEFAULT   ""
#endif
#ifndef TG_OPENWEBUI_KEY_DEFAULT
  #define TG_OPENWEBUI_KEY_DEFAULT  ""
#endif
#ifndef TG_OPENWEBUI_ENDPOINT_DEFAULT
  #define TG_OPENWEBUI_ENDPOINT_DEFAULT  ""
#endif
#ifndef TG_OPENWEBUI_MODEL_DEFAULT
  #define TG_OPENWEBUI_MODEL_DEFAULT  ""
#endif
// Set to 1 to verify TLS cert on the user LLM endpoint.
// Keep 0 if your server uses a self-signed cert or plain HTTP on LAN.
#ifndef TG_LLM_VERIFY_TLS
  #define TG_LLM_VERIFY_TLS 0
#endif

// ── Timing & limits ───────────────────────────────────────────────
#define TG_POLL_INTERVAL_MS    3000UL    // poll Telegram every 3 s
#define TG_HTTP_TIMEOUT_MS    10000UL    // socket timeout
#define TG_MAX_RESPONSE_LEN     512      // max chars sent to Telegram
#define TG_MAX_MSG_LEN          512      // max incoming message length (DoS guard)
#define TG_ALERT_COOLDOWN_MS  300000UL   // 5 min between push alerts
#define TG_ADVISOR_COOLDOWN_MS 120000UL  // 2 min between LLM advisor calls
#define TG_ALERT_THRESHOLD      20       // stat level that triggers alert

// ── Commands returned by tick() to the main loop ──────────────────
// Pet commands mirror handleBleCmdIfAny() in the .ino exactly.
// WiFi commands mirror the audit BLE commands.
enum TgCommand : uint8_t {
  TG_CMD_NONE = 0,
  // ── Pet ──────────────────────────────────────────────────────────
  TG_CMD_FEED, TG_CMD_CLEAN, TG_CMD_SLEEP,
  TG_CMD_PLAY, TG_CMD_PET,   TG_CMD_HEAL, TG_CMD_WAKE,
  // ── WiFi ─────────────────────────────────────────────────────────
  TG_CMD_WIFI_SCAN, TG_CMD_WIFI_MONITOR,
  TG_CMD_WIFI_DEAUTH, TG_CMD_WIFI_HANDSHAKE,
  TG_CMD_WIFI_PMKID,  TG_CMD_WIFI_STOP,
  // ── AP selection (payload: _pendingApIndex) ───────────────────────
  TG_CMD_WIFI_SELECT_AP,
};

// ── Bot context menu state ────────────────────────────────────────
// Tracks which top-level menu the user last opened.
enum TgMenuCtx : uint8_t {
  CTX_NONE = 0,
  CTX_PET,
  CTX_WIFI,
  CTX_WIFI_AP_LIST,   // waiting for user to pick an AP number
};

class TelegramBot {
public:
  bool    enabled      = false;
  uint8_t pendingApIdx = 0;    // AP index chosen via /ap_N callback

#if FEATURE_WIFI_AUDIT
  // The .ino passes a pointer after g_wifiAudit.begin()
  WifiAudit* wifiAudit = nullptr;
#endif

  // ── Call once in setup() ──────────────────────────────────────────
  void begin(Preferences* prefs, const PersonalityTraits* traits) {
    _prefs  = prefs;
    _traits = traits;
    _loadConfig();
  }

  // ── Call every loop(),returns a TgCommand for the main loop ─────
  TgCommand tick(unsigned long nowMs, int hun, int fat, int cle, int exp,
                 bool petSick = false, bool petAlive = true) {
    if (!enabled) return TG_CMD_NONE;
    if (WiFi.status() != WL_CONNECTED) return TG_CMD_NONE;

    _checkAlerts(nowMs, hun, fat, cle);
    _checkLifecycleAlerts(nowMs, petSick, petAlive);

    if (nowMs - _lastPollMs < TG_POLL_INTERVAL_MS) return TG_CMD_NONE;
    _lastPollMs = nowMs;

    char    userMsg[256]    = {0};
    int64_t callbackMsgId   = 0;
    char    callbackData[64]= {0};
    if (!_pollTelegram(userMsg, sizeof(userMsg), callbackData, sizeof(callbackData), &callbackMsgId))
      return TG_CMD_NONE;

    // ── Inline keyboard callback query (button press) ────────────────
    if (callbackData[0]) {
      return _handleCallback(callbackData, callbackMsgId, nowMs, hun, fat, cle, exp);
    }

    if (!userMsg[0]) return TG_CMD_NONE;

    // ── /start or /menu → show main menu ─────────────────────────────
    if (strcmp(userMsg, "/start") == 0 || strcmp(userMsg, "/menu") == 0) {
      _sendMainMenu(); return TG_CMD_NONE;
    }
    // ── /help ─────────────────────────────────────────────────────────
    if (strcmp(userMsg, "/help") == 0) {
      _sendHelp(); return TG_CMD_NONE;
    }
    // ── /stats ────────────────────────────────────────────────────────
    if (strcmp(userMsg, "/stats") == 0) {
      _sendStats(hun, fat, cle, exp); return TG_CMD_NONE;
    }
    // ── /wifi ─────────────────────────────────────────────────────────
    if (strcmp(userMsg, "/wifi") == 0) {
      _sendWifiMenu(nowMs); return TG_CMD_NONE;
    }
    // ── /pet (or /care) ───────────────────────────────────────────────
    if (strcmp(userMsg, "/pet") == 0 || strcmp(userMsg, "/care") == 0) {
      _sendPetMenu(); return TG_CMD_NONE;
    }

    // ── Free-text chat → LLM reply ───────────────────────────────────
    _menuCtx = CTX_NONE;
    char response[TG_MAX_RESPONSE_LEN];
    _callLLM(userMsg, hun, fat, cle, exp, response, sizeof(response));
    _sendMessage(response[0] ? response : "...");
    return TG_CMD_NONE;
  }

  // ── Proactive send (called from .ino after applying a command) ────
  void sendMessage(const char* text) { _sendMessage(text); }

  // ── Runtime config setters ────────────────────────────────────────
  void setToken(const char* token) {
    strlcpy(_token, token, sizeof(_token));
    if (_prefs) _prefs->putString(NVS_TG_TOKEN, token);
    _refreshEnabled();
  }
  void setAllowedChatId(int64_t id) {
    _chatId = id;
    if (_prefs) _prefs->putLong64(NVS_TG_CHAT_ID, id);
  }
  void setApiKey(const char* key) {
    strlcpy(_owuKey, key, sizeof(_owuKey));
    if (_prefs) _prefs->putString(NVS_TG_OWU_KEY, key);
  }
  void setModel(const char* model) {
    strlcpy(_model, model, sizeof(_model));
    if (_prefs) _prefs->putString(NVS_TG_OWU_MODEL, model);
  }
  void setEndpoint(const char* ep) {
    if (!_isEndpointAllowed(ep)) {
      // Silently reject disallowed endpoints (plain HTTP to non-localhost)
      return;
    }
    strlcpy(_owuEndpoint, ep, sizeof(_owuEndpoint));
    if (_prefs) _prefs->putString(NVS_TG_OWU_EP, ep);
  }

private:
  Preferences*             _prefs       = nullptr;
  const PersonalityTraits* _traits      = nullptr;

  char    _token[128]       = TG_BOT_TOKEN_DEFAULT;
  int64_t _chatId           = 0;
  char    _owuKey[128]      = TG_OPENWEBUI_KEY_DEFAULT;
  char    _owuEndpoint[128] = TG_OPENWEBUI_ENDPOINT_DEFAULT;
  char    _model[96]        = TG_OPENWEBUI_MODEL_DEFAULT;

  unsigned long _lastPollMs      = 0;
  unsigned long _lastAlertMs     = 0;
  unsigned long _lastSickAlertMs = 0;   // cooldown for sick notifications
  unsigned long _lastAdvisorMs   = 0;
  int64_t       _updateOffset    = 0;
  TgMenuCtx     _menuCtx         = CTX_NONE;

  // ── NVS load ──────────────────────────────────────────────────────
  void _loadConfig() {
    if (!_prefs) { _refreshEnabled(); return; }
    String tok = _prefs->getString(NVS_TG_TOKEN,    ""); if (tok.length()) strlcpy(_token,       tok.c_str(), sizeof(_token));
    String key = _prefs->getString(NVS_TG_OWU_KEY,  ""); if (key.length()) strlcpy(_owuKey,      key.c_str(), sizeof(_owuKey));
    String ep  = _prefs->getString(NVS_TG_OWU_EP,   "");
    // SSRF guard: validate stored endpoint before trusting it
    if (ep.length() && _isEndpointAllowed(ep.c_str())) strlcpy(_owuEndpoint, ep.c_str(), sizeof(_owuEndpoint));
    String mdl = _prefs->getString(NVS_TG_OWU_MODEL,""); if (mdl.length()) strlcpy(_model,       mdl.c_str(), sizeof(_model));
    _chatId       = _prefs->getLong64(NVS_TG_CHAT_ID, 0);
    _updateOffset = _prefs->getLong64(NVS_TG_OFFSET,  0);
    _refreshEnabled();
  }

  void _refreshEnabled() {
    enabled = strlen(_token) > 12 && strchr(_token, ':') != nullptr;
  }

  // ── Push alerts ───────────────────────────────────────────────────
  void _checkAlerts(unsigned long nowMs, int hun, int fat, int cle) {
    if (_chatId == 0) return;
    if (nowMs - _lastAlertMs < TG_ALERT_COOLDOWN_MS) return;
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    char msg[200] = {0};
    if      (hun <= TG_ALERT_THRESHOLD && hun <= fat && hun <= cle)
      snprintf(msg, sizeof(msg), "\xF0\x9F\x8D\x9C %s is STARVING! (hunger %d%%)", name, hun);
    else if (fat <= TG_ALERT_THRESHOLD && fat <= cle)
      snprintf(msg, sizeof(msg), "\xF0\x9F\x98\xB4 %s is EXHAUSTED! (fatigue %d%%)", name, fat);
    else if (cle <= TG_ALERT_THRESHOLD)
      snprintf(msg, sizeof(msg), "\xF0\x9F\x9B\x81 %s is FILTHY! (cleanliness %d%%)", name, cle);
    if (!msg[0]) return;
    _lastAlertMs = nowMs;
    // Append pet menu inline keyboard to the alert
    _sendWithKeyboard(msg, _buildPetKeyboard());
  }

  // ── Lifecycle alerts (sickness, death warning) ────────────────────
  void _checkLifecycleAlerts(unsigned long nowMs, bool sick, bool alive) {
    if (_chatId == 0) return;
    if (!alive) return;  // death msg sent directly from updateLifecycle
    if (nowMs - _lastSickAlertMs < TG_ALERT_COOLDOWN_MS * 2) return;
    if (!sick) return;
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    char msg[200];
    snprintf(msg, sizeof(msg),
      "\xF0\x9F\xA4\x92 %s is SICK and needs medicine!\n"
      "Use /heal or buy Medicine from the shop.", name);
    _lastSickAlertMs = nowMs;
    _sendWithKeyboard(msg, _buildPetKeyboard());
  }

  // ════════════════════════════════════════════════════════════════
  //  MENU BUILDERS  (return serialized InlineKeyboardMarkup JSON)
  // ════════════════════════════════════════════════════════════════

  String _buildPetKeyboard() {
    // Row 1: Feed Clean Sleep   Row 2: Play Pet Heal   Row 3: Wake Stats
    return "{"
      "\"inline_keyboard\":["
        "[{\"text\":\"\xF0\x9F\x8D\x9C Feed\",\"callback_data\":\"p_feed\"},"
         "{\"text\":\"\xF0\x9F\x9B\x81 Clean\",\"callback_data\":\"p_clean\"},"
         "{\"text\":\"\xF0\x9F\x8C\x99 Sleep\",\"callback_data\":\"p_sleep\"}],"
        "[{\"text\":\"\xF0\x9F\x8E\xAE Play\",\"callback_data\":\"p_play\"},"
         "{\"text\":\"\xF0\x9F\x92\x9C Pet\",\"callback_data\":\"p_pet\"},"
         "{\"text\":\"\xE2\x9D\xA4 Heal\",\"callback_data\":\"p_heal\"}],"
        "[{\"text\":\"\xE2\x98\x80 Wake\",\"callback_data\":\"p_wake\"},"
         "{\"text\":\"\xF0\x9F\x93\x8A Stats\",\"callback_data\":\"p_stats\"},"
         "{\"text\":\"\xF0\x9F\x93\xA1 WiFi\",\"callback_data\":\"menu_wifi\"}]"
      "]}";
  }

  String _buildWifiKeyboard() {
    return "{"
      "\"inline_keyboard\":["
        "[{\"text\":\"\xF0\x9F\x94\x8D Scan APs\",\"callback_data\":\"w_scan\"},"
         "{\"text\":\"\xF0\x9F\x93\xA1 Monitor\",\"callback_data\":\"w_monitor\"}],"
        "[{\"text\":\"\xF0\x9F\x94\xA5 Deauth\",\"callback_data\":\"w_deauth\"},"
         "{\"text\":\"\xF0\x9F\xA4\x9D Handshake\",\"callback_data\":\"w_handshake\"}],"
        "[{\"text\":\"\xF0\x9F\x94\x91 PMKID\",\"callback_data\":\"w_pmkid\"},"
         "{\"text\":\"\xE2\x9D\x8C Stop\",\"callback_data\":\"w_stop\"}],"
        "[{\"text\":\"\xF0\x9F\xA4\x96 AI Advisor\",\"callback_data\":\"w_advisor\"},"
         "{\"text\":\"\xF0\x9F\x90\xBE Pet Menu\",\"callback_data\":\"menu_pet\"}]"
      "]}";
  }

  String _buildApListKeyboard() {
#if FEATURE_WIFI_AUDIT
    if (!wifiAudit || wifiAudit->apCount == 0) return "{}";
    // Up to 8 APs as inline buttons, 2 per row
    String kb = "{\"inline_keyboard\":[";
    uint8_t count = wifiAudit->apCount < 8 ? wifiAudit->apCount : 8;
    for (uint8_t i = 0; i < count; i += 2) {
      kb += "[";
      for (uint8_t j = i; j < i + 2 && j < count; j++) {
        char label[40];
        snprintf(label, sizeof(label), "%d: %s (%ddBm)",
          j+1, wifiAudit->aps[j].ssid[0] ? wifiAudit->aps[j].ssid : "Hidden",
          wifiAudit->aps[j].rssi);
        char cbdata[16]; snprintf(cbdata, sizeof(cbdata), "ap_%d", j);
        if (j > i) kb += ",";
        // JSON-escape label (strip quotes just in case)
        String lbl = String(label);
        lbl.replace("\"", "'");
        kb += "{\"text\":\"" + lbl + "\",\"callback_data\":\"" + cbdata + "\"}";
      }
      kb += "]";
      if (i + 2 < count) kb += ",";
    }
    kb += ",[{\"text\":\"Back\",\"callback_data\":\"menu_wifi\"}]";
    kb += "]}";
    return kb;
#else
    return "{}";
#endif
  }

  // ════════════════════════════════════════════════════════════════
  //  MENU SENDERS
  // ════════════════════════════════════════════════════════════════

  void _sendMainMenu() {
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    char text[200];
    snprintf(text, sizeof(text),
      "\xF0\x9F\xA5\x9A %s Control Panel\n"
      "Choose a category:", name);
    String kb = "{"
      "\"inline_keyboard\":["
        "[{\"text\":\"\xF0\x9F\x90\xBE Care for " + String(name) + "\",\"callback_data\":\"menu_pet\"}],"
        "[{\"text\":\"\xF0\x9F\x93\xA1 WiFi Security Audit\",\"callback_data\":\"menu_wifi\"}],"
        "[{\"text\":\"\xF0\x9F\x93\x8A Stats\",\"callback_data\":\"p_stats\"}]"
      "]}";
    _sendWithKeyboard(text, kb);
  }

  void _sendPetMenu() {
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    char text[120];
    snprintf(text, sizeof(text), "\xF0\x9F\x90\xBE What do you want to do with %s?", name);
    _menuCtx = CTX_PET;
    _sendWithKeyboard(text, _buildPetKeyboard());
  }

  void _sendWifiMenu(unsigned long nowMs) {
    (void)nowMs;
#if FEATURE_WIFI_AUDIT
    const char* modeName = "IDLE";
    if (wifiAudit) {
      const char* modeNames[] = {"IDLE","SCANNING","MONITOR","DEAUTH","HANDSHAKE","PMKID"};
      uint8_t m = (uint8_t)wifiAudit->mode;
      if (m < 6) modeName = modeNames[m];
    }
    char text[160];
    snprintf(text, sizeof(text),
      "\xF0\x9F\x93\xA1 WiFi Security Audit\n"
      "Mode: %s  |  APs found: %d\n"
      "Pick an action:",
      modeName, wifiAudit ? wifiAudit->apCount : 0);
    _menuCtx = CTX_WIFI;
    _sendWithKeyboard(text, _buildWifiKeyboard());
#else
    _sendMessage("WiFi Audit is not compiled in (FEATURE_WIFI_AUDIT=0).");
#endif
  }

  void _sendApList() {
#if FEATURE_WIFI_AUDIT
    if (!wifiAudit || wifiAudit->apCount == 0) {
      _sendMessage("No APs found yet. Run Scan first.");
      return;
    }
    char header[80];
    snprintf(header, sizeof(header),
      "Found %d APs. Pick a target:", wifiAudit->apCount);
    _menuCtx = CTX_WIFI_AP_LIST;
    _sendWithKeyboard(header, _buildApListKeyboard());
#endif
  }

  void _sendStats(int hun, int fat, int cle, int exp) {
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    const char* mood = "okay";
    if      (hun < 25) mood = "starving";
    else if (fat < 25) mood = "exhausted";
    else if (cle < 25) mood = "dirty";
    else if (hun > 75 && fat > 75 && cle > 75) mood = "happy";
    char bars[4][14];
    _makeBar(bars[0], hun); _makeBar(bars[1], fat);
    _makeBar(bars[2], cle); _makeBar(bars[3], exp % 100);
    char buf[320];
    snprintf(buf, sizeof(buf),
      "\xF0\x9F\x93\x8A %s's Vitals\n"
      "Hunger      %s %d%%\n"
      "Fatigue     %s %d%%\n"
      "Cleanliness %s %d%%\n"
      "Coins       %s %d\n"
      "Mood: %s",
      name, bars[0], hun, bars[1], fat, bars[2], cle, bars[3], exp, mood);
    _sendWithKeyboard(buf, _buildPetKeyboard());
  }

  void _sendHelp() {
    _sendMessage(
      "/menu  ,open control panel\n"
      "/pet   ,care menu\n"
      "/wifi  ,WiFi audit menu\n"
      "/stats ,show vitals\n"
      "/help  ,this message\n\n"
      "Or just send any text to chat!"
    );
  }

  // ════════════════════════════════════════════════════════════════
  //  CALLBACK QUERY HANDLER  (inline keyboard button presses)
  // ════════════════════════════════════════════════════════════════

  TgCommand _handleCallback(const char* data, int64_t callbackMsgId,
                            unsigned long nowMs,
                            int hun, int fat, int cle, int exp) {
    _answerCallback(callbackMsgId);   // acknowledge the tap immediately

    // ── Menu navigation ──────────────────────────────────────────────
    if (strcmp(data, "menu_pet")  == 0) { _sendPetMenu();         return TG_CMD_NONE; }
    if (strcmp(data, "menu_wifi") == 0) { _sendWifiMenu(nowMs);   return TG_CMD_NONE; }

    // ── Pet actions ──────────────────────────────────────────────────
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    if (strcmp(data, "p_feed")  == 0) { char b[80]; snprintf(b,sizeof(b),"Feeding %s... nom nom!",name);  _sendMessage(b); return TG_CMD_FEED; }
    if (strcmp(data, "p_clean") == 0) { char b[80]; snprintf(b,sizeof(b),"Bath time for %s!",name);        _sendMessage(b); return TG_CMD_CLEAN; }
    if (strcmp(data, "p_sleep") == 0) { char b[80]; snprintf(b,sizeof(b),"Putting %s to sleep... zzz",name);_sendMessage(b);return TG_CMD_SLEEP; }
    if (strcmp(data, "p_play")  == 0) { char b[80]; snprintf(b,sizeof(b),"Playtime with %s!",name);        _sendMessage(b); return TG_CMD_PLAY; }
    if (strcmp(data, "p_pet")   == 0) { char b[80]; snprintf(b,sizeof(b),"Giving %s some love!",name);     _sendMessage(b); return TG_CMD_PET; }
    if (strcmp(data, "p_heal")  == 0) { char b[80]; snprintf(b,sizeof(b),"Healing %s...",name);            _sendMessage(b); return TG_CMD_HEAL; }
    if (strcmp(data, "p_wake")  == 0) { char b[80]; snprintf(b,sizeof(b),"Waking up %s!",name);            _sendMessage(b); return TG_CMD_WAKE; }
    if (strcmp(data, "p_stats") == 0) { _sendStats(hun, fat, cle, exp); return TG_CMD_NONE; }

    // ── WiFi actions ─────────────────────────────────────────────────
    if (strcmp(data, "w_scan") == 0) {
      _sendMessage("\xF0\x9F\x94\x8D Scanning for APs..."); return TG_CMD_WIFI_SCAN;
    }
    if (strcmp(data, "w_monitor") == 0) {
      _sendMessage("\xF0\x9F\x93\xA1 Passive monitor started."); return TG_CMD_WIFI_MONITOR;
    }
    if (strcmp(data, "w_deauth") == 0) {
      _sendApList(); return TG_CMD_NONE;  // user picks AP first
    }
    if (strcmp(data, "w_handshake") == 0) {
      _sendApList(); _menuCtx = CTX_WIFI_AP_LIST; return TG_CMD_NONE;
    }
    if (strcmp(data, "w_pmkid") == 0) {
      _sendMessage("\xF0\x9F\x94\x91 PMKID capture started."); return TG_CMD_WIFI_PMKID;
    }
    if (strcmp(data, "w_stop") == 0) {
      _sendMessage("\xE2\x9D\x8C WiFi audit stopped."); return TG_CMD_WIFI_STOP;
    }
    if (strcmp(data, "w_advisor") == 0) {
      _callWifiAdvisor(nowMs); return TG_CMD_NONE;
    }

    // ── AP selection (ap_0 … ap_7) ────────────────────────────────────
    if (strncmp(data, "ap_", 3) == 0) {
      uint8_t idx = (uint8_t)atoi(data + 3);
      pendingApIdx = idx;
#if FEATURE_WIFI_AUDIT
      if (wifiAudit && idx < wifiAudit->apCount) {
        char b[120];
        snprintf(b, sizeof(b),
          "Target: %s (%02X:%02X:%02X:%02X:%02X:%02X)\nStarting attack...",
          wifiAudit->aps[idx].ssid[0] ? wifiAudit->aps[idx].ssid : "Hidden",
          wifiAudit->aps[idx].bssid[0], wifiAudit->aps[idx].bssid[1],
          wifiAudit->aps[idx].bssid[2], wifiAudit->aps[idx].bssid[3],
          wifiAudit->aps[idx].bssid[4], wifiAudit->aps[idx].bssid[5]);
        _sendMessage(b);
      }
#endif
      _menuCtx = CTX_NONE;
      return TG_CMD_WIFI_SELECT_AP;
    }

    return TG_CMD_NONE;
  }

  // ── WiFi LLM Advisor (rate-limited to TG_ADVISOR_COOLDOWN_MS) ────
  void _callWifiAdvisor(unsigned long nowMs) {
    if (nowMs - _lastAdvisorMs < TG_ADVISOR_COOLDOWN_MS) {
      _sendMessage("AI advisor on cooldown, please wait.");
      return;
    }
    _lastAdvisorMs = nowMs;

#if FEATURE_WIFI_AUDIT
    // Build context from scan results
    char context[512] = {0};
    if (wifiAudit && wifiAudit->apCount > 0) {
      int pos = 0;
      pos += snprintf(context + pos, sizeof(context) - pos,
        "WiFi scan results (%d APs found):\n", wifiAudit->apCount);
      uint8_t n = wifiAudit->apCount < 5 ? wifiAudit->apCount : 5;
      for (uint8_t i = 0; i < n && pos < (int)sizeof(context) - 60; i++) {
        const char* enc[] = {"OPEN","WEP","WPA","WPA2","WPA3"};
        uint8_t e = wifiAudit->aps[i].encryption < 5 ? wifiAudit->aps[i].encryption : 2;
        pos += snprintf(context + pos, sizeof(context) - pos,
          "%d. %s ch%d %ddBm %s clients:%d HS:%s PMKID:%s\n",
          i+1,
          wifiAudit->aps[i].ssid[0] ? wifiAudit->aps[i].ssid : "Hidden",
          wifiAudit->aps[i].channel,
          wifiAudit->aps[i].rssi,
          enc[e],
          wifiAudit->aps[i].clientCount,
          wifiAudit->aps[i].handshakeCaptured ? "Y" : "N",
          wifiAudit->aps[i].pmkidCaptured ? "Y" : "N");
      }
    } else {
      strlcpy(context, "No APs scanned yet.", sizeof(context));
    }

    char question[640];
    snprintf(question, sizeof(question),
      "You are a WiFi security auditing assistant helping an authorized pentester. "
      "Given this scan:\n%s\n"
      "Which AP is the best target and what attack (deauth+handshake or PMKID) is recommended? "
      "Consider signal strength, encryption, and whether clients are present. "
      "Reply in 3-4 sentences max.", context);

    // One-shot LLM call with the advisor system prompt
    char sysPrompt[] =
      "You are a concise WiFi security advisor. "
      "Give practical, direct recommendations. "
      "Only suggest actions on networks the user is authorized to test.";

    JsonDocument req;
    req["model"]                  = _model;
    req["max_tokens"]             = 150;
    req["messages"][0]["role"]    = "system";
    req["messages"][0]["content"] = sysPrompt;
    req["messages"][1]["role"]    = "user";
    req["messages"][1]["content"] = question;
    String body; serializeJson(req, body);

    String host, path; int port;
    if (!_parseEndpoint(_owuEndpoint, host, path, port)) {
      _sendMessage("AI advisor: invalid or disallowed endpoint."); return;
    }
    char authHdr[160] = {0};
    if (_owuKey[0]) snprintf(authHdr, sizeof(authHdr), "Authorization: Bearer %s", _owuKey);

    char resp[2048] = {0};
    if (!_llmRequest(host.c_str(), path.c_str(), port,
                     authHdr[0] ? authHdr : nullptr,
                     body.c_str(), resp, sizeof(resp))) {
      _sendMessage("AI advisor: could not reach LLM endpoint."); return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp)) { _sendMessage("AI advisor: JSON parse error."); return; }
    const char* text = doc["choices"][0]["message"]["content"];
    if (text && text[0]) {
      char prefixed[600];
      snprintf(prefixed, sizeof(prefixed), "\xF0\x9F\xA4\x96 AI Advisor:\n%s", text);
      _sendWithKeyboard(prefixed, _buildWifiKeyboard());
    }
#else
    _sendMessage("WiFi audit not compiled in.");
#endif
  }

  // ── ASCII progress bar (10 chars wide) ───────────────────────────
  void _makeBar(char* out, int value) {
    int filled = constrain(value, 0, 100) / 10;
    int i = 0;
    out[i++] = '[';
    for (int j = 0; j < 10; j++) out[i++] = (j < filled) ? '#' : '-';
    out[i++] = ']'; out[i] = '\0';
  }

  // ════════════════════════════════════════════════════════════════
  //  HTTP LAYER
  // ════════════════════════════════════════════════════════════════

  // ── Telegram API requests, TLS encrypted ────────────────────────
  // api.telegram.org uses DigiCert (not the previously-pinned GoDaddy G2).
  // setInsecure() keeps the connection TLS-encrypted without certificate
  // chain verification, which is the standard approach for ESP32 bots.
  bool _tgApiRequest(const char* path,
                     const char* method, const char* extraHeader,
                     const char* body, char* outBuf, size_t outLen) {
    outBuf[0] = '\0';
    WiFiClientSecure client;
    client.setInsecure();   // TLS encrypted; Telegram uses DigiCert, not GoDaddy
    client.setTimeout((uint32_t)(TG_HTTP_TIMEOUT_MS / 1000));
    if (!client.connect("api.telegram.org", 443)) return false;
    return _sendHttp(client, "api.telegram.org", path, method, extraHeader, body, outBuf, outLen);
  }

  // ── User LLM endpoint requests ────────────────────────────────────
  // TLS_VERIFY_LLM=0 by default: home servers often use self-signed
  // certs or plain HTTP on LAN. Set TG_LLM_VERIFY_TLS 1 in config.h
  // to enable cert verification for external/cloud endpoints.
  bool _llmRequest(const char* host, const char* path, int port,
                   const char* extraHeader, const char* body,
                   char* outBuf, size_t outLen) {
    outBuf[0] = '\0';
    WiFiClientSecure client;
#if TG_LLM_VERIFY_TLS
    client.setCACert(nullptr);   // use system root store if available
#else
    client.setInsecure();
#endif
    client.setTimeout((uint32_t)(TG_HTTP_TIMEOUT_MS / 1000));
    if (!client.connect(host, port)) return false;
    return _sendHttp(client, host, path, "POST", extraHeader, body, outBuf, outLen);
  }

  // ── Shared HTTP send/receive (no TLS setup here) ─────────────────
  bool _sendHttp(WiFiClientSecure& client,
                 const char* host, const char* path,
                 const char* method, const char* extraHeader,
                 const char* body, char* outBuf, size_t outLen) {
    String req = String(method) + " " + path + " HTTP/1.1\r\n";
    req += String("Host: ") + host + "\r\n";
    if (extraHeader && extraHeader[0]) req += String(extraHeader) + "\r\n";
    if (body && body[0]) {
      req += "Content-Type: application/json\r\n";
      req += String("Content-Length: ") + strlen(body) + "\r\n";
    }
    req += "Connection: close\r\n\r\n";
    if (body && body[0]) req += body;
    client.print(req);

    unsigned long t0 = millis();
    while (client.connected() && millis() - t0 < TG_HTTP_TIMEOUT_MS) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }
    size_t pos = 0;
    while (client.available() && pos < outLen - 1 && millis() - t0 < TG_HTTP_TIMEOUT_MS)
      outBuf[pos++] = (char)client.read();
    outBuf[pos] = '\0';
    client.stop();
    return pos > 0;
  }

  // ── SSRF guard: validate LLM endpoint URL ────────────────────────
  // Allows:  https://any-host/...
  //          http://localhost/...   http://127.x.x.x/...  (local Ollama)
  // Rejects: anything else over plain http (prevents internal network probing)
  static bool _isEndpointAllowed(const char* ep) {
    if (!ep || !ep[0]) return false;
    String e(ep);
    if (e.startsWith("https://")) return true;              // HTTPS: always OK
    if (!e.startsWith("http://"))  return false;            // unknown scheme
    // Plain HTTP: only allow localhost / loopback
    String rest = e.substring(7);
    return rest.startsWith("localhost") ||
           rest.startsWith("127.")      ||
           rest.startsWith("[::1]");
  }

  // ── getUpdates, also extracts callback_query data ───────────────
  bool _pollTelegram(char* msgOut, size_t msgLen,
                     char* cbDataOut, size_t cbLen,
                     int64_t* cbMsgIdOut) {
    msgOut[0] = cbDataOut[0] = '\0';
    if (!_token[0]) return false;
    char path[160];
    snprintf(path, sizeof(path),
      "/bot%s/getUpdates?limit=5&timeout=0&offset=%lld",
      _token, (long long)_updateOffset);

    char resp[3072] = {0};
    if (!_tgApiRequest(path, "GET", nullptr, nullptr, resp, sizeof(resp)))
      return false;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) return false;
    if (!doc["ok"].as<bool>()) return false;

    JsonArray results = doc["result"].as<JsonArray>();
    if (results.size() == 0) return false;

    bool got = false;
    for (JsonObject upd : results) {
      int64_t uid = upd["update_id"].as<int64_t>();
      if (uid >= _updateOffset) {
        _updateOffset = uid + 1;
        if (_prefs) _prefs->putLong64(NVS_TG_OFFSET, _updateOffset);
      }

      // ── Callback query (inline button) ──────────────────────────
      JsonObject cbq = upd["callback_query"];
      if (cbq) {
        int64_t fromId = cbq["from"]["id"].as<int64_t>();
        // Auto-register first sender; reject all others once registered
        if (_chatId == 0) setAllowedChatId(fromId);
        if (fromId != _chatId) continue;             // strict: drop unknown IDs
        const char* cd = cbq["data"];
        if (cd && cd[0] && strlen(cd) < cbLen) {
          strlcpy(cbDataOut, cd, cbLen);
          if (cbMsgIdOut) *cbMsgIdOut = cbq["id"].as<int64_t>();
          got = true;
          continue;
        }
      }

      // ── Regular message ──────────────────────────────────────────
      JsonObject msg = upd["message"];
      if (!msg) continue;
      int64_t fromId = msg["chat"]["id"].as<int64_t>();
      const char* text = msg["text"];
      if (!text || !text[0]) continue;
      // Auto-register first sender; reject all others once registered
      if (_chatId == 0) setAllowedChatId(fromId);
      if (fromId != _chatId) continue;               // strict: drop unknown IDs
      // Cap message length, DoS guard
      if (strlen(text) > TG_MAX_MSG_LEN) continue;
      strlcpy(msgOut, text, msgLen);
      got = true;
    }
    return got;
  }

  // ── answerCallbackQuery (removes the "loading" spinner on the button)
  void _answerCallback(int64_t callbackId) {
    if (callbackId == 0) return;
    char path[128];
    snprintf(path, sizeof(path), "/bot%s/answerCallbackQuery", _token);
    char body[64];
    snprintf(body, sizeof(body), "{\"callback_query_id\":\"%lld\"}", (long long)callbackId);
    char resp[256] = {0};
    _tgApiRequest(path, "POST", nullptr, body, resp, sizeof(resp));
  }

  // ── sendMessage ───────────────────────────────────────────────────
  void _sendMessage(const char* text) {
    if (!_token[0] || _chatId == 0) return;
    char path[128];
    snprintf(path, sizeof(path), "/bot%s/sendMessage", _token);
    JsonDocument req;
    req["chat_id"] = _chatId;
    req["text"]    = text;   // ArduinoJson handles JSON escaping
    String body; serializeJson(req, body);
    char resp[512] = {0};
    _tgApiRequest(path, "POST", nullptr, body.c_str(), resp, sizeof(resp));
  }

  // ── sendMessage with inline keyboard ─────────────────────────────
  void _sendWithKeyboard(const char* text, const String& replyMarkupJson) {
    if (!_token[0] || _chatId == 0) return;
    char path[128];
    snprintf(path, sizeof(path), "/bot%s/sendMessage", _token);
    // Use ArduinoJson for the outer object so text is properly escaped
    JsonDocument req;
    req["chat_id"] = _chatId;
    req["text"]    = text;
    // Embed the pre-built keyboard JSON raw, parse it back in
    JsonDocument kbDoc;
    if (deserializeJson(kbDoc, replyMarkupJson) == DeserializationError::Ok)
      req["reply_markup"] = kbDoc.as<JsonObject>();
    String body; serializeJson(req, body);
    char resp[512] = {0};
    _tgApiRequest(path, "POST", nullptr, body.c_str(), resp, sizeof(resp));
  }

  // ── LLM chat completion ───────────────────────────────────────────
  // ── Parse an endpoint URL into host, path, port ──────────────────
  // Returns false if the endpoint fails the SSRF guard.
  static bool _parseEndpoint(const char* epRaw,
                              String& hostOut, String& pathOut, int& portOut) {
    if (!_isEndpointAllowed(epRaw)) return false;
    String ep(epRaw);
    bool tls = ep.startsWith("https://");
    portOut = tls ? 443 : 80;
    ep = ep.substring(tls ? 8 : 7);
    // Optional explicit port: host:port/path
    int colon = ep.indexOf(':');
    int slash  = ep.indexOf('/');
    if (colon >= 0 && (slash < 0 || colon < slash)) {
      hostOut = ep.substring(0, colon);
      String rest = ep.substring(colon + 1);
      int sl2 = rest.indexOf('/');
      portOut = (sl2 < 0) ? rest.toInt() : rest.substring(0, sl2).toInt();
      pathOut = (sl2 < 0) ? "/api/chat/completions" : rest.substring(sl2);
    } else {
      hostOut = (slash < 0) ? ep : ep.substring(0, slash);
      pathOut = (slash < 0) ? "/api/chat/completions" : ep.substring(slash);
    }
    return hostOut.length() > 0;
  }

  void _callLLM(const char* userMessage,
                int hun, int fat, int cle, int exp,
                char* responseOut, size_t responseLen) {
    strlcpy(responseOut, "...", responseLen);
    if (!_owuEndpoint[0]) return;
    const char* name = (_traits && _traits->name[0]) ? _traits->name : "Sablina";
    int play = _traits ? (int)_traits->playfulness : 60;
    int grmp = _traits ? (int)_traits->grumpiness  : 20;
    int soci = _traits ? (int)_traits->sociability : 70;
    char sysPrompt[512];
    snprintf(sysPrompt, sizeof(sysPrompt),
      "You are %s, a virtual tamagotchi with a big personality. "
      "Never call yourself a pet or an AI. "
      "Personality: playfulness=%d/100, grumpiness=%d/100, sociability=%d/100. "
      "Current vitals: hunger=%d%% fatigue=%d%% cleanliness=%d%% coins=%d. "
      "Reply in 1-3 short sentences. Stay in character. Be expressive.",
      name, play, grmp, soci, hun, fat, cle, exp);
    JsonDocument req;
    req["model"] = _model; req["max_tokens"] = 120;
    req["messages"][0]["role"]    = "system";
    req["messages"][0]["content"] = sysPrompt;
    req["messages"][1]["role"]    = "user";
    req["messages"][1]["content"] = userMessage;  // ArduinoJson escapes this
    String body; serializeJson(req, body);

    String host, path; int port;
    if (!_parseEndpoint(_owuEndpoint, host, path, port)) return;
    char authHdr[160] = {0};
    if (_owuKey[0]) snprintf(authHdr, sizeof(authHdr), "Authorization: Bearer %s", _owuKey);

    char resp[2048] = {0};
    if (!_llmRequest(host.c_str(), path.c_str(), port,
                     authHdr[0] ? authHdr : nullptr, body.c_str(),
                     resp, sizeof(resp))) return;
    JsonDocument doc;
    if (deserializeJson(doc, resp)) return;
    const char* text = doc["choices"][0]["message"]["content"];
    if (text && text[0]) strlcpy(responseOut, text, responseLen);
  }
};
