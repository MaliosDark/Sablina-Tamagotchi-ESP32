// app_main.c – Sablina Tamagotchi IDF – ReAct Agent edition
//
// Architecture:
//   agent_task  ─── unified Observe → Think (LLM) → Decide → Act loop
//   app_main    ─── hardware init, BLE bridge, sound/haptic loop

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "llm.h"
#include "ble_bridge.h"
#include "agent_tools.h"
#include "lcd_ui.h"

static const char *TAG = "SABLINA_AGENT";

#ifndef CONFIG_SABLINA_LLM_PROFILE_SUPER_SMALL
#define CONFIG_SABLINA_LLM_PROFILE_SUPER_SMALL 1
#endif

#ifndef CONFIG_SABLINA_LLM_SUPER_SMALL_CHECKPOINT
#define CONFIG_SABLINA_LLM_SUPER_SMALL_CHECKPOINT "/data/stories260K.bin"
#endif

#ifndef CONFIG_SABLINA_LLM_SUPER_SMALL_TOKENIZER
#define CONFIG_SABLINA_LLM_SUPER_SMALL_TOKENIZER "/data/tok512.bin"
#endif

#ifndef CONFIG_SABLINA_LLM_SUPER_SMALL_THINK_STEPS
#define CONFIG_SABLINA_LLM_SUPER_SMALL_THINK_STEPS 32
#endif

#ifndef CONFIG_SABLINA_LLM_SUPER_SMALL_MAX_CHECKPOINT_KB
#define CONFIG_SABLINA_LLM_SUPER_SMALL_MAX_CHECKPOINT_KB 1152
#endif

#ifndef CONFIG_SABLINA_LLM_SUPER_SMALL_MIN_FREE_HEAP_KB
#define CONFIG_SABLINA_LLM_SUPER_SMALL_MIN_FREE_HEAP_KB 1024
#endif

#ifndef CONFIG_SABLINA_LLM_SMALL_CHECKPOINT
#define CONFIG_SABLINA_LLM_SMALL_CHECKPOINT "/data/stories1M.bin"
#endif

#ifndef CONFIG_SABLINA_LLM_SMALL_TOKENIZER
#define CONFIG_SABLINA_LLM_SMALL_TOKENIZER "/data/tok512.bin"
#endif

#ifndef CONFIG_SABLINA_LLM_SMALL_THINK_STEPS
#define CONFIG_SABLINA_LLM_SMALL_THINK_STEPS 48
#endif

#ifndef CONFIG_SABLINA_LLM_SMALL_MAX_CHECKPOINT_KB
#define CONFIG_SABLINA_LLM_SMALL_MAX_CHECKPOINT_KB 4096
#endif

#ifndef CONFIG_SABLINA_LLM_SMALL_MIN_FREE_HEAP_KB
#define CONFIG_SABLINA_LLM_SMALL_MIN_FREE_HEAP_KB 3072
#endif

#ifndef CONFIG_SABLINA_LLM_CUSTOM_NAME
#define CONFIG_SABLINA_LLM_CUSTOM_NAME "custom"
#endif

#ifndef CONFIG_SABLINA_LLM_CUSTOM_CHECKPOINT
#define CONFIG_SABLINA_LLM_CUSTOM_CHECKPOINT "/data/custom.bin"
#endif

#ifndef CONFIG_SABLINA_LLM_CUSTOM_TOKENIZER
#define CONFIG_SABLINA_LLM_CUSTOM_TOKENIZER "/data/tok512.bin"
#endif

#ifndef CONFIG_SABLINA_LLM_CUSTOM_THINK_STEPS
#define CONFIG_SABLINA_LLM_CUSTOM_THINK_STEPS 32
#endif

#ifndef CONFIG_SABLINA_LLM_CUSTOM_MAX_CHECKPOINT_KB
#define CONFIG_SABLINA_LLM_CUSTOM_MAX_CHECKPOINT_KB 1024
#endif

#ifndef CONFIG_SABLINA_LLM_CUSTOM_MIN_FREE_HEAP_KB
#define CONFIG_SABLINA_LLM_CUSTOM_MIN_FREE_HEAP_KB 1024
#endif

// ── Agent timing ─────────────────────────────────────────────────────────
#define AGENT_CYCLE_MS        12000  // time between full ReAct cycles
#define AGENT_URGENT_CYCLE_MS  4000  // faster loop when needs are critical
#define NEED_DECAY_EVERY_MS   15000  // how often needs decay by 1-2 pts

// ── Hardware config ────────────────────────────────────────────────────────
#define SPEAKER_GPIO  (-1)
#define VIBRO_GPIO    (-1)
#define NOTIFY_BEEPS  2
#define NOTIFY_VIBES  2
#define VIBE_PULSE_MS 90

// ── Room identifiers ───────────────────────────────────────────────────────
typedef enum {
    ROOM_LIVING = 0,
    ROOM_KITCHEN,
    ROOM_BATHROOM,
    ROOM_BEDROOM,
    ROOM_PLAYROOM,
    ROOM_LAB,
} mikuru_room_t;

// ── Global game state ──────────────────────────────────────────────────────
static int  g_hunger  = 68;
static int  g_rest    = 66;
static int  g_clean   = 70;
static int  g_coins   = 15;
static int  g_wifi_networks = 0;

static mikuru_room_t  g_active_room = ROOM_LIVING;
static sablina_tool_t g_last_tool   = TOOL_NONE;

static char g_current_thought[192];
static char g_last_tool_result[96];

static char     g_popup_text[192];
static uint32_t g_popup_until_ms    = 0;
static uint32_t g_last_need_decay_ms = 0;

static uint8_t  g_pending_beeps  = 0;
static uint8_t  g_pending_vibes  = 0;
static bool     g_vibe_active    = false;
static uint32_t g_vibe_toggle_ms = 0;

// LLM generation buffer (written by token_cb)
static char   g_gen_buf[256];
static size_t g_gen_len = 0;

typedef struct {
    const char *name;
    const char *checkpoint_path;
    const char *tokenizer_path;
    int think_steps;
    size_t max_checkpoint_bytes;
    size_t recommended_free_heap_bytes;
} llm_profile_t;

// ─────────────────────────────────────────────────────────────────────────
//  Utilities
// ─────────────────────────────────────────────────────────────────────────

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static int clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static const llm_profile_t *get_llm_profile(void)
{
#if CONFIG_SABLINA_LLM_PROFILE_SMALL
    static const llm_profile_t profile = {
        .name = "small",
        .checkpoint_path = CONFIG_SABLINA_LLM_SMALL_CHECKPOINT,
        .tokenizer_path = CONFIG_SABLINA_LLM_SMALL_TOKENIZER,
        .think_steps = CONFIG_SABLINA_LLM_SMALL_THINK_STEPS,
        .max_checkpoint_bytes = (size_t)CONFIG_SABLINA_LLM_SMALL_MAX_CHECKPOINT_KB * 1024u,
        .recommended_free_heap_bytes = (size_t)CONFIG_SABLINA_LLM_SMALL_MIN_FREE_HEAP_KB * 1024u,
    };
#elif CONFIG_SABLINA_LLM_PROFILE_CUSTOM
    static const llm_profile_t profile = {
        .name = CONFIG_SABLINA_LLM_CUSTOM_NAME,
        .checkpoint_path = CONFIG_SABLINA_LLM_CUSTOM_CHECKPOINT,
        .tokenizer_path = CONFIG_SABLINA_LLM_CUSTOM_TOKENIZER,
        .think_steps = CONFIG_SABLINA_LLM_CUSTOM_THINK_STEPS,
        .max_checkpoint_bytes = (size_t)CONFIG_SABLINA_LLM_CUSTOM_MAX_CHECKPOINT_KB * 1024u,
        .recommended_free_heap_bytes = (size_t)CONFIG_SABLINA_LLM_CUSTOM_MIN_FREE_HEAP_KB * 1024u,
    };
#else
    static const llm_profile_t profile = {
        .name = "super_small",
        .checkpoint_path = CONFIG_SABLINA_LLM_SUPER_SMALL_CHECKPOINT,
        .tokenizer_path = CONFIG_SABLINA_LLM_SUPER_SMALL_TOKENIZER,
        .think_steps = CONFIG_SABLINA_LLM_SUPER_SMALL_THINK_STEPS,
        .max_checkpoint_bytes = (size_t)CONFIG_SABLINA_LLM_SUPER_SMALL_MAX_CHECKPOINT_KB * 1024u,
        .recommended_free_heap_bytes = (size_t)CONFIG_SABLINA_LLM_SUPER_SMALL_MIN_FREE_HEAP_KB * 1024u,
    };
#endif
    return &profile;
}

static bool get_file_size_bytes(const char *path, size_t *size_out)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "Missing asset: %s", path ? path : "(null)");
        return false;
    }
    *size_out = (size_t)st.st_size;
    return true;
}

static bool validate_llm_profile(const llm_profile_t *profile)
{
    size_t checkpoint_size = 0;
    size_t tokenizer_size = 0;

    if (!profile) {
        ESP_LOGE(TAG, "LLM profile is null");
        return false;
    }

    if (!get_file_size_bytes(profile->checkpoint_path, &checkpoint_size) ||
        !get_file_size_bytes(profile->tokenizer_path, &tokenizer_size)) {
        return false;
    }

    ESP_LOGI(TAG,
             "LLM profile=%s checkpoint=%s (%u KB) tokenizer=%s (%u KB) think_steps=%d free_heap=%lu KB",
             profile->name,
             profile->checkpoint_path,
             (unsigned)(checkpoint_size / 1024u),
             profile->tokenizer_path,
             (unsigned)(tokenizer_size / 1024u),
             profile->think_steps,
             (unsigned long)(esp_get_free_heap_size() / 1024u));

    if (checkpoint_size > profile->max_checkpoint_bytes) {
        ESP_LOGE(TAG,
                 "Checkpoint exceeds profile budget: %u KB > %u KB",
                 (unsigned)(checkpoint_size / 1024u),
                 (unsigned)(profile->max_checkpoint_bytes / 1024u));
        return false;
    }

    if (esp_get_free_heap_size() < profile->recommended_free_heap_bytes) {
        ESP_LOGW(TAG,
                 "Free heap is below the recommended budget for profile %s: %lu KB < %u KB",
                 profile->name,
                 (unsigned long)(esp_get_free_heap_size() / 1024u),
                 (unsigned)(profile->recommended_free_heap_bytes / 1024u));
    }

    return true;
}

// ── Board hooks (weak – override in board-specific translation unit) ───────
__attribute__((weak)) void mikuru_ui_show_floating_popup(const char *text, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "POPUP (%u ms): %s", (unsigned)timeout_ms, text ? text : "");
}

__attribute__((weak)) void mikuru_ui_navigate_room(const char *room)
{
    ESP_LOGI(TAG, "NAVIGATE: %s", room ? room : "LIVING");
}

__attribute__((weak)) void mikuru_ui_perform_room_action(const char *room,
                                                          const char *action,
                                                          uint32_t hold_ms)
{
    ESP_LOGI(TAG, "ACTION: %s -> %s (%u ms)",
             room ? room : "LIVING", action ? action : "IDLE", (unsigned)hold_ms);
}

__attribute__((weak)) void mikuru_ui_set_animation(const char *animation)
{
    ESP_LOGI(TAG, "ANIM: %s", animation ? animation : "mikurugif");
}

__attribute__((weak)) void mikuru_audio_beep_once(void)
{
    ESP_LOGI(TAG, "BEEP");
}

// ─────────────────────────────────────────────────────────────────────────
//  Storage (SPIFFS)
// ─────────────────────────────────────────────────────────────────────────

static void init_storage(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/data",
        .partition_label        = NULL,
        .max_files              = 5,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
        return;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS total=%u used=%u", (unsigned)total, (unsigned)used);
}

// ─────────────────────────────────────────────────────────────────────────
//  Popup / sound / haptic
// ─────────────────────────────────────────────────────────────────────────

static void popup_show(const char *text)
{
    if (!text || !text[0]) return;
    strlcpy(g_popup_text, text, sizeof(g_popup_text));
    g_popup_until_ms = now_ms() + 6000;
    g_pending_beeps  = NOTIFY_BEEPS;
    g_pending_vibes  = NOTIFY_VIBES;
    g_vibe_active    = false;
    g_vibe_toggle_ms = 0;
    mikuru_ui_show_floating_popup(g_popup_text, 6000);
}

static void process_sound_haptic(void)
{
    uint32_t t = now_ms();

    if (g_pending_beeps > 0) {
        mikuru_audio_beep_once();
        g_pending_beeps--;
    }

    if (VIBRO_GPIO >= 0) {
        if (g_vibe_active) {
            if (t - g_vibe_toggle_ms >= VIBE_PULSE_MS) {
                gpio_set_level(VIBRO_GPIO, 0);
                g_vibe_active    = false;
                g_vibe_toggle_ms = t;
            }
        } else if (g_pending_vibes > 0 &&
                   (g_vibe_toggle_ms == 0 || t - g_vibe_toggle_ms >= 120)) {
            gpio_set_level(VIBRO_GPIO, 1);
            g_vibe_active    = true;
            g_vibe_toggle_ms = t;
            g_pending_vibes--;
        }
    } else if (g_pending_vibes > 0) {
        ESP_LOGI(TAG, "VIBRO");
        g_pending_vibes = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────
//  Room helpers
// ─────────────────────────────────────────────────────────────────────────

static const char *room_name_str(mikuru_room_t r)
{
    switch (r) {
        case ROOM_KITCHEN:  return "KITCHEN";
        case ROOM_BATHROOM: return "BATHROOM";
        case ROOM_BEDROOM:  return "BEDROOM";
        case ROOM_PLAYROOM: return "PLAYROOM";
        case ROOM_LAB:      return "LAB";
        default:            return "LIVING";
    }
}

static const char *room_animation(mikuru_room_t r)
{
    switch (r) {
        case ROOM_KITCHEN:  return "eatgif";
        case ROOM_BATHROOM: return "cleangif";
        case ROOM_BEDROOM:  return "sleepgif";
        case ROOM_PLAYROOM: return "gamegif";
        case ROOM_LAB:      return "gamewalk";
        default:            return "mikurugif";
    }
}

static void navigate_to(mikuru_room_t room)
{
    g_active_room = room;
    mikuru_ui_navigate_room(room_name_str(room));
    mikuru_ui_set_animation(room_animation(room));
}

// ─────────────────────────────────────────────────────────────────────────
//  Needs decay
// ─────────────────────────────────────────────────────────────────────────

static void decay_needs_if_due(void)
{
    uint32_t now = now_ms();
    if (g_last_need_decay_ms == 0) { g_last_need_decay_ms = now; return; }
    if (now - g_last_need_decay_ms < NEED_DECAY_EVERY_MS) return;
    g_last_need_decay_ms = now;
    g_hunger = clamp(g_hunger - 2, 0, 100);
    g_rest   = clamp(g_rest   - 1, 0, 100);
    g_clean  = clamp(g_clean  - 1, 0, 100);
}

// ─────────────────────────────────────────────────────────────────────────
//  OBSERVE – build agent_state_t from current globals + BLE
// ─────────────────────────────────────────────────────────────────────────

static agent_state_t observe_state(void)
{
    agent_state_t s;
    memset(&s, 0, sizeof(s));
    s.hunger         = g_hunger;
    s.rest           = g_rest;
    s.clean          = g_clean;
    s.coins          = g_coins;
    s.wifi_networks  = g_wifi_networks;
    s.wifi_available = true;
    s.peer_visible   = ble_bridge_peer_visible();
    s.last_tool      = g_last_tool;
    s.uptime_ms      = now_ms();

    const char *pn = ble_bridge_peer_name();
    if (pn && pn[0]) {
        strlcpy(s.peer_name, pn, sizeof(s.peer_name));
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────
//  THINK – LLM token callbacks + prompt builder
// ─────────────────────────────────────────────────────────────────────────

static void token_cb(const char *piece)
{
    if (!piece || !piece[0]) return;
    for (size_t i = 0; piece[i]; ++i) {
        unsigned char c = (unsigned char)piece[i];
        if ((isprint(c) || isspace(c)) && g_gen_len < sizeof(g_gen_buf) - 1) {
            g_gen_buf[g_gen_len++] = (char)c;
        }
    }
    g_gen_buf[g_gen_len] = '\0';
}

static void complete_cb(float tokens_ps)
{
    ESP_LOGI(TAG, "LLM done (%.2f tok/s): %s", tokens_ps, g_gen_buf);
}

static void build_react_prompt(const agent_state_t *s, sablina_tool_t tool,
                                char *buf, size_t buflen)
{
    snprintf(buf, buflen,
             "Sablina is a friendly digital girl. She %s. She says:",
             tool_thought_hint(tool));
}

// ─────────────────────────────────────────────────────────────────────────
//  ACT – tool implementations
// ─────────────────────────────────────────────────────────────────────────

static tool_result_t exec_hunt_wifi(const agent_state_t *s)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_HUNT_WIFI;

    // Stub: on real hardware call esp_wifi_scan_start / esp_wifi_ap_get_ap_num
    int found = g_wifi_networks;
    if (found == 0) {
        // Simulate finding 2-4 networks when no prior scan result
        found = 2 + (int)((s->uptime_ms / 3333) % 3);
    }
    int earned    = found > 10 ? 10 : found;
    g_wifi_networks = found;
    r.coins_delta = earned;
    r.success     = true;
    snprintf(r.description, sizeof(r.description),
             "hunt_wifi: %d nets, +%d coins", found, earned);
    return r;
}

static tool_result_t exec_deauth_target(const agent_state_t *s)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_DEAUTH_TARGET;

    if (g_wifi_networks == 0) {
        r = exec_hunt_wifi(s);
        r.tool = TOOL_DEAUTH_TARGET;
        snprintf(r.description, sizeof(r.description),
                 "deauth: scanned first (%d nets)", g_wifi_networks);
        return r;
    }
    int target_idx = (int)((s->uptime_ms / 7919) %
                           (uint32_t)(g_wifi_networks > 0 ? g_wifi_networks : 1));
    r.coins_delta = 3;
    r.success     = true;
    snprintf(r.description, sizeof(r.description),
             "deauth_target: AP#%d (+3 coins)", target_idx);
    return r;
}

static tool_result_t exec_beacon_spam(void)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_BEACON_SPAM;
    static const char *fake_ssids[] = {
        "Free_Snacks_WiFi", "Sablina_Network", "Not_A_Trap_5G",
        "FBI_Surveillance_Van", "MikuruSignal"
    };
    int idx = (int)((now_ms() / 11003) % 5);
    r.coins_delta = 2;
    r.success     = true;
    snprintf(r.description, sizeof(r.description),
             "beacon_spam: \"%s\" (+2 coins)", fake_ssids[idx]);
    return r;
}

static tool_result_t exec_feed_pet(void)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_FEED_PET;
    navigate_to(ROOM_KITCHEN);
    mikuru_ui_perform_room_action("KITCHEN", "EAT", 1200);
    vTaskDelay(pdMS_TO_TICKS(1200));
    r.hunger_delta = 20;
    r.rest_delta   = -1;
    r.success      = true;
    strlcpy(r.description, "feed_pet: ate (+20 hunger)", sizeof(r.description));
    navigate_to(ROOM_LIVING);
    return r;
}

static tool_result_t exec_sleep_pet(void)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_SLEEP_PET;
    navigate_to(ROOM_BEDROOM);
    mikuru_ui_perform_room_action("BEDROOM", "REST", 1400);
    vTaskDelay(pdMS_TO_TICKS(1400));
    r.rest_delta   = 22;
    r.hunger_delta = -1;
    r.success      = true;
    strlcpy(r.description, "sleep_pet: rested (+22 rest)", sizeof(r.description));
    navigate_to(ROOM_LIVING);
    return r;
}

static tool_result_t exec_clean_pet(void)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_CLEAN_PET;
    navigate_to(ROOM_BATHROOM);
    mikuru_ui_perform_room_action("BATHROOM", "CLEAN", 1200);
    vTaskDelay(pdMS_TO_TICKS(1200));
    r.clean_delta  = 26;
    r.hunger_delta = -1;
    r.success      = true;
    strlcpy(r.description, "clean_pet: washed (+26 clean)", sizeof(r.description));
    navigate_to(ROOM_LIVING);
    return r;
}

static tool_result_t exec_play_game(void)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_PLAY_GAME;
    navigate_to(ROOM_PLAYROOM);
    mikuru_ui_perform_room_action("PLAYROOM", "PLAY", 1000);
    vTaskDelay(pdMS_TO_TICKS(1000));
    r.hunger_delta = -3;
    r.rest_delta   = -4;
    r.clean_delta  = -2;
    r.coins_delta  = 2;
    r.success      = true;
    strlcpy(r.description, "play_game: played (+2 coins)", sizeof(r.description));
    navigate_to(ROOM_LIVING);
    return r;
}

static tool_result_t exec_check_stats(const agent_state_t *s)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool    = TOOL_CHECK_STATS;
    r.success = true;
    snprintf(r.description, sizeof(r.description),
             "H:%d R:%d C:%d coins:%d nets:%d",
             s->hunger, s->rest, s->clean, s->coins, s->wifi_networks);
    return r;
}

static tool_result_t exec_peer_interact(const agent_state_t *s)
{
    tool_result_t r;
    memset(&r, 0, sizeof(r));
    r.tool = TOOL_PEER_INTERACT;

    if (!s->peer_visible) {
        r.success = false;
        strlcpy(r.description, "peer_interact: no peer", sizeof(r.description));
        return r;
    }

    char msg[16];
    if (s->hunger < AGENT_LOW_THRESHOLD)     strlcpy(msg, "hungry here!",  sizeof(msg));
    else if (s->rest < AGENT_LOW_THRESHOLD)  strlcpy(msg, "sleepy here~",  sizeof(msg));
    else if (s->coins >= 20)                 strlcpy(msg, "snack gift",    sizeof(msg));
    else                                     strlcpy(msg, "hey there!",    sizeof(msg));

    ble_bridge_notify_text(msg);
    r.coins_delta = -2;
    r.success     = true;
    snprintf(r.description, sizeof(r.description),
             "peer: %s -> \"%s\"",
             s->peer_name[0] ? s->peer_name : "nearby", msg);
    return r;
}

// ── Main tool dispatcher ──────────────────────────────────────────────────
static tool_result_t execute_tool(sablina_tool_t tool, const agent_state_t *s)
{
    switch (tool) {
        case TOOL_FEED_PET:      return exec_feed_pet();
        case TOOL_SLEEP_PET:     return exec_sleep_pet();
        case TOOL_CLEAN_PET:     return exec_clean_pet();
        case TOOL_PLAY_GAME:     return exec_play_game();
        case TOOL_HUNT_WIFI:     return exec_hunt_wifi(s);
        case TOOL_DEAUTH_TARGET: return exec_deauth_target(s);
        case TOOL_BEACON_SPAM:   return exec_beacon_spam();
        case TOOL_CHECK_STATS:   return exec_check_stats(s);
        case TOOL_PEER_INTERACT: return exec_peer_interact(s);
        default: {
            tool_result_t r;
            memset(&r, 0, sizeof(r));
            r.tool    = TOOL_NONE;
            r.success = false;
            strlcpy(r.description, "no-op", sizeof(r.description));
            return r;
        }
    }
}

static void apply_result(const tool_result_t *r)
{
    g_hunger = clamp(g_hunger + r->hunger_delta, 0, 100);
    g_rest   = clamp(g_rest   + r->rest_delta,   0, 100);
    g_clean  = clamp(g_clean  + r->clean_delta,  0, 100);
    g_coins  = clamp(g_coins  + r->coins_delta,  0, 9999);
}

// ─────────────────────────────────────────────────────────────────────────
//  AGENT TASK – full ReAct loop
// ─────────────────────────────────────────────────────────────────────────

static void agent_task(void *arg)
{
    (void)arg;
    const llm_profile_t *llm_profile = get_llm_profile();

    Transformer transformer;
    Tokenizer   tokenizer;
    Sampler     sampler;

    if (!validate_llm_profile(llm_profile)) {
        ESP_LOGE(TAG, "LLM profile validation failed, stopping agent task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Loading %s transformer from %s ...",
             llm_profile->name, llm_profile->checkpoint_path);
    build_transformer(&transformer, (char *)llm_profile->checkpoint_path);
    build_tokenizer(&tokenizer,
                    (char *)llm_profile->tokenizer_path,
                    transformer.config.vocab_size);
    build_sampler(&sampler, transformer.config.vocab_size,
                  1.0f, 0.9f, (unsigned long long)time(NULL));
    ESP_LOGI(TAG, "LLM ready profile=%s (vocab=%d seq=%d)",
             llm_profile->name,
             transformer.config.vocab_size,
             transformer.config.seq_len);

    while (1) {
        // 1. OBSERVE
        decay_needs_if_due();
        agent_state_t state = observe_state();

        // 2. DECIDE (before LLM so we can steer the flavor prompt)
        sablina_tool_t tool = agent_decide_tool(&state);
        ESP_LOGI(TAG, "[REACT] tool=%s H=%d R=%d C=%d coins=%d peer=%s",
                 tool_name(tool),
                 state.hunger, state.rest, state.clean, state.coins,
                 state.peer_visible ? state.peer_name : "none");

        // 3. THINK (LLM generates flavor thought, ~32 tokens)
        char prompt[160];
        build_react_prompt(&state, tool, prompt, sizeof(prompt));
        g_gen_len    = 0;
        g_gen_buf[0] = '\0';

        generate_with_callbacks(&transformer, &tokenizer, &sampler,
                                prompt, llm_profile->think_steps, complete_cb, token_cb);

        // Trim trailing whitespace
        int tl = (int)g_gen_len;
        while (tl > 0 && isspace((unsigned char)g_gen_buf[tl - 1])) tl--;
        g_gen_buf[tl] = '\0';
        g_gen_len     = (size_t)tl;
        strlcpy(g_current_thought,
                g_gen_buf[0] ? g_gen_buf : "...",
                sizeof(g_current_thought));

        // 4. ACT
        tool_result_t result = execute_tool(tool, &state);
        apply_result(&result);
        g_last_tool = tool;
        strlcpy(g_last_tool_result, result.description, sizeof(g_last_tool_result));

        // 5. DISPLAY – thought + tool + result
        char display[256];
        snprintf(display, sizeof(display),
                 "[%s] %s | %s",
                 tool_name(tool),
                 g_current_thought[0] ? g_current_thought : "...",
                 result.description);
        popup_show(display);
        lcd_ui_update(g_hunger, g_rest, g_clean, g_coins,
                      tool_name(tool), g_current_thought, result.description);

        // Broadcast compact state to BLE peers
        char ble_brief[48];
        snprintf(ble_brief, sizeof(ble_brief),
                 "%.12s|H%dR%dC%d$%d",
                 tool_name(tool), g_hunger, g_rest, g_clean, g_coins);
        ble_bridge_notify_text(ble_brief);

        ESP_LOGI(TAG, "[REACT] result: %s  new H=%d R=%d C=%d coins=%d",
                 result.description, g_hunger, g_rest, g_clean, g_coins);

        // 6. WAIT – shorter cycle when needs are urgent
        bool urgent = (g_hunger < AGENT_URGENT_THRESHOLD ||
                       g_rest   < AGENT_URGENT_THRESHOLD ||
                       g_clean  < AGENT_URGENT_THRESHOLD);
        vTaskDelay(pdMS_TO_TICKS(urgent ? AGENT_URGENT_CYCLE_MS : AGENT_CYCLE_MS));
    }
}

// ─────────────────────────────────────────────────────────────────────────
//  app_main
// ─────────────────────────────────────────────────────────────────────────

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    init_storage();
    lcd_ui_init();
    ble_bridge_init();

    if (VIBRO_GPIO >= 0) {
        gpio_config_t io = {
            .pin_bit_mask  = (1ULL << VIBRO_GPIO),
            .mode          = GPIO_MODE_OUTPUT,
            .pull_up_en    = GPIO_PULLUP_DISABLE,
            .pull_down_en  = GPIO_PULLDOWN_DISABLE,
            .intr_type     = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        gpio_set_level(VIBRO_GPIO, 0);
    }

    // Single ReAct agent task (replaces the old llm_task + room_task pair)
    xTaskCreate(agent_task, "agent_task", 20480, NULL, 5, NULL);

    // Main loop: incoming BLE messages + sound/haptic
    while (1) {
        char peer_name[24];
        char peer_text[17];
        if (ble_bridge_poll_text(peer_name, sizeof(peer_name),
                                 peer_text, sizeof(peer_text))) {
            char peer_popup[96];
            snprintf(peer_popup, sizeof(peer_popup),
                     "%s: %s",
                     peer_name[0] ? peer_name : "Nearby Sablina", peer_text);
            popup_show(peer_popup);
        }

        if (g_popup_until_ms && now_ms() > g_popup_until_ms) {
            g_popup_until_ms = 0;
            g_popup_text[0]  = '\0';
        }

        process_sound_haptic();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

