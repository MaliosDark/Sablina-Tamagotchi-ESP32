#pragma once
// ═══════════════════════════════════════════════════════════════════
//  LLM Personality Engine
//  • When WiFi is up  → calls an OpenAI-compatible REST endpoint
//  • When offline     → uses a lightweight offline mood engine
//  Dependencies : ArduinoJson, WiFiClientSecure (builtin ESP32)
// ═══════════════════════════════════════════════════════════════════
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstring>

// ── Mood enum (drives RGB LED + display reactions) ─────────────────
enum PetMood {
  MOOD_HAPPY    = 0,
  MOOD_HUNGRY   = 1,
  MOOD_TIRED    = 2,
  MOOD_DIRTY    = 3,
  MOOD_BORED    = 4,
  MOOD_PLAYFUL  = 5,
  MOOD_SAD      = 6,
  MOOD_EXCITED  = 7
};

// ── Personality traits (customisable via BLE app) ──────────────────
struct PersonalityTraits {
  uint8_t playfulness;   // 0-100
  uint8_t grumpiness;    // 0-100
  uint8_t sociability;   // 0-100
  uint8_t gluttony;      // 0-100
  char    name[24];      // Sablina name
};

// ── LLMPersonality class ──────────────────────────────────────────
class LLMPersonality {
public:
  char     lastResponse[256] = "...";
  PetMood  currentMood       = MOOD_HAPPY;
  bool     responseReady     = false;

  PersonalityTraits traits = {60, 20, 70, 40, "Sablina"};

  void begin(Preferences* prefs) {
    _prefs = prefs;
    _loadConfig();
    _loadTraits();
  }

  // ── Call from loop() – non-blocking via flag ────────────────────
  void tick(int hun, int fat, int cle, int exp, unsigned long nowMs) {
    // Update mood from stats (always runs, offline)
    currentMood = _computeMood(hun, fat, cle, exp);

    const bool onlineReady = (!_forceOffline) && _llmEnabled && WiFi.status() == WL_CONNECTED;

    // Online path: query remote/OpenAI-compatible model.
    if (onlineReady) {
      if (nowMs - _lastLlmCall < LLM_IDLE_INTERVAL_MS) return;
      _lastLlmCall = nowMs;
      _callLLM(hun, fat, cle, exp);
      return;
    }

    // Offline path: autonomous local thoughts so pet still feels alive.
    if (nowMs - _lastOfflineThought >= LLM_OFFLINE_THOUGHT_INTERVAL_MS) {
      _lastOfflineThought = nowMs;
      _offlineAutonomousThought(hun, fat, cle, exp);
    }
  }

  // ── Trigger an immediate LLM reaction to an event ──────────────
  void reactToEvent(const char* event, int hun, int fat, int cle) {
    if (_forceOffline || !_llmEnabled || WiFi.status() != WL_CONNECTED) {
      _offlineReact(event, hun, fat, cle);
      return;
    }
    _callLLM(hun, fat, cle, 0, event);
  }

  // ── Config setters (called from BLE service on write) ──────────
  void setApiKey(const char* key) {
    strlcpy(_apiKey, key, sizeof(_apiKey));
    _prefs->putString(NVS_LLM_KEY, key);
    _refreshLlmEnabled();
  }

  void setEndpoint(const char* ep) {
    strlcpy(_endpoint, ep, sizeof(_endpoint));
    _prefs->putString(NVS_LLM_ENDPOINT, ep);
    _refreshLlmEnabled();
  }

  void setModel(const char* model) {
    strlcpy(_model, model, sizeof(_model));
    _prefs->putString(NVS_LLM_MODEL, model);
  }

  void setTraitsFromJson(const char* json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    traits.playfulness = doc["playfulness"] | traits.playfulness;
    traits.grumpiness  = doc["grumpiness"]  | traits.grumpiness;
    traits.sociability = doc["sociability"] | traits.sociability;
    traits.gluttony    = doc["gluttony"]    | traits.gluttony;
    const char* n = doc["name"];
    if (n) strlcpy(traits.name, n, sizeof(traits.name));
    _saveTraits();
  }

  void traitsToJson(char* buf, size_t len) {
    snprintf(buf, len,
      "{\"name\":\"%s\",\"playfulness\":%d,\"grumpiness\":%d,"
      "\"sociability\":%d,\"gluttony\":%d}",
      traits.name, traits.playfulness, traits.grumpiness,
      traits.sociability, traits.gluttony);
  }

  bool isOnline() { return (!_forceOffline) && _llmEnabled && WiFi.status() == WL_CONNECTED; }
  bool isForceOffline() const { return _forceOffline; }

  void setForceOffline(bool enabled) {
    _forceOffline = enabled;
    if (_prefs) _prefs->putBool(NVS_FORCE_OFFLINE, enabled);
  }

  // ── RGB mood colour ─────────────────────────────────────────────
  void getMoodColor(uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (currentMood) {
      case MOOD_HAPPY:   r=0;   g=255; b=80;  break;
      case MOOD_HUNGRY:  r=255; g=100; b=0;   break;
      case MOOD_TIRED:   r=80;  g=0;   b=255; break;
      case MOOD_DIRTY:   r=120; g=80;  b=0;   break;
      case MOOD_BORED:   r=100; g=100; b=100; break;
      case MOOD_PLAYFUL: r=255; g=255; b=0;   break;
      case MOOD_SAD:     r=0;   g=50;  b=200; break;
      case MOOD_EXCITED: r=255; g=0;   b=200; break;
      default:           r=255; g=255; b=255; break;
    }
  }

  const char* moodName() {
    const char* names[] = {"Happy","Hungry","Tired","Dirty",
                           "Bored","Playful","Sad","Excited"};
    return names[currentMood];
  }

  // ── Peer social line generator (uses anti-repeat engine) ────────
  // Picks a short phrase (≤15 chars) for BLE peer chat, personality-aware.
  // call from choosePeerOfferText / choosePeerReplyText in the main sketch.
  void pickPeerLine(const char* const* opts, uint8_t count,
                    char* out, size_t outLen) {
    if (!opts || count == 0 || !out || outLen == 0) { out[0]='\0'; return; }
    uint8_t start = (uint8_t)random(0, count);
    for (uint8_t i = 0; i < count; ++i) {
      const char* c = opts[(start + i) % count];
      if (!_recentlyUsedOfflineLine(c)) {
        strlcpy(out, c, outLen);
        _rememberOfflineLine(c);
        return;
      }
    }
    strlcpy(out, opts[start], outLen);
    _rememberOfflineLine(out);
  }

  // ── Public peer-chat helpers (call from main sketch) ────────────
  // All phrases are ≤15 chars to fit the BLE payload limit.
public:
  void peerOffer(bool justDetected, int affinity, int hun, int fat, int cle,
                 char* out, size_t outLen) {
    if (!out || outLen == 0) return;
    out[0] = '\0';

    if (justDetected) {
      if (affinity >= 50) {
        static const char* hi[] = {
          "Missed you!", "There you are!", "You're back!", "Finally, hi!",
          "I missed you!", "Long time!", "Found you again!"
        };
        pickPeerLine(hi, 7, out, outLen);
      } else if (affinity >= 20) {
        static const char* hi[] = {
          "Hey, you're here!", "Oh hey!", "You again!", "Still around?",
          "Hey there!", "Good to meet!", "Hi, neighbor!"
        };
        pickPeerLine(hi, 7, out, outLen);
      } else {
        static const char* hi[] = {
          "Hello nearby.", "Hey, stranger!", "Hi there!", "Oh! Company!",
          "Hello!", "I see you!", "Who's nearby?"
        };
        pickPeerLine(hi, 7, out, outLen);
      }
      return;
    }

    // Ongoing chat — mood-driven
    if (currentMood == MOOD_PLAYFUL || currentMood == MOOD_EXCITED) {
      static const char* play[] = {
        "Race time?", "Wanna play?", "Let's have fun!", "Challenge me!",
        "Catch up fast!", "Tag, you're it!", "Play with me?"
      };
      pickPeerLine(play, 7, out, outLen);
    } else if (currentMood == MOOD_HUNGRY || hun < 35) {
      static const char* eat[] = {
        "Snacks hunt?", "Feed me first?", "I need snacks.", "So hungry...",
        "Hungry here!", "Find food?", "Empty tummy!"
      };
      pickPeerLine(eat, 7, out, outLen);
    } else if (currentMood == MOOD_TIRED || fat < 35) {
      static const char* rest[] = {
        "Slow walk?", "I'm tired too.", "Rest a bit?", "Need a break.",
        "Sleepy today.", "Walk slowly?", "Low energy..."
      };
      pickPeerLine(rest, 7, out, outLen);
    } else if (currentMood == MOOD_DIRTY || cle < 35) {
      static const char* cln[] = {
        "Need a bath.", "So dusty here.", "Clean up time?", "Dirty today.",
        "Let's clean?", "Bath needed!", "Messy day..."
      };
      pickPeerLine(cln, 7, out, outLen);
    } else if (traits.grumpiness > 60) {
      static const char* grump[] = {
        "What do you want?", "Leave me alone.", "Hmph.", "Not today.",
        "I'm busy.", "Just passing.", "Go away!"
      };
      pickPeerLine(grump, 7, out, outLen);
    } else if (traits.sociability > 60 || affinity >= 25) {
      static const char* social[] = {
        "How are you?", "Miss me?", "Stay close.", "Signal check!",
        "Good to see you", "Tell me a story", "Wanna chat?"
      };
      pickPeerLine(social, 7, out, outLen);
    } else {
      static const char* gen[] = {
        "You're close.", "Hello again!", "Hey again!", "You're near!",
        "Come here!", "I see you.", "Oh hey!"
      };
      pickPeerLine(gen, 7, out, outLen);
    }
  }

  void peerReply(const char* peerText, int affinity, int hun, int fat, int cle,
                 char* out, size_t outLen) {
    if (!out || outLen == 0) return;
    out[0] = '\0';
    String msg = String(peerText ? peerText : "");
    msg.toLowerCase();

    // Context-sensitive replies first
    if (msg.indexOf("race") >= 0 || msg.indexOf("challenge") >= 0 ||
        msg.indexOf("tag") >= 0  || msg.indexOf("catch") >= 0) {
      static const char* race[] = {
        "You're on!", "Try to keep up!", "Ready? Go!", "I'm faster!",
        "Let's do this!", "Game on!", "You can't catch me"
      };
      pickPeerLine(race, 7, out, outLen);
    } else if (msg.indexOf("play") >= 0 || msg.indexOf("fun") >= 0 ||
               msg.indexOf("wanna") >= 0) {
      static const char* play[] = {
        "Yes! Let's play!", "Always! Let's go", "Sure, why not?", "I'm in!",
        "Of course!", "Let's do it!", "Fun time!"
      };
      pickPeerLine(play, 7, out, outLen);
    } else if (msg.indexOf("snack") >= 0 || msg.indexOf("food") >= 0 ||
               msg.indexOf("hungry") >= 0 || msg.indexOf("eat") >= 0) {
      if (affinity >= 25) {
        static const char* food[] = {
          "Snack gift!", "Here, take this!", "Sharing is caring",
          "Have some snacks!", "Eat up, friend!", "I'll share mine.",
          "Snacks together!"
        };
        pickPeerLine(food, 7, out, outLen);
      } else {
        static const char* food[] = {
          "I'm hungry too.", "Find food first?", "Same here...",
          "I need snacks too", "Let's find food.", "Starving here!",
          "Me too, actually."
        };
        pickPeerLine(food, 7, out, outLen);
      }
    } else if (msg.indexOf("tired") >= 0 || msg.indexOf("rest") >= 0 ||
               msg.indexOf("slow") >= 0 || msg.indexOf("sleep") >= 0) {
      if (affinity >= 25) {
        static const char* rest[] = {
          "Rest gift!", "Take it easy.", "I'll help you rest",
          "Slow down with me", "Nap time?", "Rest well, friend",
          "Let's rest together"
        };
        pickPeerLine(rest, 7, out, outLen);
      } else {
        static const char* rest[] = {
          "I'll keep pace.", "Slow is fine.", "Rest a bit then.",
          "I'm tired too.", "Let's both rest.", "Take your time.",
          "No rush here."
        };
        pickPeerLine(rest, 7, out, outLen);
      }
    } else if (msg.indexOf("clean") >= 0 || msg.indexOf("bath") >= 0 ||
               msg.indexOf("dust") >= 0 || msg.indexOf("dirty") >= 0) {
      if (affinity >= 25) {
        static const char* cln[] = {
          "Clean gift!", "I'll help you!", "Bath time!", "Scrub scrub!",
          "Clean together!", "Let me help.", "Cleanliness wins!"
        };
        pickPeerLine(cln, 7, out, outLen);
      } else {
        static const char* cln[] = {
          "We can clean.", "Soap time?", "Dirty too?", "Let's tidy up.",
          "Yeah, let's wash.", "Messy here too.", "Clean up crew!"
        };
        pickPeerLine(cln, 7, out, outLen);
      }
    } else if (msg.indexOf("miss") >= 0 || msg.indexOf("found") >= 0 ||
               msg.indexOf("back") >= 0 || msg.indexOf("there") >= 0) {
      if (affinity >= 50) {
        static const char* miss[] = {
          "Missed you too!", "I was waiting!", "So glad you're here",
          "Finally together!", "Don't go again!", "You were gone long",
          "Yay, you're back!"
        };
        pickPeerLine(miss, 7, out, outLen);
      } else {
        static const char* miss[] = {
          "I see you too.", "Good to meet!", "Hello again!", "Hi there!",
          "Nice to see you.", "You're around!", "Hey, you're here."
        };
        pickPeerLine(miss, 7, out, outLen);
      }
    } else if (msg.indexOf("signal") >= 0 || msg.indexOf("check") >= 0) {
      static const char* sig[] = {
        "Signal strong!", "Loud and clear!", "I hear you!",
        "Receiving you!", "5 by 5!", "Clear signal!", "Roger that!"
      };
      pickPeerLine(sig, 7, out, outLen);
    } else if (msg.indexOf("hello") >= 0 || msg.indexOf("hey") >= 0 ||
               msg.indexOf("hi") >= 0) {
      if (currentMood == MOOD_PLAYFUL || currentMood == MOOD_EXCITED) {
        static const char* hi[] = {
          "Heyyyy!", "Oh hiiii!", "Finally!", "YAY, hi!",
          "So excited!", "Hi hi hi!", "Woooo!"
        };
        pickPeerLine(hi, 7, out, outLen);
      } else {
        static const char* hi[] = {
          "Hey there!", "Oh hi!", "Hello!", "I see you!",
          "Hi neighbor!", "Good to see ya", "Hey yourself!"
        };
        pickPeerLine(hi, 7, out, outLen);
      }
    } else {
      // Generic mood-based reply
      if (currentMood == MOOD_PLAYFUL) {
        static const char* p[] = {
          "Sounds fun!", "Let's do it!", "You're funny!", "Haha, yes!",
          "I'm listening!", "Interesting...", "Tell me more!"
        };
        pickPeerLine(p, 7, out, outLen);
      } else if (currentMood == MOOD_SAD || currentMood == MOOD_BORED) {
        static const char* s[] = {
          "I hear you.", "Okay then...", "If you say so.", "Hmm, I see.",
          "Sure, whatever.", "Okay, fine.", "Noted."
        };
        pickPeerLine(s, 7, out, outLen);
      } else {
        static const char* g[] = {
          "Good to see you.", "Stay nearby!", "I like that.", "You're nice!",
          "Makes sense.", "Cool!", "Agreed!"
        };
        pickPeerLine(g, 7, out, outLen);
      }
    }
  }

private:
  Preferences* _prefs = nullptr;
  char  _apiKey[128]  = "";
  char  _endpoint[128]= LLM_ENDPOINT_DEFAULT;
  char  _model[48]    = LLM_MODEL_DEFAULT;
  bool  _llmEnabled   = false;
  bool  _forceOffline = false;
  unsigned long _lastLlmCall = 0;
  unsigned long _lastOfflineThought = 0;

  static const uint8_t RECENT_OFFLINE_LINES = 6;
  char _recentOffline[RECENT_OFFLINE_LINES][96] = {{0}};
  uint8_t _recentOfflinePos = 0;

  bool _hasApiKey() const {
    return strlen(_apiKey) > 0;
  }

  bool _isLikelyLocalEndpoint() const {
    String ep = String(_endpoint);
    ep.toLowerCase();
    return ep.startsWith("http://192.168.") ||
           ep.startsWith("http://10.") ||
           ep.startsWith("http://172.") ||
           ep.indexOf("localhost") >= 0 ||
           ep.indexOf(":11434") >= 0;
  }

  void _refreshLlmEnabled() {
    // Cloud endpoints usually require a key. Local LAN gateways (Ollama/LM Studio)
    // can run without API key, so endpoint itself can enable the feature.
    _llmEnabled = _hasApiKey() || _isLikelyLocalEndpoint();
  }

  bool _recentlyUsedOfflineLine(const char* line) {
    if (!line || !line[0]) return false;
    for (uint8_t i = 0; i < RECENT_OFFLINE_LINES; ++i) {
      if (_recentOffline[i][0] && strcmp(_recentOffline[i], line) == 0) return true;
    }
    return false;
  }

  void _rememberOfflineLine(const char* line) {
    if (!line || !line[0]) return;
    strlcpy(_recentOffline[_recentOfflinePos], line, sizeof(_recentOffline[_recentOfflinePos]));
    _recentOfflinePos = (_recentOfflinePos + 1) % RECENT_OFFLINE_LINES;
  }

  void _emitOfflineLine(const char* line) {
    _normalizeResponseText(line, lastResponse, sizeof(lastResponse));
    _rememberOfflineLine(lastResponse);
    responseReady = true;
  }

  void _normalizeResponseText(const char* input, char* out, size_t outLen) {
    String text = String(input ? input : "");
    if (!text.length()) {
      strlcpy(out, traits.name, outLen);
      return;
    }

    const String displayName = String(traits.name[0] ? traits.name : "Sablina");

    text.replace("Sablina", displayName);
    text.replace("sablina", displayName);
    text.replace("virtual pet", displayName);
    text.replace("Virtual pet", displayName);
    text.replace("little pet", displayName);
    text.replace("Little pet", displayName);
    text.replace("the pet", displayName);
    text.replace("The pet", displayName);
    text.replace("your pet", displayName);
    text.replace("Your pet", displayName);
    text.replace(" pet ", String(" ") + displayName + " ");
    text.replace(" pet.", String(" ") + displayName + ".");
    text.replace(" pet!", String(" ") + displayName + "!");
    text.replace(" pet?", String(" ") + displayName + "?");
    text.replace(" pet,", String(" ") + displayName + ",");

    text.trim();
    if (!text.length()) text = displayName;
    strlcpy(out, text.c_str(), outLen);
  }

  void _emitOfflineFromOptions(const char* const* options, uint8_t count) {
    if (!options || count == 0) return;
    uint8_t start = (uint8_t)random(0, count);
    for (uint8_t i = 0; i < count; ++i) {
      const char* candidate = options[(start + i) % count];
      if (!_recentlyUsedOfflineLine(candidate)) {
        _emitOfflineLine(candidate);
        return;
      }
    }
    _emitOfflineLine(options[start]);
  }

  void _offlineAutonomousThought(int hun, int fat, int cle, int exp) {
    const char* petState = _derivePetState(hun, fat, cle);

    if (strcmp(petState, "critical") == 0) {
      static const char* lines[] = {
        "I feel very weak... stay with me.",
        "Everything feels heavy... please help me.",
        "I need you right now..."
      };
      _emitOfflineFromOptions(lines, 3);
    } else if (strcmp(petState, "sick") == 0) {
      static const char* lines[] = {
        "I feel dirty and unwell...",
        "I don't feel good... can we clean up?",
        "Something feels off today..."
      };
      _emitOfflineFromOptions(lines, 3);
    } else if (strcmp(petState, "hungry") == 0) {
      static const char* lines[] = {
        "My tummy is empty... food please.",
        "I'm hungry... can we eat soon?",
        "I could really use a snack right now."
      };
      _emitOfflineFromOptions(lines, 3);
    } else if (strcmp(petState, "sleepy") == 0) {
      static const char* lines[] = {
        "My eyes are heavy... I need rest.",
        "I feel sleepy... let's slow down.",
        "Can we rest for a little while?"
      };
      _emitOfflineFromOptions(lines, 3);
    } else if (strcmp(petState, "happy") == 0) {
      if (traits.playfulness > 60) {
        static const char* lines[] = {
          "I feel amazing! Let's do something fun!",
          "Today feels exciting!",
          "I'm full of energy right now!"
        };
        _emitOfflineFromOptions(lines, 3);
      } else {
        static const char* lines[] = {
          "I feel calm and happy right now.",
          "Everything feels peaceful.",
          "I'm doing well, thank you."
        };
        _emitOfflineFromOptions(lines, 3);
      }
    } else {
      if (exp > 50) {
        static const char* lines[] = {
          "I am here, watching over our little world.",
          "I wonder what adventure we do next.",
          "We are doing great, keep it up!",
          "I'm okay and waiting for you."
        };
        _emitOfflineFromOptions(lines, 4);
      } else {
        static const char* lines[] = {
          "I am here, watching over our little world.",
          "I wonder what adventure we do next.",
          "Let's keep growing together.",
          "I'm okay and waiting for you."
        };
        _emitOfflineFromOptions(lines, 4);
      }
    }
  }

  const char* _derivePetState(int hun, int fat, int cle) {
    if (hun <= 5 && fat <= 5 && cle <= 5) return "critical";
    if (cle < 20) return "sick";
    if (hun < 20) return "hungry";
    if (fat < 20) return "sleepy";
    if (hun > 80 && fat > 80 && cle > 80) return "happy";
    return "normal";
  }

  // ── Mood logic (offline, always available) ──────────────────────
  PetMood _computeMood(int hun, int fat, int cle, int exp) {
    if (hun < 30) return MOOD_HUNGRY;
    if (fat < 30) return MOOD_TIRED;
    if (cle < 30) return MOOD_DIRTY;
    if (hun > 80 && fat > 80 && cle > 80) {
      return (traits.playfulness > 50) ? MOOD_PLAYFUL : MOOD_HAPPY;
    }
    return MOOD_HAPPY;
  }

  // ── Offline fallback responses ──────────────────────────────────
  void _offlineReact(const char* event, int hun, int fat, int cle) {
    (void)hun;
    (void)fat;
    (void)cle;

    if (strstr(event, "eat") || strstr(event, "food")) {
      if (traits.gluttony > 60) {
        static const char* lines[] = {
          "Yummy! More please!",
          "That was delicious!",
          "Best meal ever!"
        };
        _emitOfflineFromOptions(lines, 3);
      } else {
        static const char* lines[] = {
          "Thanks for the food.",
          "I feel better after eating.",
          "That hit the spot."
        };
        _emitOfflineFromOptions(lines, 3);
      }
    } else if (strstr(event, "sleep")) {
      static const char* lines[] = {
        "Zzz... so sleepy...",
        "Good night...",
        "I'll rest and recover."
      };
      _emitOfflineFromOptions(lines, 3);
    } else if (strstr(event, "clean")) {
      static const char* lines[] = {
        "Squeaky clean!",
        "I feel fresh now.",
        "Clean and comfy again."
      };
      _emitOfflineFromOptions(lines, 3);
    } else if (strstr(event, "play") || strstr(event, "game")) {
      if (traits.playfulness > 60) {
        static const char* lines[] = {
          "Yay! Let's play!",
          "That was fun!",
          "Again! Again!"
        };
        _emitOfflineFromOptions(lines, 3);
      } else {
        static const char* lines[] = {
          "Okay, I'll play for a bit.",
          "That was nice.",
          "I enjoyed that."
        };
        _emitOfflineFromOptions(lines, 3);
      }
    } else {
      static const char* lines[] = {
        "I'm listening.",
        "I noticed that.",
        "I'm curious about that."
      };
      _emitOfflineFromOptions(lines, 3);
    }
  }

  // ── WiFi LLM call (blocking, call from non-UI task or accept lag) ─
  void _callLLM(int hun, int fat, int cle, int exp, const char* event = nullptr) {
    const char* petState = _derivePetState(hun, fat, cle);
    int energy = constrain(100 - fat, 0, 100);

    // Build system prompt from personality traits
    char systemPrompt[320];
    snprintf(systemPrompt, sizeof(systemPrompt),
      "You are %s, a tamagotchi named %s. "
      "Personality: playfulness=%d/100, grumpiness=%d/100, sociability=%d/100. "
      "Never use the word pet. Use the name %s whenever you refer to yourself by name. "
      "Always match your current state and needs. "
      "Reply in 1 short sentence (max 15 words). Be in-character.",
      traits.name, traits.name, traits.playfulness, traits.grumpiness, traits.sociability, traits.name);

    // Build user message from pet state
    char userMsg[320];
    if (event) {
      snprintf(userMsg, sizeof(userMsg),
        "Event: %s. State:%s. Stats -> hunger:%d%% fatigue:%d%% cleanliness:%d%% energy:%d%%",
        event, petState, hun, fat, cle, energy);
    } else {
      snprintf(userMsg, sizeof(userMsg),
        "How do I feel right now? State:%s. hunger:%d%% fatigue:%d%% cleanliness:%d%% energy:%d%% coins:%d",
        petState, hun, fat, cle, energy, exp);
    }

    // Serialize request JSON
    JsonDocument req;
    req["model"]                          = _model;
    req["max_tokens"]                     = LLM_MAX_TOKENS;
    req["messages"][0]["role"]            = "system";
    req["messages"][0]["content"]         = systemPrompt;
    req["messages"][1]["role"]            = "user";
    req["messages"][1]["content"]         = userMsg;

    String body;
    serializeJson(req, body);

    // ── HTTPS request ──
    WiFiClientSecure client;
    client.setInsecure();  // skip cert verification – acceptable for hobby use
    client.setTimeout(LLM_TIMEOUT_MS / 1000);

    // Parse host from endpoint
    String ep = String(_endpoint);
    String host, path;
    bool useTLS = ep.startsWith("https://");
    ep = ep.substring(useTLS ? 8 : 7);
    int slash = ep.indexOf('/');
    if (slash < 0) { host = ep; path = "/v1/chat/completions"; }
    else            { host = ep.substring(0, slash); path = ep.substring(slash); }

    if (!client.connect(host.c_str(), useTLS ? 443 : 80)) return;

    // Build HTTP request
    String httpReq =
      String("POST ") + path + " HTTP/1.1\r\n" +
      "Host: " + host + "\r\n";
    if (_hasApiKey()) {
      httpReq += String("Authorization: Bearer ") + _apiKey + "\r\n";
    }
    httpReq +=
      String("Content-Type: application/json\r\n") +
      "Content-Length: " + body.length() + "\r\n" +
      "Connection: close\r\n\r\n" + body;

    client.print(httpReq);

    // Skip headers
    unsigned long t0 = millis();
    while (client.connected() && millis() - t0 < LLM_TIMEOUT_MS) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }

    // Read body
    String respBody = "";
    while (client.available() && millis() - t0 < LLM_TIMEOUT_MS) {
      respBody += (char)client.read();
    }
    client.stop();

    // Parse response
    JsonDocument resp;
    if (!deserializeJson(resp, respBody)) {
      const char* text = resp["choices"][0]["message"]["content"];
      if (text) {
        _normalizeResponseText(text, lastResponse, sizeof(lastResponse));
        responseReady = true;
      }
    }
  }

  // ── NVS helpers ─────────────────────────────────────────────────
  void _loadConfig() {
    if (!_prefs) return;
    strlcpy(_apiKey,   _prefs->getString(NVS_LLM_KEY,      "").c_str(), sizeof(_apiKey));
    strlcpy(_endpoint, _prefs->getString(NVS_LLM_ENDPOINT, LLM_ENDPOINT_DEFAULT).c_str(), sizeof(_endpoint));
    strlcpy(_model,    _prefs->getString(NVS_LLM_MODEL,    LLM_MODEL_DEFAULT).c_str(), sizeof(_model));
    _forceOffline = _prefs->getBool(NVS_FORCE_OFFLINE, false);
    _refreshLlmEnabled();
  }

  void _loadTraits() {
    if (!_prefs) return;
    String j = _prefs->getString(NVS_PERSONALITY, "");
    if (j.length()) setTraitsFromJson(j.c_str());
  }

  void _saveTraits() {
    if (!_prefs) return;
    char buf[256];
    traitsToJson(buf, sizeof(buf));
    _prefs->putString(NVS_PERSONALITY, buf);
  }
};
