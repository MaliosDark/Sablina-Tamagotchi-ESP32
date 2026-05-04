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
//  #define USE_HSPI_PORT
//  #define TFT_MOSI    45
//  #define TFT_SCLK    40
//  #define TFT_CS      42
//  #define TFT_DC      41
//  #define TFT_RST     39
//  #define TFT_BL      46        // backlight on this verified board variant
//  #define TFT_OFFSET_X 34
//  #define TFT_OFFSET_Y  0
//  #define TFT_INVERSION_ON
//  #define SPI_FREQUENCY  40000000
//  #define SPI_READ_FREQUENCY  6000000
// ──────────────────────────────────────────────────────────────────

// ── Display ───────────────────────────────────────────────────────
#define TFT_BL_PIN      46      // verified PWM backlight pin on this board
#define TFT_BL_PIN_ALT  -1      // disable fallback to avoid driving the wrong BL path
#define SCREEN_W       320      // physical pixels after rotation-1 (landscape)
#define SCREEN_H       172
// Legacy game canvas  (all original coordinates still valid – images are
// scaled from virtual 128×128 space to GAME_W×GAME_H via pushImageScaled)
#define GAME_W         234     // matches simulator LEFT_W - 2
#define GAME_H         157     // matches simulator SCREEN.h - GAME_Y - 1
#define GAME_X           1     // matches simulator GAME_X (LEFT_X + 1)
#define GAME_Y          14     // matches simulator GAME_TOPBAR_H + 2
// Right sidebar
#define SIDEBAR_X      236     // matches simulator RIGHT_X (LEFT_X + LEFT_W)
#define SIDEBAR_W       84     // matches simulator RIGHT_W (SCREEN_W - 236)
#define SIDEBAR_H      SCREEN_H

// ── Buttons ───────────────────────────────────────────────────────
// Hardware reality: Waveshare ESP32-S3-LCD-1.47 has ONLY ONE GPIO button.
//   GPIO0  = BOOT button (the ONLY usable button)
//   RST    = EN / hardware-reset pin  (NOT readable as GPIO)
// Both pins are set to 0 here; the virtual button state machine in .ino
// distinguishes short-press (navigate) from long-press (select/confirm).
#define BTN_A_PIN        0      // BOOT button – long  press (≥600 ms) = select
#define BTN_B_PIN        0      // BOOT button – short press (< 600 ms) = navigate

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
#define BL_PWM_FREQ     1000
#define BL_PWM_RES        10
// This board variant responds to 10-bit PWM on GPIO46, with max duty required
// for a reliably visible backlight.
const int BRIGHT_LEVELS[] = {120, 260, 420, 620, 820, 1023};
#define BL_DEFAULT_IDX     5
// Keep PWM enabled; this board was verified on GPIO46 with PWM MAX.
#define BL_FORCE_ALWAYS_ON 0

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
#define BLE_PEER_SCAN_INTERVAL_MS 3000UL   // scan more often
#define BLE_PEER_SCAN_DURATION_S  2        // 2-second window (was 1)
#define BLE_PEER_TIMEOUT_MS      12000UL   // 4 scan cycles to lose peer
#define BLE_PEER_CHAT_INTERVAL_MS 12000UL
#define BLE_PEER_MESSAGE_TTL_MS   9000UL   // longer than 3 scan cycles
#define BLE_PEER_MESSAGE_MAX_TEXT 15
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
// ── Lifetime counters (badges / achievements) ─────────────────────
#define NVS_LT_FOOD        "lt_food"     // uint32 total food items eaten
#define NVS_LT_CLEANS      "lt_cleans"   // uint32 total shower/clean actions
#define NVS_LT_SLEEPS      "lt_sleeps"   // uint32 total sleep sessions
#define NVS_LT_PETS        "lt_pets"     // uint32 total pet/cuddle actions
#define NVS_LT_GAMES       "lt_games"    // uint32 total mini-games played
#define NVS_LT_WINS        "lt_wins"     // uint32 total mini-games won
#define NVS_LT_COINS_E     "lt_coins_e"  // uint32 total coins ever earned
#define NVS_LT_COINS_S     "lt_coins_s"  // uint32 total coins ever spent
#define NVS_LT_SCANS       "lt_scans"    // uint32 total WiFi scans performed
#define NVS_LT_NETS        "lt_nets"     // uint32 maximum networks found in one scan

// ── Compile-time feature flags ────────────────────────────────────
#define FEATURE_BLE          0   // temporarily disabled for Arduino->IDF bridge bring-up
#define FEATURE_BLE_PEERS    0   // temporarily disabled for Arduino->IDF bridge bring-up
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

// ── Platform Canvas integration ───────────────────────────────────
#define NVS_PLATFORM_URL     "plt_url"    // Canvas server base URL e.g. https://canvas.host
#define NVS_PLATFORM_KEY     "plt_key"    // API key from /api/auth/apikey

// ── Trait Evolution ──────────────────────────────────────────────────
#define NVS_TRAIT_CURIOSITY  "tr_curio"   // uint8 0-100
#define NVS_TRAIT_ACTIVITY   "tr_activ"   // uint8 0-100
#define NVS_TRAIT_STRESS     "tr_stress"  // uint8 0-100

// ── Day/Night ────────────────────────────────────────────────────────
// NTP pool for time sync
#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"
// Hour range considered "night" (24-hour clock)
#define NIGHT_HOUR_START     22   // 10 PM
#define NIGHT_HOUR_END       7    // 7 AM
// Dim level during night (index into BRIGHT_LEVELS[])
#define BL_NIGHT_IDX         1
// Dark-mode dim level (index into BRIGHT_LEVELS[])
#define BL_DARKMODE_IDX      1

// ── WiFi Security Audit ───────────────────────────────────────────
#define FEATURE_WIFI_AUDIT   0   // temporarily disabled for Arduino->IDF bridge bring-up
// Channel hop interval (ms) during scanning
#define WIFI_AUDIT_HOP_MS   200
// Number of deauth frames per burst
#define WIFI_AUDIT_DEAUTH_COUNT  5
