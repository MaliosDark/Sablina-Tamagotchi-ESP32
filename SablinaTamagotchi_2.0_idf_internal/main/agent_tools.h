// agent_tools.h – Sablina ReAct Agent: tool registry + decision logic
//
// Pattern:  OBSERVE → THINK (LLM flavor text) → DECIDE (deterministic) → ACT
//
// The LLM (stories260K) generates a short "thought" that gives the action
// personality; the actual tool is chosen by agent_decide_tool() based on
// hard state thresholds.  This keeps decisions reliable while the LLM
// provides charm.

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// ── Tool identifiers ───────────────────────────────────────────────────────
typedef enum {
    TOOL_NONE = 0,
    TOOL_FEED_PET,        // hunger low  → navigate KITCHEN → eat
    TOOL_SLEEP_PET,       // rest low    → navigate BEDROOM → rest
    TOOL_CLEAN_PET,       // hygiene low → navigate BATHROOM → clean
    TOOL_PLAY_GAME,       // play / happiness → navigate PLAYROOM
    TOOL_HUNT_WIFI,       // WiFi Food: scan networks, collect PMKIDs → earn coins
    TOOL_DEAUTH_TARGET,   // WiFi Food: send deauth frame to selected AP
    TOOL_BEACON_SPAM,     // WiFi Food: broadcast fake beacons
    TOOL_CHECK_STATS,     // show current needs + coins as popup
    TOOL_PEER_INTERACT,   // BLE peer exchange: chat or send gift
    TOOL_COUNT,
} sablina_tool_t;

// ── Snapshot of world state consumed by the agent ─────────────────────────
typedef struct {
    int hunger;             // 0-100
    int rest;               // 0-100
    int clean;              // 0-100
    int coins;              // 0-9999
    int wifi_networks;      // count from most recent scan (0 if no scan yet)
    bool wifi_available;    // WiFi hardware present and ready
    bool peer_visible;      // at least one BLE peer advertising nearby
    char peer_name[24];     // name of the closest peer (empty if none)
    sablina_tool_t last_tool; // previous tool so we avoid repeating forever
    uint32_t uptime_ms;
} agent_state_t;

// ── Result returned by agent_execute_tool() ───────────────────────────────
typedef struct {
    sablina_tool_t tool;
    bool           success;
    char           description[96]; // human-readable log line
    int            hunger_delta;
    int            rest_delta;
    int            clean_delta;
    int            coins_delta;
} tool_result_t;

// ── Urgency thresholds ────────────────────────────────────────────────────
#define AGENT_URGENT_THRESHOLD  30
#define AGENT_LOW_THRESHOLD     50
#define AGENT_SATED_THRESHOLD   75

// ── Deterministic tool selector ───────────────────────────────────────────
static inline sablina_tool_t agent_decide_tool(const agent_state_t *s)
{
    if (!s) return TOOL_NONE;

    // ── Critical needs: always address immediately ─────────────────────
    if (s->hunger < AGENT_URGENT_THRESHOLD) return TOOL_FEED_PET;
    if (s->rest   < AGENT_URGENT_THRESHOLD) return TOOL_SLEEP_PET;
    if (s->clean  < AGENT_URGENT_THRESHOLD) return TOOL_CLEAN_PET;

    // ── BLE peer in range: greet/chat (avoid consecutive loops) ───────
    if (s->peer_visible && s->last_tool != TOOL_PEER_INTERACT) {
        return TOOL_PEER_INTERACT;
    }

    // ── WiFi Food: collect coins when reserves are low ─────────────────
    if (s->wifi_available && s->coins < 20) {
        return TOOL_HUNT_WIFI;
    }

    // ── WiFi Food: occasionally deauth or spam beacons ─────────────────
    // (Only when there are actual networks visible and coins are fine.)
    if (s->wifi_available && s->wifi_networks > 0 && s->coins >= 20) {
        uint32_t slot = (s->uptime_ms / 45000) % 3; // rotate every 45 s slot
        if (slot == 1) return TOOL_DEAUTH_TARGET;
        if (slot == 2) return TOOL_BEACON_SPAM;
    }

    // ── Moderate needs ────────────────────────────────────────────────
    if (s->hunger < AGENT_LOW_THRESHOLD) return TOOL_FEED_PET;
    if (s->rest   < AGENT_LOW_THRESHOLD) return TOOL_SLEEP_PET;
    if (s->clean  < AGENT_LOW_THRESHOLD) return TOOL_CLEAN_PET;

    // ── Default: play, or periodically check stats ────────────────────
    uint32_t minute_slot = (s->uptime_ms / 60000) % 5;
    if (minute_slot == 0) return TOOL_CHECK_STATS;
    return TOOL_PLAY_GAME;
}

// ── Human-readable tool name (for logs and displays) ─────────────────────
static inline const char *tool_name(sablina_tool_t tool)
{
    switch (tool) {
        case TOOL_FEED_PET:      return "feed_pet";
        case TOOL_SLEEP_PET:     return "sleep_pet";
        case TOOL_CLEAN_PET:     return "clean_pet";
        case TOOL_PLAY_GAME:     return "play_game";
        case TOOL_HUNT_WIFI:     return "hunt_wifi";
        case TOOL_DEAUTH_TARGET: return "deauth_target";
        case TOOL_BEACON_SPAM:   return "beacon_spam";
        case TOOL_CHECK_STATS:   return "check_stats";
        case TOOL_PEER_INTERACT: return "peer_interact";
        default:                 return "none";
    }
}

// ── Short prompt fragment per tool (steers LLM flavor text) ──────────────
static inline const char *tool_thought_hint(sablina_tool_t tool)
{
    switch (tool) {
        case TOOL_FEED_PET:      return "so hungry, need snacks";
        case TOOL_SLEEP_PET:     return "feels tired and cozy";
        case TOOL_CLEAN_PET:     return "wants a fresh shower";
        case TOOL_PLAY_GAME:     return "wants to play";
        case TOOL_HUNT_WIFI:     return "senses nearby signals";
        case TOOL_DEAUTH_TARGET: return "notices a crowded network";
        case TOOL_BEACON_SPAM:   return "dreams of imaginary networks";
        case TOOL_CHECK_STATS:   return "checks how she is doing";
        case TOOL_PEER_INTERACT: return "senses a nearby friend";
        default:                 return "wonders what to do";
    }
}
