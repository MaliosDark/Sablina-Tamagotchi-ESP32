#pragma once

// ═══════════════════════════════════════════════════════════════════
//  SABLINA TAMAGOTCHI 2.0  –  Hardware & Feature Configuration
//  Target board : ESP32-S3  +  1.47" ST7789 172×320 IPS
//  e.g.  Waveshare ESP32-S3-LCD-1.47  /  similar clone boards
// ═══════════════════════════════════════════════════════════════════

// ─── TFT_eSPI User_Setup.h required settings ──────────────────────
//  Put this in  <ArduinoLibraries>/TFT_eSPI/User_Setup.h
//  (or create a User_Setup_Select.h that includes a custom file)
//
//  #define ST7789_DRIVER
//  #define TFT_WIDTH   172
//  #define TFT_HEIGHT  320
//  #define TFT_MOSI    11
//  #define TFT_SCLK    10
//  #define TFT_CS       9
//  #define TFT_DC       8
//  #define TFT_RST     12
//  #define TFT_BL      46        // backlight – controlled by ledcAttach
//  #define SPI_FREQUENCY  40000000
//  #define SPI_READ_FREQUENCY  6000000
// ──────────────────────────────────────────────────────────────────

// ── Display ───────────────────────────────────────────────────────
#define TFT_BL_PIN      46      // PWM backlight pin
#define SCREEN_W       320      // physical pixels after rotation-1 (landscape)
#define SCREEN_H       172
// Legacy game canvas  (all original coordinates still valid)
#define GAME_W         128
#define GAME_H         128
#define GAME_X         ((SCREEN_W - GAME_W) / 2 - SIDEBAR_W / 2)  // ~66
#define GAME_Y         ((SCREEN_H - GAME_H) / 2)                   // ~22
// Right sidebar
#define SIDEBAR_X      (GAME_X + GAME_W + 4)   // starts right after game area
#define SIDEBAR_W       88
#define SIDEBAR_H      SCREEN_H

// ── Buttons ───────────────────────────────────────────────────────
#define BTN_A_PIN        0      // Boot / main confirm
#define BTN_B_PIN       14      // Secondary / scroll  ← adjust for your board

// ── Touch panel ────────────────────────────────────────────────────
// This firmware currently targets non-touch ST7789 modules.
// Set to 1 and add a touch driver only if your hardware includes a touch IC.
#define HAS_TOUCH_PANEL   0

// ── IMU  QMI8658  (I²C) ───────────────────────────────────────────
#define IMU_SDA_PIN      6
#define IMU_SCL_PIN      7
#define QMI8658_I2C_ADDR 0x6B   // SA0 high → 0x6B ; SA0 low → 0x6A
// Shake threshold (accel magnitude delta above which = shake event)
#define SHAKE_THRESHOLD  2.5f   // g

// ── RGB LED  (WS2812 single LED) ──────────────────────────────────
#define RGB_PIN         38       // adjust for your board

// ── SD card  (separate SPI bus) ───────────────────────────────────
#define SD_CS_PIN       42
// SD shares main SPI only if wired so – left separate here

// ── PWM backlight ─────────────────────────────────────────────────
#define BL_PWM_FREQ     5000
#define BL_PWM_RES         8
// brightness levels (lower = brighter with most driver circuits)
const int BRIGHT_LEVELS[] = {10, 40, 80, 130, 200, 245};
#define BL_DEFAULT_IDX     3

// ── WiFi (stored in NVS; these are just compile-time defaults) ─────
#define WIFI_SSID_DEFAULT  ""
#define WIFI_PASS_DEFAULT  ""
#define WIFI_CONNECT_TIMEOUT_MS  10000

// ── LLM / AI personality ──────────────────────────────────────────
// OpenAI-compatible endpoint  (Ollama, LM Studio, OpenAI, etc.)
#define LLM_ENDPOINT_DEFAULT  "https://api.openai.com/v1/chat/completions"
#define LLM_MODEL_DEFAULT     "gpt-4o-mini"
// Tiny model preset for local gateways (e.g. Ollama on PC/RPi in same LAN)
// Example endpoint: http://192.168.1.10:11434/v1/chat/completions
#define LLM_MODEL_TINY_DEFAULT "qwen2.5:0.5b"
#define LLM_MAX_TOKENS         80
#define LLM_TIMEOUT_MS       6000
// How often (ms) to run an autonomous LLM "thought" when idle
#define LLM_IDLE_INTERVAL_MS  30000UL
// When offline, how often (ms) the pet generates autonomous local thoughts
#define LLM_OFFLINE_THOUGHT_INTERVAL_MS 20000UL

// ── BLE  GATT service UUIDs ───────────────────────────────────────
// Keep these consistent with your Expo app
#define BLE_DEVICE_NAME          "SablinaPet"
#define BLE_DEVICE_NAME_PREFIX   "Sablina"
#define BLE_SERVICE_UUID         "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// Characteristics
#define BLE_STATE_UUID           "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // notify → pet state JSON
#define BLE_CONFIG_UUID          "beb5483f-36e1-4688-b7f5-ea07361b26a8"  // write  → config JSON
#define BLE_CMD_UUID             "beb54840-36e1-4688-b7f5-ea07361b26a8"  // write  → command
#define BLE_PERSONALITY_UUID     "beb54841-36e1-4688-b7f5-ea07361b26a8"  // RW     → personality traits JSON
#define BLE_LLM_RESPONSE_UUID    "beb54842-36e1-4688-b7f5-ea07361b26a8"  // notify → last LLM response text

// BLE notify interval
#define BLE_NOTIFY_INTERVAL_MS   2000UL
#define BLE_PEER_SCAN_INTERVAL_MS 9000UL
#define BLE_PEER_SCAN_DURATION_S 1
#define BLE_PEER_TIMEOUT_MS      16000UL
#define BLE_PEER_CHAT_INTERVAL_MS 18000UL
#define BLE_PEER_MESSAGE_TTL_MS   6000UL
#define BLE_PEER_MESSAGE_MAX_TEXT 16
#define MAX_SOCIAL_PEERS          6

// Dual-speaker chat bubble colors (RGB565)
#define CHAT_BUBBLE_FRAME_COLOR  TFT_WHITE
#define CHAT_LOCAL_BG_COLOR      0x18C3
#define CHAT_LOCAL_TEXT_COLOR    0xFF3A
#define CHAT_PEER_BG_COLOR       0x09B1
#define CHAT_PEER_TEXT_COLOR     TFT_CYAN

// ── NVS persistence keys ──────────────────────────────────────────
#define NVS_NS             "sablina"
#define NVS_WIFI_SSID      "wifi_ssid"
#define NVS_WIFI_PASS      "wifi_pass"
#define NVS_LLM_KEY        "llm_key"
#define NVS_LLM_ENDPOINT   "llm_ep"
#define NVS_LLM_MODEL      "llm_model"
#define NVS_PET_NAME       "pet_name"
#define NVS_HUN_MAS        "hun_mas"
#define NVS_FAT_MAS        "fat_mas"
#define NVS_CLE_MAS        "cle_mas"
#define NVS_EXP_MAS        "exp_mas"
#define NVS_PERSONALITY    "pers"
#define NVS_FORCE_OFFLINE  "off_force"
#define NVS_SOCIAL_MEMORY  "peer_mem"
// ── Life-cycle persistence ────────────────────────────────────────
#define NVS_PET_STAGE      "pet_stage"   // uint8 0=BABY 1=CHILD 2=TEEN 3=ADULT 4=ELDER
#define NVS_PET_ALIVE      "pet_alive"   // uint8 1=alive 0=dead
#define NVS_PET_AGE_H      "pet_age_h"  // uint32 cumulative age in hours
#define NVS_PET_AGE_D      "pet_age_d"  // uint32 cumulative age in days
#define NVS_PET_SICK       "pet_sick"    // uint8 1=sick 0=healthy

// ── Compile-time feature flags ────────────────────────────────────
#define FEATURE_BLE          1   // enable BLE server
#define FEATURE_BLE_PEERS    1   // discover nearby Sablinas over BLE advertising
#define FEATURE_WIFI_LLM     1   // enable WiFi + LLM calls
#define FEATURE_IMU          1   // enable QMI8658 shake detection
#define FEATURE_RGB          1   // enable RGB mood LED

// ── Sound notification hooks (future flat speaker + mini amp) ─────
// Set SPEAKER_PIN to a valid GPIO once hardware is wired.
#define SPEAKER_PIN        (-1)
#define FEATURE_SOUND        1
#define SOUND_NOTIFY_BEEPS   2

// ── Vibration notification hooks (motor/driver) ────────────────────
// Set VIBRO_PIN to a valid GPIO connected to transistor/driver stage.
#define VIBRO_PIN          (-1)
#define FEATURE_VIBRATION    1
#define VIBRO_NOTIFY_PULSES  2
#define VIBRO_PULSE_MS      90

// ── Telegram Bot ──────────────────────────────────────────────────
#define FEATURE_TELEGRAM     1   // enable Telegram bot (requires secrets.h)
// NVS keys for Telegram credentials
// SECURITY NOTE: NVS partitions are stored in plaintext flash by default.
// To protect credentials at rest (bot token, API key, chat_id), enable
// NVS encryption via the ESP-IDF NVS encryption guide:
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html#nvs-encryption
// Generate an NVS encryption key partition and flash it separately; it is
// stored in a key-partition protected by eFuse BLOCK_KEY0 (HMAC-based scheme
// supported on ESP32-S3).  The Arduino IDE does not automate this, use
// ESP-IDF or PlatformIO for production builds if credential protection is
// required.
#define NVS_TG_TOKEN         "tg_token"
#define NVS_TG_CHAT_ID       "tg_chat_id"
#define NVS_TG_OWU_KEY       "tg_owu_key"
#define NVS_TG_OWU_EP        "tg_owu_ep"
#define NVS_TG_OWU_MODEL     "tg_owu_model"
#define NVS_TG_OFFSET        "tg_offset"

// ── WiFi Security Audit ───────────────────────────────────────────
#define FEATURE_WIFI_AUDIT   1   // enable WiFi pentesting module
// Channel hop interval (ms) during scanning
#define WIFI_AUDIT_HOP_MS   200
// Number of deauth frames per burst
#define WIFI_AUDIT_DEAUTH_COUNT  5
