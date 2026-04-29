// ═══════════════════════════════════════════════════════════════════
//  SABLINA TAMAGOTCHI 2.0  –  ESP32-S3  +  1.47" ST7789 172×320
//  New features: LLM personality  •  BLE app  •  IMU gestures  •  RGB LED
//               WiFi security audit (deauth / handshake / PMKID)
//  Required libraries:
//    TFT_eSPI (configure User_Setup.h – see config.h for pin table)
//    ArduinoJson   ≥ 7.x           (Sketch > Include Library)
//    ESP32 BLE (built in esp32 Arduino core)
//    Adafruit NeoPixel  (for RGB LED)
// ═══════════════════════════════════════════════════════════════════
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "imu_handler.h"
#include "llm_personality.h"
#include "ble_service.h"
#if FEATURE_WIFI_AUDIT
#include "wifi_audit.h"
#endif
#if FEATURE_TELEGRAM
#include "telegram_bot.h"
#endif
#include "pest.h"
#include "weber.h"
#include "baby.h"
#include "freg.h"
#include "pancho.h"
#include "sushi.h"
#include "shop.h"
#include "door.h"
#include "box.h"
#include "computer.h"
#include "watercooler.h"
#include "salida.h"
#include "warn.h"
#include "sablinagif.h"
#include "sablinaeat.h"
#include "sablinaeat2.h"
#include "sablinaeat3.h"
#include "sablinaeat4.h"
#include "sablinaeat5.h"
#include "sablinaeat6.h"
#include "sablinaeat7.h"
#include "sablinaeat8.h"
#include "sablinaeat9.h"
#include "sablinaeat10.h"
#include "sablinaeat11.h"
#include "roomred.h"
#include "roomblue.h"
#include "roomgray.h"
#include "roomwhite.h"
#include "roomgreen.h"
#include "roomblack.h"
#include "picture0.h"
#include "picture1.h"
#include "colorgif.h"
#include "f01.h"
#include "f02.h"
#include "f03.h"
#include "f04.h"
#include "f05.h"
#include "f06.h"
#include "f07.h"
#include "f08.h"
#include "f09.h"
#include "f10.h"
#include "f11.h"
#include "f12.h"
#include "f13.h"
#include "f14.h"
#include "f15.h"
#include "f16.h"
#include "f17.h"
#include "f18.h"
#include "f19.h"
#include "f20.h"
#include "f21.h"
#include "f22.h"

#include "forest1.h"
#include "forest2.h"

#include "cleangif.h"
#include "eatgif.h"
#include "gamegif.h"
#include "gardengif.h"
#include "sleepgif.h"
#include "sablinasleep.h"
#include "gamewalk.h"


TFT_eSPI tft = TFT_eSPI();

// ── Scaled-image / scaled-rect helpers ───────────────────────────
// Replicates the simulator's gameToScreenX/Y/W/H() scaling.
// All source coordinates are in the virtual 128×128 game space;
// output is nearest-neighbour scaled to the actual GAME_W×GAME_H area.

static uint16_t _pis_rowbuf[320];  // row buffer for pushImageScaled

// Dynamic game-canvas width: updated by drawSidebar() when sidebar hides/shows.
// Must be declared before pushImageScaled() / drawRectScaled() which reference it.
int16_t g_gameW = GAME_W;

void pushImageScaled(int16_t vx, int16_t vy, int16_t vw, int16_t vh, const uint16_t* img) {
  int16_t dx = GAME_X + (int32_t)vx * g_gameW / 128;
  int16_t dy = GAME_Y + (int32_t)vy * GAME_H / 128;
  int16_t dw = (int16_t)max((int32_t)1,
    (int32_t)(vx + vw) * g_gameW / 128 - (int32_t)vx * g_gameW / 128);
  int16_t dh = (int16_t)max((int32_t)1,
    (int32_t)(vy + vh) * GAME_H / 128 - (int32_t)vy * GAME_H / 128);
  tft.resetViewport();
  tft.startWrite();
  tft.setAddrWindow(dx, dy, dw, dh);
  for (int y = 0; y < dh; y++) {
    int sy = (int32_t)y * vh / dh;
    const uint16_t* row = img + (int32_t)sy * vw;
    for (int x = 0; x < dw; x++) {
      _pis_rowbuf[x] = row[(int32_t)x * vw / dw];
    }
    tft.pushColors(_pis_rowbuf, dw, true);
  }
  tft.endWrite();
  tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
}

void pushImageScaledTransp(int16_t vx, int16_t vy, int16_t vw, int16_t vh, const uint16_t* img, uint16_t transp) {
  int16_t dx = GAME_X + (int32_t)vx * g_gameW / 128;
  int16_t dy = GAME_Y + (int32_t)vy * GAME_H / 128;
  int16_t dw = (int16_t)max((int32_t)1,
    (int32_t)(vx + vw) * g_gameW / 128 - (int32_t)vx * g_gameW / 128);
  int16_t dh = (int16_t)max((int32_t)1,
    (int32_t)(vy + vh) * GAME_H / 128 - (int32_t)vy * GAME_H / 128);
  tft.resetViewport();
  tft.startWrite();
  for (int y = 0; y < dh; y++) {
    int sy = (int32_t)y * vh / dh;
    const uint16_t* row = img + (int32_t)sy * vw;
    for (int x = 0; x < dw; x++) {
      _pis_rowbuf[x] = row[(int32_t)x * vw / dw];
    }
    int x = 0;
    while (x < dw) {
      if (_pis_rowbuf[x] == transp) { x++; continue; }
      int runStart = x;
      int runLen = 0;
      while (x < dw && _pis_rowbuf[x] != transp) { runLen++; x++; }
      tft.setAddrWindow(dx + runStart, dy + y, runLen, 1);
      tft.pushColors(_pis_rowbuf + runStart, runLen, true);
    }
  }
  tft.endWrite();
  tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
}

void drawRectScaled(int16_t vx, int16_t vy, int16_t vw, int16_t vh, uint16_t color) {
  int16_t dx = GAME_X + (int32_t)vx * g_gameW / 128;
  int16_t dy = GAME_Y + (int32_t)vy * GAME_H / 128;
  int16_t dw = (int16_t)max((int32_t)1, (int32_t)vw * g_gameW / 128);
  int16_t dh = (int16_t)max((int32_t)1, (int32_t)vh * GAME_H / 128);
  tft.resetViewport();
  tft.drawRect(dx, dy, dw, dh, color);
  tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
}


Preferences       g_prefs;
LLMPersonality    g_llm;
SablinaBLE         g_ble;
IMUHandler        g_imu;
Adafruit_NeoPixel g_rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);
#if FEATURE_WIFI_AUDIT
WifiAudit         g_wifiAudit;
bool              g_auditScreenActive  = false;
uint8_t           g_auditSelectedAP    = 0;
unsigned long     g_lastAuditDraw      = 0;
#endif
#if FEATURE_TELEGRAM
TelegramBot       g_tg;
#endif

// ── New global state ─────────────────────────────────────────────
char          g_wifiSSID[64] = WIFI_SSID_DEFAULT;
char          g_wifiPass[64] = WIFI_PASS_DEFAULT;
bool          g_wifiChanged  = false;
char          g_platformUrl[128] = "";  // Canvas server base URL (set via BLE)
char          g_platformKey[64]  = "";  // Canvas API key (set via BLE)
unsigned long g_lastHeartbeatMs  = 0;   // last successful heartbeat timestamp
bool          g_wifiEnabled  = false;
unsigned long g_lastBleNotify       = 0;
unsigned long g_lastSidebarDraw     = 0;
unsigned long g_lastRgbUpdate       = 0;
uint32_t      g_rgbEventColor       = 0;  // temporary flash color (0 = none)
unsigned long g_rgbEventUntilMs     = 0;  // expiry timestamp for flash color
bool          g_rgbPulsePhase       = false;  // toggled for blinking effects
unsigned long g_iconsLastInteractMs = 0;  // for 15-second icon auto-hide
bool          g_iconsShown         = true;   // track icon visibility for transition detection
char          g_popupText[192]   = "";
char          g_popupPeerText[96]= "";
char          g_popupPeerName[24]= "";
unsigned long g_popupUntilMs     = 0;
unsigned long g_lastPeerChatMs   = 0;
bool          g_popupDualSpeaker = false;
bool          g_peerWasVisible   = false;
bool          g_peerNeedGreeting = false;  // persists through popup guard until greeting is shown
bool          g_bubbleDirty      = false;  // set after maingif() so drawFloatingMessage redraws once
char          g_lastPeerOfferText[BLE_PEER_MESSAGE_MAX_TEXT + 1] = "";
uint8_t       g_lastPeerOfferSeq = 0;
unsigned long g_lastPeerOfferUntilMs = 0;
uint8_t       g_pendingBeeps     = 0;
unsigned long g_lastBeepMs       = 0;
uint8_t       g_pendingVibes     = 0;
bool          g_vibeActive       = false;
unsigned long g_vibeToggleMs     = 0;
char          g_targetRoom[16]   = "LIVING";
unsigned long g_lastAutoRoomMs   = 0;
bool          g_navActive        = false;  // true while autonomous walk/action is in progress
bool          g_idleFirstFrame   = true;   // clear once when entering idle
bool          g_idleFrameDrawn   = false;  // set by maingif each new frame; bubble redraws on top

// ── WiFi passive scan (autonomous) ───────────────────────────────
unsigned long g_lastWifiScanMs   = 0;      // last time async WiFi scan was started
bool          g_wifiScanPending  = false;  // async scan in progress

// ── Stat-based autonomous navigation (independent of LLM) ────────
unsigned long g_lastStatNavMs    = 0;      // last stat-nav decision tick

// ── Life-cycle state ──────────────────────────────────────────────
// Stages: 0=BABY 1=CHILD 2=TEEN 3=ADULT 4=ELDER
const char* const STAGE_NAMES[] = { "BABY", "CHILD", "TEEN", "ADULT", "ELDER" };
uint8_t       g_petStage         = 0;      // current life stage
bool          g_petAlive         = true;   // false = game over
uint32_t      g_ageHours         = 0;      // cumulative hours alive
uint32_t      g_ageDays          = 0;      // cumulative days alive
unsigned long g_lastAgeTickMs    = 0;      // last hour tick
bool          g_petSick          = false;  // true = currently sick
unsigned long g_sickStartMs      = 0;      // when sickness began
unsigned long g_criticalStatMs   = 0;      // when a stat first hit critical
unsigned long g_lastLifecycleMs  = 0;      // lifecycle check interval

// ── Lifetime counters (achievements / badges) ─────────────────────
struct LifetimeStats {
  uint32_t foodEaten   = 0;
  uint32_t cleans      = 0;
  uint32_t sleeps      = 0;
  uint32_t pets        = 0;
  uint32_t gamesPlayed = 0;
  uint32_t gamesWon    = 0;
  uint32_t coinsEarned = 0;
  uint32_t coinsSpent  = 0;
  uint32_t wifiScans   = 0;
  uint32_t maxNets     = 0;
};
LifetimeStats g_lifetime;

// ── Trait Evolution ──────────────────────────────────────────────────
// curiosity  0-100 : rises when WiFi/BLE events happen, decays slowly
// activity   0-100 : rises when pet moves/plays, decays toward 50
// stress     0-100 : rises when stats are critical, decays when cared for
struct PetTraits {
  uint8_t curiosity = 50;
  uint8_t activity  = 50;
  uint8_t stress    = 20;
};
PetTraits g_traits;
unsigned long g_lastTraitTickMs = 0;  // last trait evolution tick (every 60 s)

// ── Day/Night cycle ───────────────────────────────────────────────
int8_t g_currentHour   = -1;   // 0-23; -1 = not synced yet
bool   g_isNight       = false; // true between NIGHT_HOUR_START and NIGHT_HOUR_END
bool   g_ntpSynced     = false; // true after first successful NTP sync
bool   g_blNightDimmed = false; // backlight was dimmed by night logic

// ── Dark mode ──────────────────────────────────────────────────────
// darkmode bool already declared below; g_darkModeActive mirrors it for
// the new auto-dim logic to avoid changing the existing menu code.
// True = reduce BL + use dimmed sidebar colours.
bool g_darkModeActive = false;  // set by night logic and existing dark-mode menu

// ── Trait evolution event flags (set by game events, read in tickTraitEvolution) ──
bool s_traitNavFlag  = false;  // pet navigated to a room this cycle → activity +
bool s_traitScanFlag = false;  // WiFi scan found networks → curiosity +

struct SocialPeerMemory {
  uint16_t senderId      = 0;
  uint16_t encounters    = 0;
  uint16_t chats         = 0;
  int16_t  affinity      = 0;
  uint16_t giftsGiven    = 0;
  uint16_t giftsReceived = 0;
  char     name[24]      = "";
  char     lastGift[12]  = "";
};

SocialPeerMemory g_socialPeers[MAX_SOCIAL_PEERS];
uint8_t          g_socialPeerCount = 0;

void connectWiFi();
void drawSidebar();
void handleBleCmdIfAny();
void updateRgbMood();
void triggerFloatingMessage(const char* text, unsigned long nowMs);
void triggerPeerConversation(const char* localText, const char* peerName, const char* peerText, unsigned long nowMs);
void triggerRgbEvent(uint8_t r, uint8_t g, uint8_t b, unsigned long durationMs);
void drawFloatingMessage(unsigned long nowMs);
void maybeBlePeerExchange(unsigned long nowMs);
void choosePeerOfferText(bool justDetected, char* out, size_t outLen);
void choosePeerReplyText(const char* peerText, char* out, size_t outLen);
void handleIncomingPeerMessage(const BLEPeerMessage& msg, unsigned long nowMs);
void loadSocialMemory();
void saveSocialMemory();
SocialPeerMemory* findSocialPeer(uint16_t senderId);
SocialPeerMemory* getOrCreateSocialPeer(uint16_t senderId, const char* peerName);
void notePeerEncounter(uint16_t senderId, const char* peerName, bool justDetected);
const char* socialBondLabel(const SocialPeerMemory* peer);
const char* socialGiftKindFromText(const char* text);
void registerGiftGiven(SocialPeerMemory* peer, const char* giftKind);
void registerGiftReceived(SocialPeerMemory* peer, const char* giftKind);
void applyGiftReward(const char* giftKind);
void trimChatLine(const char* src, char* dst, size_t dstLen, size_t maxChars);
void processSoundNotifications(unsigned long nowMs);
void processVibrationNotifications(unsigned long nowMs);
void sendPlatformHeartbeat(unsigned long nowMs,
                           const char* socialEvent = nullptr,
                           const char* socialMsg   = nullptr,
                           const char* targetAlias = nullptr);
void updateTargetRoomFromText(const char* text);
void autoNavigateToTargetRoom(unsigned long nowMs);
const char* decideNextTargetRoomFromNeeds();
void drawAutoRoom(const unsigned short* roomImage, int cursorX, int cursorY);
void showSleepStill(unsigned long holdMs);
void playGardenWalkSequence();
void playGardenExploreSequence();
void autoPerformKitchenAction();
void autoPerformBedroomAction();
void autoPerformBathroomAction();
void autoPerformPlayroomAction();
void tickTraitEvolution(unsigned long nowMs);
void tickDayNight(unsigned long nowMs);
void applyDarkMode(bool enable);
void runMiniGame(unsigned long nowMs);

// ── Virtual single-button state machine ─────────────────────────────────────
// Hardware reality: only GPIO0 (BOOT) is a real button.  RST = hardware EN.
//
// Timing:
//   Tap   (< 400 ms)  → readBtnB() fires on RELEASE   → navigate
//   Hold  (≥ 400 ms)  → readBtnA() fires WHILE HELD   → select / enter menu
//   Hold  (≥ 1800 ms) → checkBack() fires WHILE HELD  → back / cancel
//
// Long/back events fire as soon as the threshold is reached so there is
// no need to release the button – response is immediate.
// ────────────────────────────────────────────────────────────────────────────
static unsigned long g_btnPressMs      = 0;
static bool          g_btnHwDown       = false;
static bool          g_vBtnAFired      = false;
static bool          g_vBtnBFired      = false;
static bool          g_longPressFired  = false; // prevents short-press after long/back
bool                 g_backRequested   = false;

void updateVBtn() {
  bool hw = (digitalRead(0) == LOW);
  unsigned long now = millis();

  if (hw && !g_btnHwDown) {                    // ── falling edge: press ──
    g_btnPressMs         = now;
    g_btnHwDown          = true;
    g_longPressFired     = false;
    g_iconsLastInteractMs = now;

  } else if (hw && g_btnHwDown && !g_longPressFired) { // ── held, threshold check ──
    unsigned long held = now - g_btnPressMs;
    if (held >= 1800UL) {
      g_backRequested  = true;
      g_longPressFired = true;          // suppress further events this press
    } else if (held >= 400UL) {
      g_vBtnAFired     = true;
      g_longPressFired = true;
    }

  } else if (!hw && g_btnHwDown) {             // ── rising edge: release ──
    if (!g_longPressFired) {
      unsigned long held = now - g_btnPressMs;
      if (held >= 30UL) g_vBtnBFired = true;  // only a short press
    }
    g_btnHwDown = false;
  }
}
// readBtnA() – returns LOW exactly once per long-press event (≥400 ms).
int readBtnA() {
  updateVBtn();
  if (g_vBtnAFired) { g_vBtnAFired = false; return LOW; }
  return HIGH;
}
// readBtnB() – returns LOW exactly once per short-press event (< 400 ms).
int readBtnB() {
  updateVBtn();
  if (g_vBtnBFired) { g_vBtnBFired = false; return LOW; }
  return HIGH;
}
// checkBack() – returns true once per extra-long-press (≥1800 ms).
bool checkBack() {
  updateVBtn();
  if (g_backRequested) { g_backRequested = false; return true; }
  return false;
}
// ─────────────────────────────────────────────────────────────────────────────

int frame1 = 0;
int idleFrame = 0;
int buttonbefore47 = 1, buttonstate47 = 1;
int buttonbefore0 = 1, buttonstate0 = 1;
int mainx1 = 1, mainy1 = 0, mainx2 = 30, mainy2 = 30;
int mainnum = 0;
int foodx1 = 22, foody1 = 22, foodx2 = 30, foody2 = 30;
int sleepx1 = 5, sleepy1 = 13, sleepx2 = 76, sleepy2 = 21;
int timex1 = 5, timey1 = 18, timex2 = 76, timey2 = 21;
int shopx1 = 0, shopy1 = 0, shopx2 = 128, shopy2 = 100;
int modex1 = 5, modey1 = 23, modex2 = 76, modey2 = 21;
int gamex1 = 5, gamey1 = 23, gamex2 = 76, gamey2 = 21;
int n_main = 32;
int n_food = 52;
int food_page = 0;
int sleeptime = 0, sleep_page = 0;
float Hun1, Fat1, Cle1;
int Hun, Fat, Cle, Exp;
int Hun_mas = 0, Fat_mas = 0, Cle_mas = 0;
int Hun_s = 0, Fat_s = 0, Cle_s = 0; //开始吃之前判断需要补充多少饥饿值所用的变量
int Exp_mas = 0;
int roomwhitex = 83, roomwhitey = 16;
int picture = 0;
unsigned long long picture_s = 86400000;//目前为每24小时刷新一次商店图片
int buy_j = 0;
int picture_group[100];
int pic = 0;//picture_group数组序号
int boxx1 = 101, boxy1 = 99, boxx2 = 26, boxy2 = 18;
bool darkmode = 0;//主屏幕模式0为亮，1为暗；

// Backlight brightness table (higher = brighter)
int bright[] = {BRIGHT_LEVELS[0], BRIGHT_LEVELS[1], BRIGHT_LEVELS[2], BRIGHT_LEVELS[3], BRIGHT_LEVELS[4], BRIGHT_LEVELS[5]};
int b = BL_DEFAULT_IDX;

static inline void applyBacklightRaw(int level)
{
#if BL_FORCE_ALWAYS_ON
  (void)level;
  digitalWrite(TFT_BL_PIN, HIGH);
#if TFT_BL_PIN_ALT >= 0
  digitalWrite(TFT_BL_PIN_ALT, HIGH);
#endif
  return;
#endif

  int maxLevel = (1 << BL_PWM_RES) - 1;
  if (level < 0) level = 0;
  if (level > maxLevel) level = maxLevel;

  // This board variant needs a real PWM signal on GPIO46.
  ledcWrite(TFT_BL_PIN, level);

#if TFT_BL_PIN_ALT >= 0
  ledcWrite(TFT_BL_PIN_ALT, level);
#endif
}

int gamenum_left;
int gamenum_right;
int gamex = 9;
char buff[512];

//void wifi_scan()
//{
//  tft.setTextColor(TFT_WHITE, TFT_BLACK);
//  tft.fillScreen(TFT_BLACK);
//  tft.setTextDatum(MC_DATUM);
//  tft.setTextSize(1);
//
//  tft.drawString("Scan Network", tft.width() / 2, tft.height() / 2);
//  
//  WiFi.mode(WIFI_STA);
//  WiFi.disconnect();
//  delay(100);
//  int16_t wifi_n = WiFi.scanNetworks();//有一定延迟
//
//  tft.fillScreen(TFT_BLACK);
//  if (wifi_n == 0) 
//  {
//    tft.drawString("no networks found", tft.width() / 2, tft.height() / 2);
//  } 
//  else 
//  {
//    tft.setTextDatum(TL_DATUM);
//    tft.setCursor(0, 0);
//    Serial.printf("Found %d net\n", wifi_n);
//    for (int i = 0; i < wifi_n; ++i) 
//    {
//      sprintf(buff,
//              "[%d]:%s(%d)",
//              i + 1,
//              WiFi.SSID(i).c_str(),
//              WiFi.RSSI(i));
//      tft.println(buff);
//    }
//  }
//  //WiFi.mode( WIFI_OFF );
//  tft.setTextDatum(TL_DATUM);
//}

// Draw the 8 menu icons without clearing the background.
// Call this when only the icon rows need to be (re)drawn.
void drawIconsOnly() {
  if(Hun < 60 || Fat < 60 || Cle < 60)
    pushImageScaled(2, 1, 28, 28, warn);
  else
    pushImageScaled(2, 1, 28, 28, pest);
  pushImageScaled(34, 1, 29, 29, weber);
  pushImageScaled(66, 1, 29, 29, baby);
  pushImageScaled(98, 1, 29, 29, freg);
  pushImageScaled(2, 99, 29, 29, shop);
  pushImageScaled(34, 99, 29, 29, door);
  pushImageScaled(66, 99, 29, 29, box);
  pushImageScaled(98, 99, 29, 29, computer);
}

void mainicon()
{
  tft.fillScreen(TFT_BLACK);
  // Mark idle as needing a fresh one-time clear so the next maingif() frame
  // starts from a known-clean state (prevents residual pixels from any page).
  g_idleFirstFrame = true;
  // Auto-hide icons after 15 s of inactivity (mirrors simulator iconsVisible())
  if (millis() - g_iconsLastInteractMs >= 15000UL) return;
  drawIconsOnly();
}

void Generalmenu()
{
  if(readBtnA() == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    while(readBtnA() != 0 && readBtnB() != 0)
    {
      if (checkBack()) { return; }  // extra-long press = back
      tft.drawString("Hunger", 9, 9, 2);
      tft.drawString(String(Hun), 84, 9, 2);
      tft.drawString("%", 110, 9, 2);
    
      tft.drawString("Fatigue", 9, 39, 2);
      tft.drawString(String(Fat), 84, 39, 2);
      tft.drawString("%", 110, 39, 2);
    
      tft.drawString("Clean", 9, 69, 2);
      tft.drawString(String(Cle), 84, 69, 2);
      tft.drawString("%", 110, 69, 2);
    
      tft.drawString("Sablina coins", 9, 99, 2);
      tft.drawString(String(Exp), 94, 99, 2);

      // Trait evolution display
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString("Curiosity", 9, 122, 1);
      tft.drawString(String(g_traits.curiosity), 90, 122, 1);
      tft.drawString("Activity ", 9, 134, 1);
      tft.drawString(String(g_traits.activity),  90, 134, 1);
      tft.drawString("Stress   ", 9, 146, 1);
      tft.drawString(String(g_traits.stress),    90, 146, 1);
      // Night / dark-mode status
      if (g_isNight) {
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.drawString("[ Night Mode ]", 9, 158, 1);
      }
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    delay(300);
    mainicon();
  }
}

// ── Life-cycle helpers ────────────────────────────────────────────

static void persistLifecycle() {
  g_prefs.putUInt(NVS_PET_STAGE, g_petStage);
  g_prefs.putUChar(NVS_PET_ALIVE, g_petAlive ? 1 : 0);
  g_prefs.putUInt(NVS_PET_AGE_H, g_ageHours);
  g_prefs.putUInt(NVS_PET_AGE_D, g_ageDays);
  g_prefs.putUChar(NVS_PET_SICK, g_petSick ? 1 : 0);
}

static void persistLifetime() {
  g_prefs.putUInt(NVS_LT_FOOD,    g_lifetime.foodEaten);
  g_prefs.putUInt(NVS_LT_CLEANS,  g_lifetime.cleans);
  g_prefs.putUInt(NVS_LT_SLEEPS,  g_lifetime.sleeps);
  g_prefs.putUInt(NVS_LT_PETS,    g_lifetime.pets);
  g_prefs.putUInt(NVS_LT_GAMES,   g_lifetime.gamesPlayed);
  g_prefs.putUInt(NVS_LT_WINS,    g_lifetime.gamesWon);
  g_prefs.putUInt(NVS_LT_COINS_E, g_lifetime.coinsEarned);
  g_prefs.putUInt(NVS_LT_COINS_S, g_lifetime.coinsSpent);
  g_prefs.putUInt(NVS_LT_SCANS,   g_lifetime.wifiScans);
  g_prefs.putUInt(NVS_LT_NETS,    g_lifetime.maxNets);
  // Save traits alongside lifetime stats
  g_prefs.putUInt(NVS_TRAIT_CURIOSITY, g_traits.curiosity);
  g_prefs.putUInt(NVS_TRAIT_ACTIVITY,  g_traits.activity);
  g_prefs.putUInt(NVS_TRAIT_STRESS,    g_traits.stress);
}

static uint8_t computeStage(uint32_t ageH, int avgStat) {
  if (ageH >= 720 && avgStat > 40) return 4; // ELDER  (30 days)
  if (ageH >= 168 && avgStat > 45) return 3; // ADULT  (7 days)
  if (ageH >= 72  && avgStat > 35) return 2; // TEEN   (3 days)
  if (ageH >= 24  && avgStat > 25) return 1; // CHILD  (1 day)
  return 0;                                  // BABY
}

void updateLifecycle(unsigned long nowMs) {
  if (!g_petAlive) return;

  // ── Age tick (once per hour) ──────────────────────────────────
  if (nowMs - g_lastAgeTickMs >= 3600000UL) {
    g_lastAgeTickMs = nowMs;
    g_ageHours++;
    if (g_ageHours >= 24) { g_ageHours -= 24; g_ageDays++; }
  }

  // ── Life-stage evolution ──────────────────────────────────────
  int avg = (Hun + Fat + Cle) / 3;
  uint8_t newStage = computeStage(g_ageHours + (uint32_t)g_ageDays * 24, avg);
  if (newStage != g_petStage) {
    g_petStage = newStage;
    const char* name = g_llm.traits.name[0] ? g_llm.traits.name : "Sablina";
    char msg[100];
    snprintf(msg, sizeof(msg), "\xF0\x9F\x8C\x9F %s evolved into %s!", name, STAGE_NAMES[g_petStage]);
#if FEATURE_TELEGRAM
    g_tg.sendMessage(msg);
#endif
  }

  // ── Sickness check (triggered when Cle < 20 for > 5 min) ─────
  if (Cle < 20) {
    if (!g_petSick) {
      if (g_sickStartMs == 0) g_sickStartMs = nowMs;
      if (nowMs - g_sickStartMs > 300000UL) {  // 5 minutes
        g_petSick = true;
        triggerRgbEvent(255, 0, 0, 4000);  // red flash: pet is sick!
        const char* name = g_llm.traits.name[0] ? g_llm.traits.name : "Sablina";
        char msg[100];
        snprintf(msg, sizeof(msg), "\xF0\x9F\xA4\x92 %s is SICK! Give her medicine!", name);
#if FEATURE_TELEGRAM
        g_tg.sendMessage(msg);
#endif
      }
    }
  } else if (Cle >= 50) {
    g_sickStartMs = 0;
    g_petSick = false;
    triggerRgbEvent(0, 255, 120, 2000);  // green flash: healed!
  }

  // ── Death check (any critical stat <= 5 for > 5 min) ─────────
  bool critical = (Hun <= 5 || Fat <= 5 || Cle <= 5);
  if (critical) {
    if (g_criticalStatMs == 0) g_criticalStatMs = nowMs;
    if (nowMs - g_criticalStatMs > 300000UL) {  // 5 minutes
      g_petAlive = false;
      const char* name = g_llm.traits.name[0] ? g_llm.traits.name : "Sablina";
      char msg[100];
      snprintf(msg, sizeof(msg), "\xF0\x9F\x92\x80 %s has passed away after %lu days...", name, (unsigned long)g_ageDays);
#if FEATURE_TELEGRAM
      g_tg.sendMessage(msg);
#endif
    }
  } else {
    g_criticalStatMs = 0;
  }

  // Persist once per minute
  if (nowMs - g_lastLifecycleMs >= 60000UL) {
    g_lastLifecycleMs = nowMs;
    persistLifecycle();
  }
}

void state_count()
{
  Hun1 = (0.5- (millis() / 86400000.00)) * 100;
  Fat1 = (0.5- (millis() / 259200000.00)) * 100;
  Cle1 = (0.5- (millis() / 172800000.00)) * 100;
  Hun = round(Hun1) + Hun_mas;
  Fat = round(Fat1) + Fat_mas;
  Cle = round(Cle1) + Cle_mas;
  Exp = (millis() / 600000 - Exp_mas + 300) / 10;

  if(Fat > 100)
  {
    Fat = 100;
  }

  else if(Cle > 100)
  {
    Cle = 100;
  }
}

void Food()
{
  if(readBtnA() == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    food_page = 0;
    pushImageScaled(23, 23, 29, 29, f01);
    pushImageScaled(75, 23, 29, 29, f02);
    pushImageScaled(23, 75, 29, 29, f03);
    pushImageScaled(75, 75, 29, 29, f22);

    Hun_s = Hun; //把饥饿值Hun赋值给判断用的Hun_s
    tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
    while(!(readBtnA() == 0 && foodx1 == 74 && foody1 == 74))
    {
      if (checkBack()) { return; }  // extra-long press = back
      if(readBtnB() == 0 && foodx1 == 22 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && foodx1 == 74 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && foodx1 == 22 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && foodx1 == 74 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 -= n_food;
        if(food_page < 6)
        {
          food_page += 1;
          if(food_page == 1)
          {
            pushImageScaled(23, 23, 29, 29, f04);
            pushImageScaled(75, 23, 29, 29, f05);
            pushImageScaled(23, 75, 29, 29, f06);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 2)
          {
            pushImageScaled(23, 23, 29, 29, f07);
            pushImageScaled(75, 23, 29, 29, f08);
            pushImageScaled(23, 75, 29, 29, f09);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 3)
          {
            pushImageScaled(23, 23, 29, 29, f10);
            pushImageScaled(75, 23, 29, 29, f11);
            pushImageScaled(23, 75, 29, 29, f12);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 4)
          {
            pushImageScaled(23, 23, 29, 29, f13);
            pushImageScaled(75, 23, 29, 29, f14);
            pushImageScaled(23, 75, 29, 29, f15);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 5)
          {
            pushImageScaled(23, 23, 29, 29, f16);
            pushImageScaled(75, 23, 29, 29, f17);
            pushImageScaled(23, 75, 29, 29, f18);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 6)
          {
            pushImageScaled(23, 23, 29, 29, f19);
            pushImageScaled(75, 23, 29, 29, f20);
            pushImageScaled(23, 75, 29, 29, f21);
            pushImageScaled(75, 75, 29, 29, f22);
          }

        }
        else if(food_page >= 6)
        {
          food_page = 0;
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
        }
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
      
      
      //以下为点击确认键（0）时的反应 00
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f01);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f02);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 0)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f03);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 00

      //以下为点击确认键（0）时的反应 01
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f04);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f04);
          pushImageScaled(75, 23, 29, 29, f05);
          pushImageScaled(23, 75, 29, 29, f06);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f05);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f04);
          pushImageScaled(75, 23, 29, 29, f05);
          pushImageScaled(23, 75, 29, 29, f06);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 1)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f06);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f04);
          pushImageScaled(75, 23, 29, 29, f05);
          pushImageScaled(23, 75, 29, 29, f06);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 01

      //以下为点击确认键（0）时的反应 02
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f07);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f07);
          pushImageScaled(75, 23, 29, 29, f08);
          pushImageScaled(23, 75, 29, 29, f09);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f08);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f07);
          pushImageScaled(75, 23, 29, 29, f08);
          pushImageScaled(23, 75, 29, 29, f09);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 2)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f09);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f07);
          pushImageScaled(75, 23, 29, 29, f08);
          pushImageScaled(23, 75, 29, 29, f09);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 02

      //以下为点击确认键（0）时的反应 03
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f10);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f10);
          pushImageScaled(75, 23, 29, 29, f11);
          pushImageScaled(23, 75, 29, 29, f12);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f11);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f10);
          pushImageScaled(75, 23, 29, 29, f11);
          pushImageScaled(23, 75, 29, 29, f12);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 3)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f12);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f10);
          pushImageScaled(75, 23, 29, 29, f11);
          pushImageScaled(23, 75, 29, 29, f12);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 03

      //以下为点击确认键（0）时的反应 04
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f13);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f13);
          pushImageScaled(75, 23, 29, 29, f14);
          pushImageScaled(23, 75, 29, 29, f15);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f14);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f13);
          pushImageScaled(75, 23, 29, 29, f14);
          pushImageScaled(23, 75, 29, 29, f15);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 4)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f15);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f13);
          pushImageScaled(75, 23, 29, 29, f14);
          pushImageScaled(23, 75, 29, 29, f15);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 04

      //以下为点击确认键（0）时的反应 05
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f16);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f16);
          pushImageScaled(75, 23, 29, 29, f17);
          pushImageScaled(23, 75, 29, 29, f18);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f17);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f16);
          pushImageScaled(75, 23, 29, 29, f17);
          pushImageScaled(23, 75, 29, 29, f18);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 5)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f18);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f16);
          pushImageScaled(75, 23, 29, 29, f17);
          pushImageScaled(23, 75, 29, 29, f18);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 05

      //以下为点击确认键（0）时的反应 06
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f19);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f19);
          pushImageScaled(75, 23, 29, 29, f20);
          pushImageScaled(23, 75, 29, 29, f21);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f20);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f19);
          pushImageScaled(75, 23, 29, 29, f20);
          pushImageScaled(23, 75, 29, 29, f21);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 6)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f21);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f19);
          pushImageScaled(75, 23, 29, 29, f20);
          pushImageScaled(23, 75, 29, 29, f21);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 06
    }
    foodx1 = 22, foody1 = 22, foodx2 = 30, foody2 = 30;
    delay(300);
    mainicon();
  }
}

void maingif()
{
  // Only show idle home scene when the pet is at LIVING and NOT navigating
  if (g_navActive || strcmp(g_targetRoom, "LIVING") != 0) return;

  static unsigned long lastPetFrame = 0;
  unsigned long now = millis();

  if (now - lastPetFrame < 150) return;
  lastPetFrame = now;
  idleFrame = (idleFrame + 1) % 41;
  g_idleFrameDrawn = true;  // signal bubble to redraw on top this tick

  // One-time clear of the middle zone when entering idle
  if (g_idleFirstFrame) {
    g_idleFirstFrame = false;
    tft.resetViewport();
    int16_t midY = GAME_Y + (int32_t)30 * GAME_H / 128;
    int16_t midH = (int32_t)96 * GAME_H / 128 - (int32_t)30 * GAME_H / 128;
    tft.fillRect(GAME_X, midY, g_gameW, midH, TFT_BLACK);
    tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
  }

  if (g_iconsShown) {
    // Icons visible: clip the sprite draw to virtual y=[30,96) so we NEVER write
    // into the icon rows above (y<30) or below (y>=96). Icons stay pristine — no flicker,
    // and the sprite is naturally "behind" the buttons with no redraw needed.
    const int16_t vx = 14, vy = 14, vw = 100, vh = 100;
    int16_t dx = GAME_X + (int32_t)vx * g_gameW / 128;
    int16_t dy = GAME_Y + (int32_t)vy * GAME_H / 128;
    int16_t dw = (int16_t)max((int32_t)1,
      (int32_t)(vx + vw) * g_gameW / 128 - (int32_t)vx * g_gameW / 128);
    int16_t dh = (int16_t)max((int32_t)1,
      (int32_t)(vy + vh) * GAME_H / 128 - (int32_t)vy * GAME_H / 128);
    // Physical row range that falls inside the safe middle zone
    int16_t clipPY0 = GAME_Y + (int32_t)30 * GAME_H / 128;
    int16_t clipPY1 = GAME_Y + (int32_t)96 * GAME_H / 128;
    int16_t yStart  = max((int16_t)0,  (int16_t)(clipPY0 - dy));
    int16_t yEnd    = min(dh,           (int16_t)(clipPY1 - dy));
    if (yEnd > yStart) {
      tft.resetViewport();
      tft.startWrite();
      tft.setAddrWindow(dx, dy + yStart, dw, yEnd - yStart);
      for (int y = yStart; y < yEnd; y++) {
        int sy = (int32_t)y * vh / dh;
        const uint16_t* row = sablinagif[idleFrame] + (int32_t)sy * vw;
        for (int x = 0; x < dw; x++) {
          _pis_rowbuf[x] = row[(int32_t)x * vw / dw];
        }
        tft.pushColors(_pis_rowbuf, dw, true);
      }
      tft.endWrite();
      tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
    }
  } else {
    // Icons hidden: draw full sprite — no icon zones to protect
    pushImageScaled(14, 14, 100, 100, sablinagif[idleFrame]);
  }
}

void Sleep()
{
  if(readBtnA() == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    tft.drawString("5 minutes", 10, 15, 2);
    tft.drawString("20 minutes", 10, 40, 2);
    tft.drawString("1 hour", 10, 65, 2);
    tft.drawString("8 hours", 10, 90, 2);
    sleepx1 = 5, sleepy1 = 13, sleepx2 = 76, sleepy2 = 21;
    sleep_page = 0;
    tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
    
    while(!(sleeptime == 150 || sleeptime == 600 || sleeptime == 1800 || sleeptime == 14400))
    {
      if(readBtnB() == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Exit", 10, 15, 2);
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 = 13;
        sleep_page += 1;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }

      else if(readBtnB() == 0 && sleepy1 == 13 && sleep_page == 1)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("5 minutes", 10, 15, 2);
        tft.drawString("20 minutes", 10, 40, 2);
        tft.drawString("1 hour", 10, 65, 2);
        tft.drawString("8 hours", 10, 90, 2);
        sleepy1 = 13;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        sleep_page -= 1;
        delay(300);
      }

      else if(readBtnA() == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        showSleepStill(450);
        for(int i = 0; i < 150; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 2;
      }

      else if(readBtnA() == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        showSleepStill(450);
        for(int i = 0; i < 600; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 10;
      }

      else if(readBtnA() == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        showSleepStill(450);
        for(int i = 0; i < 1800; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 20;
      }

      else if(readBtnA() == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        showSleepStill(450);
        for(int i = 0; i < 14400; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 50;
      }

      else if(readBtnA() == 0 && sleepy1 == 13 && sleep_page == 1)
      {
        sleeptime = 600;//假装睡觉，实则退出
        delay(300);
      }
    }
    sleeptime = 0;
    mainicon();
  }
}

void Roomfood()
{
  if(readBtnA() == 0)
  {
    delay(300);
    frame1 = 0;
    pushImageScaled(0, 12, 128, 104, eatgif[frame1]);
    delay(300);
    for(int i = 0; i < 6; ++i)
    {
      pushImageScaled(0, 12, 128, 104, eatgif[frame1]);
      frame1++;
      delay(300);
    }
    tft.fillScreen(TFT_BLACK);
    food_page = 0;
    pushImageScaled(23, 23, 29, 29, f01);
    pushImageScaled(75, 23, 29, 29, f02);
    pushImageScaled(23, 75, 29, 29, f03);
    pushImageScaled(75, 75, 29, 29, f22);

    Hun_s = Hun; //把饥饿值Hun赋值给判断用的Hun_s
    tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
    while(!(readBtnA() == 0 && foodx1 == 74 && foody1 == 74))
    {
      if (checkBack()) { return; }  // extra-long press = back
      if(readBtnB() == 0 && foodx1 == 22 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && foodx1 == 74 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && foodx1 == 22 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && foodx1 == 74 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 -= n_food;
        if(food_page < 6)
        {
          food_page += 1;
          if(food_page == 1)
          {
            pushImageScaled(23, 23, 29, 29, f04);
            pushImageScaled(75, 23, 29, 29, f05);
            pushImageScaled(23, 75, 29, 29, f06);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 2)
          {
            pushImageScaled(23, 23, 29, 29, f07);
            pushImageScaled(75, 23, 29, 29, f08);
            pushImageScaled(23, 75, 29, 29, f09);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 3)
          {
            pushImageScaled(23, 23, 29, 29, f10);
            pushImageScaled(75, 23, 29, 29, f11);
            pushImageScaled(23, 75, 29, 29, f12);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 4)
          {
            pushImageScaled(23, 23, 29, 29, f13);
            pushImageScaled(75, 23, 29, 29, f14);
            pushImageScaled(23, 75, 29, 29, f15);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 5)
          {
            pushImageScaled(23, 23, 29, 29, f16);
            pushImageScaled(75, 23, 29, 29, f17);
            pushImageScaled(23, 75, 29, 29, f18);
            pushImageScaled(75, 75, 29, 29, f22);
          }

          else if(food_page == 6)
          {
            pushImageScaled(23, 23, 29, 29, f19);
            pushImageScaled(75, 23, 29, 29, f20);
            pushImageScaled(23, 75, 29, 29, f21);
            pushImageScaled(75, 75, 29, 29, f22);
          }

        }
        else if(food_page >= 6)
        {
          food_page = 0;
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
        }
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
      
      
      //以下为点击确认键（0）时的反应 00
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f01);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f02);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 0)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f03);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f01);
          pushImageScaled(75, 23, 29, 29, f02);
          pushImageScaled(23, 75, 29, 29, f03);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 00

      //以下为点击确认键（0）时的反应 01
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f04);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f04);
          pushImageScaled(75, 23, 29, 29, f05);
          pushImageScaled(23, 75, 29, 29, f06);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f05);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f04);
          pushImageScaled(75, 23, 29, 29, f05);
          pushImageScaled(23, 75, 29, 29, f06);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 1)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f06);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f04);
          pushImageScaled(75, 23, 29, 29, f05);
          pushImageScaled(23, 75, 29, 29, f06);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 01

      //以下为点击确认键（0）时的反应 02
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f07);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f07);
          pushImageScaled(75, 23, 29, 29, f08);
          pushImageScaled(23, 75, 29, 29, f09);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f08);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f07);
          pushImageScaled(75, 23, 29, 29, f08);
          pushImageScaled(23, 75, 29, 29, f09);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 2)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f09);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f07);
          pushImageScaled(75, 23, 29, 29, f08);
          pushImageScaled(23, 75, 29, 29, f09);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 02

      //以下为点击确认键（0）时的反应 03
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f10);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f10);
          pushImageScaled(75, 23, 29, 29, f11);
          pushImageScaled(23, 75, 29, 29, f12);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f11);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f10);
          pushImageScaled(75, 23, 29, 29, f11);
          pushImageScaled(23, 75, 29, 29, f12);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 3)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f12);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f10);
          pushImageScaled(75, 23, 29, 29, f11);
          pushImageScaled(23, 75, 29, 29, f12);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 03

      //以下为点击确认键（0）时的反应 04
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f13);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f13);
          pushImageScaled(75, 23, 29, 29, f14);
          pushImageScaled(23, 75, 29, 29, f15);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f14);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f13);
          pushImageScaled(75, 23, 29, 29, f14);
          pushImageScaled(23, 75, 29, 29, f15);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 4)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f15);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f13);
          pushImageScaled(75, 23, 29, 29, f14);
          pushImageScaled(23, 75, 29, 29, f15);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 04

      //以下为点击确认键（0）时的反应 05
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f16);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f16);
          pushImageScaled(75, 23, 29, 29, f17);
          pushImageScaled(23, 75, 29, 29, f18);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f17);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f16);
          pushImageScaled(75, 23, 29, 29, f17);
          pushImageScaled(23, 75, 29, 29, f18);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 5)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f18);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f16);
          pushImageScaled(75, 23, 29, 29, f17);
          pushImageScaled(23, 75, 29, 29, f18);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 05

      //以下为点击确认键（0）时的反应 06
      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f19);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f19);
          pushImageScaled(75, 23, 29, 29, f20);
          pushImageScaled(23, 75, 29, 29, f21);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +15%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 74 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f20);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f19);
          pushImageScaled(75, 23, 29, 29, f20);
          pushImageScaled(23, 75, 29, 29, f21);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +5%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }

      else if(readBtnA() == 0 && foodx1 == 22 && foody1 == 74 && food_page == 6)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          pushImageScaled(49, 49, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          pushImageScaled(49, 74, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          pushImageScaled(49, 92, 29, 29, f21);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          pushImageScaled(0, 0, 128, 88, sablinaeat);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat2);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat3);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat4);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat5);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          pushImageScaled(0, 0, 128, 88, sablinaeat7);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat8);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat9);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat10);
          delay(200);
          pushImageScaled(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          pushImageScaled(23, 23, 29, 29, f19);
          pushImageScaled(75, 23, 29, 29, f20);
          pushImageScaled(23, 75, 29, 29, f21);
          pushImageScaled(75, 75, 29, 29, f22);
          tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
          //以上为吃东西的动画
          
          tft.drawString("Hunger +2%", 26, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
        else
        {
          tft.drawString("Can't eat that much", 4, 4, 2);
          delay(1000);
          tft.fillRect(2, 2, 125, 18, TFT_BLACK);
        }
      }
      //以上为点击确认键（0）时的反应 06
    }
    foodx1 = 22, foody1 = 22, foodx2 = 30, foody2 = 30;
    delay(300);
    pushImageScaled(0, 12, 128, 104, roomred);
    roomwhitex = 9;
    roomwhitey = 93;
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
    delay(300);
  }
}

void Roomsleep()
{
  if(readBtnA() == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    tft.drawString("5 minutes", 10, 15, 2);
    tft.drawString("20 minutes", 10, 40, 2);
    tft.drawString("1 hour", 10, 65, 2);
    tft.drawString("8 hours", 10, 90, 2);
    sleepx1 = 5, sleepy1 = 13, sleepx2 = 76, sleepy2 = 21;
    sleep_page = 0;
    tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
    
    while(!(sleeptime == 150 || sleeptime == 600 || sleeptime == 1800 || sleeptime == 3))////////////把3改回14400
    {
      if(readBtnB() == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(readBtnB() == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Exit", 10, 15, 2);
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 = 13;
        sleep_page += 1;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }

      else if(readBtnB() == 0 && sleepy1 == 13 && sleep_page == 1)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("5 minutes", 10, 15, 2);
        tft.drawString("20 minutes", 10, 40, 2);
        tft.drawString("1 hour", 10, 65, 2);
        tft.drawString("8 hours", 10, 90, 2);
        sleepy1 = 13;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        sleep_page -= 1;
        delay(300);
      }

      else if(readBtnA() == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        frame1 = 0;
        pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
          frame1++;
          delay(300);
        }
        showSleepStill(450);
        for(int i = 0; i < 150; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 2;
      }

      else if(readBtnA() == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        frame1 = 0;
        pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
          frame1++;
          delay(300);
        }
        showSleepStill(450);
        for(int i = 0; i < 600; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 10;
      }

      else if(readBtnA() == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        frame1 = 0;
        pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
          frame1++;
          delay(300);
        }
        showSleepStill(450);
        for(int i = 0; i < 1800; ++i)
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 20;
      }

      else if(readBtnA() == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        frame1 = 0;
        pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
          frame1++;
          delay(300);
        }
        showSleepStill(450);
        for(int i = 0; i < 3; ++i)//////////////////////////////////这里改回14400
        {
          tft.fillScreen(TFT_BLACK);
          delay(500);
          tft.drawString("Z", 60, 60, 4);
          delay(500);
          tft.drawString("z", 75, 45, 2);
          delay(500);
          tft.drawString("z", 83, 37, 2);
          delay(500);
          sleeptime += 1;
        }
        Fat_mas += 50;
      }

      else if(readBtnA() == 0 && sleepy1 == 13 && sleep_page == 1)
      {
        sleeptime = 600;//假装睡觉，实则退出
        delay(300);
      }
    }
    sleeptime = 0;
    pushImageScaled(0, 12, 128, 104, roomgray);
    roomwhitex = 58;
    roomwhitey = 117;
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
    delay(300);
  }
}

void Roomshower()
{
  if(readBtnA() == 0)
  {
    delay(300);
    if(Cle <= 90)
    {
      frame1 = 0;
      pushImageScaled(0, 12, 128, 104, cleangif[frame1]);
      delay(300);
      for(int i = 0; i < 15; ++i)
      {
        pushImageScaled(0, 12, 128, 104, cleangif[frame1]);
        frame1++;
        delay(200);
      }
      pushImageScaled(0, 12, 128, 104, cleangif[14]);
      delay(350);
      for(int i = 0; i < 3; ++i)
      {
        tft.fillScreen(TFT_BLACK);
        delay(500);
        tft.drawString(".", 50, 60, 2);
        delay(500);
        tft.drawString(".", 60, 60, 2);
        delay(500);
        tft.drawString(".", 70, 60, 2);
        delay(500);
      }
      Cle_mas += 50;
    }
    else
    {
      tft.fillScreen(TFT_BLACK);
      tft.drawString("No need to shower", 7, 50, 2);
      delay(1000);
    }
    pushImageScaled(0, 12, 128, 104, roomblue);
    roomwhitex = 107;
    roomwhitey = 27;
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
    delay(300);
  }
}

void Shower()
{
  if(readBtnA() == 0)
  {
    tft.fillScreen(TFT_BLACK);
    delay(300);
    if(Cle <= 90)
    {
      for(int i = 0; i < 3; ++i)
      {
        tft.fillScreen(TFT_BLACK);
        delay(500);
        tft.drawString(".", 50, 60, 2);
        delay(500);
        tft.drawString(".", 60, 60, 2);
        delay(500);
        tft.drawString(".", 70, 60, 2);
        delay(500);
      }
      Cle_mas += 50;
    }
    else
    {
      tft.fillScreen(TFT_BLACK);
      tft.drawString("No need to shower", 7, 50, 2);
      delay(1000);
    }
    mainicon();
  }
}

void Shop()//图片尺寸为：x轴不超过126，y轴不超过98，出发点为1，1.即（1，1，126，98）
{
  if(readBtnA() == 0)
  {
    shopx1 = 101;
    shopy1 = 99;
    shopx2 = 26;
    shopy2 = 18;
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
    tft.drawString("Exit", 103, 100, 2);
    delay(300);
    while(!(shopx1 == 101 && shopy1 == 99 && readBtnA() == 0))
    {
      if (checkBack()) { return; }  // extra-long press = back
      if(shopx1 == 0 && shopy1 == 0 && shopx2 == 128 && shopy2 == 100 && readBtnB() == 0)
      {
        tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
        shopx1 = 101;
        shopy1 = 99;
        shopx2 = 26;
        shopy2 = 18;
        tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
        delay(200);
      }

      else if(shopx1 == 101 && shopy1 == 99 && shopx2 == 26 && shopy2 == 18 && readBtnB() == 0)
      {
        tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
        shopx1 = 0;
        shopy1 = 0;
        shopx2 = 128;
        shopy2 = 100;
        tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
        delay(200);
      }
      switch(picture)
      {
        case 0:
        {
          pushImageScaled(1, 1, 126, 83, colorgif[0]);
        }
        break;
    
        case 1:
        {
          pushImageScaled(1, 1, 126, 83, colorgif[1]);
        }
        break;
    
        case 2:
        {
          pushImageScaled(1, 1, 126, 83, colorgif[2]);
        }
        break;

        case 3:
        {
          pushImageScaled(1, 1, 126, 83, colorgif[3]);
        }
        break;

        case 4:
        {
          pushImageScaled(1, 1, 126, 83, colorgif[4]);
        }
        break;

        case 5:
        {
          pushImageScaled(1, 1, 126, 83, colorgif[5]);
        }
        break;
      }

      //以下为确认购买图片
      if(shopx1 == 0 && shopy1 == 0 && shopx2 == 128 && shopy2 == 100 && readBtnA() == 0)
      {
        if(Exp >= 10 && picture_group[pic] == 0)
        {
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Buy this picture", 15, 20, 2);
          tft.drawString("with", 55, 40, 2);
          tft.drawString("10 coins?", 32, 60, 2);
          tft.drawString("Yes", 25, 87, 2);
          tft.drawString("No", 93, 87, 2);
          shopx1 = 22;
          shopy1 = 86;
          shopx2 = 27;
          shopy2 = 18;
          tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
          delay(300);
          while(buy_j != 1)
          {
            if(shopx1 == 22 && shopy1 == 86 && readBtnB() == 0)
            {
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
              shopx1 = 90;
              shopy1 = 86;
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
              delay(300);
            }
    
            else if(shopx1 == 90 && shopy1 == 86 && readBtnB() == 0)
            {
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
              shopx1 = 22;
              shopy1 = 86;
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
              delay(300);
            }
  
            else if(shopx1 == 22 && shopy1 == 86 && readBtnA() == 0)//确认购买Yes
            {
              tft.fillScreen(TFT_BLACK);
              tft.drawString("Success", 40, 50, 2);
              Exp_mas += 100;
              buy_j += 1;
              picture_group[pic] += 1;
              delay(1000);
            }
  
            else if(shopx1 == 90 && shopy1 == 86 && readBtnA() == 0)//放弃购买No
            {
              tft.fillScreen(TFT_BLACK);
              buy_j += 1;
            }
          }
          buy_j = 0;
          delay(300);
          shopx1 = 101;
          shopy1 = 99;
          shopx2 = 26;
          shopy2 = 18;
          tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
          tft.drawString("Exit", 103, 100, 2);
        }

        else if(picture_group[pic] >= 1)
        {
          tft.fillScreen(TFT_BLACK);
          tft.drawString("You have bought", 11, 40, 2);
          tft.drawString("this picture", 28, 60, 2);
          delay(1000);
          shopx1 = 101;
          shopy1 = 99;
          shopx2 = 26;
          shopy2 = 18;
          tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
          tft.drawString("Exit", 103, 100, 2);
        }

        else if(Exp < 10)
        {
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Not enough coins", 10, 50, 2);
          delay(1000);
          shopx1 = 101;
          shopy1 = 99;
          shopx2 = 26;
          shopy2 = 18;
          tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
          tft.drawString("Exit", 103, 100, 2);
        }
      }
    }
    mainicon();
    delay(300);
  }
}

void Box()
{
  int i = 0;
  if(readBtnA() == 0)
  {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Welcome To The", 17, 40, 2);
    tft.drawString("Collection Box", 22, 60, 2);
    tft.drawString("Exit", 100, 100, 2);
    //tft.drawRect(boxx1, boxy1, boxx2, boxy2, TFT_WHITE);
    delay(300);
    while(readBtnA() != 0)
    {
      if (checkBack()) { return; }  // extra-long press = back
      if(readBtnB() == 0)
      {
        if(picture_group[i] == 1)
        {
          pushImageScaled(1, 1, 126, 83, colorgif[i]);
          tft.drawString(String(i + 1), 10, 100, 2);
        }
        else if(picture_group[i] != 1)
        {
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Did not buy", 28, 40, 2);
          tft.drawString("this picture", 28, 60, 2);
          tft.drawString(String(i + 1), 10, 100, 2);
          tft.drawString("Exit", 100, 100, 2);
          //tft.drawRect(boxx1, boxy1, boxx2, boxy2, TFT_WHITE);
        }
        ++i;
        delay(300);
      }
    }
    i = 0;
    mainicon();
    delay(300);
  }
}

void peace()
{
  if(Hun < 0 || Fat < 0 || Cle < 0)
  {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("GameOver", 3, 30, 4);
    delay(800);
    tft.drawString("Experience", 33, 65, 2);
    delay(800);
    tft.drawString(String(Exp), 33, 85, 2);
    while(1);
  }
}

void Rooms()
{
  if(readBtnA() == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    roomwhitex = 83, roomwhitey = 16;
    pushImageScaled(0, 12, 128, 104, roomwhite);
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);//上左
    while(!(roomwhitex == 95 && roomwhitey == 105 && readBtnA() == 0))
    {
      if (checkBack()) { return; }  // extra-long press = back
      if(readBtnB() == 0 && roomwhitex == 83)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =107 , roomwhitey = 28;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(readBtnB() == 0 && roomwhitex == 107)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =95 , roomwhitey = 105;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(readBtnB() == 0 && roomwhitex == 95)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =45 , roomwhitey = 110;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(readBtnB() == 0 && roomwhitex == 45)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =21 , roomwhitey = 98;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(readBtnB() == 0 && roomwhitex == 21)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =83 , roomwhitey = 16;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
  //    tft.drawCircle(107, 28, 2, TFT_WHITE);//上右
  //    tft.drawCircle(21, 99, 2, TFT_WHITE);//下左
  //    tft.drawCircle(45, 111, 2, TFT_WHITE);//下中
  //    tft.drawCircle(95, 105, 2, TFT_WHITE);//下右
      //以下为按下确认（0）时的反应
      else if(readBtnA() == 0 && roomwhitex == 83)
      {
        pushImageScaled(0, 12, 128, 104, roomgray);
        //tft.drawCircle(45, 16, 2, TFT_WHITE);
        roomwhitex = 58;
        roomwhitey = 117;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(readBtnA() == 0 && roomwhitex == 58 && roomwhitey == 117))
        {
      if (checkBack()) { return; }  // extra-long press = back
          tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
          if(readBtnB() == 0 && roomwhitex == 58 && roomwhitey == 117)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 45;
            roomwhitey = 16;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnB() == 0 && roomwhitex == 45 && roomwhitey == 16)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 100;
            roomwhitey = 102;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnB() == 0 && roomwhitex == 100 && roomwhitey == 102)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 58;
            roomwhitey = 117;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnA() == 0 && roomwhitex == 100 && roomwhitey == 102)
          {
            Roomsleep();
          }
          
          else if(readBtnA() == 0 && roomwhitex == 45 && roomwhitey == 16)//////////////////这里（游戏）
          {
            frame1 = 0;
            pushImageScaled(0, 12, 128, 104, gamegif[frame1]);
            delay(300);
            for(int i = 0; i < 7; ++i)
            {
              pushImageScaled(0, 12, 128, 104, gamegif[frame1]);
              frame1 += 1;
              delay(200);
            }
            
            //以下开始写电脑游戏
              tft.fillScreen(TFT_BLACK);
              tft.drawString("Sic bo", 10, 25, 2);
              //tft.drawString("WiFi scan", 10, 50, 2);//这个功能太占地方还发热，暂时不用
              tft.drawString("Exit", 10, 75, 2);
              gamex1 = 5, gamey1 = 23, gamex2 = 76, gamey2 = 21;
              tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);

              while(!(gamey1 == 73 && readBtnA() == 0))
              {
      if (checkBack()) { return; }  // extra-long press = back
                tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
                if(gamey1 == 23 && readBtnB() == 0)
                {
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_BLACK);
                  gamey1 = 73;
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
                  delay(300);
                }
            
//                else if(gamey1 == 48 && readBtnB() == 0)
//                {
//                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_BLACK);
//                  gamey1 += 25;
//                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
//                  delay(300);
//                }
            
                else if(gamey1 == 73 && readBtnB() == 0)
                {
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_BLACK);
                  gamey1 = 23;
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
                  delay(300);
                }

                //以下为确认游戏
                else if(gamey1 == 23 && readBtnA() == 0)//第一个游戏sic_bo
                {
                  tft.fillScreen(TFT_BLACK);
                  tft.drawString("Sic bo", 30, 30, 4);
                  int gamex = 9;
                  delay(1500);
                  while(!(gamex == 83 && readBtnA() == 0))
                  {
      if (checkBack()) { return; }  // extra-long press = back
                    tft.fillScreen(TFT_BLACK); 
                    gamenum_left = random(1, 7);
                    tft.drawString(String(gamenum_left), 33, 30, 4);
                    delay(400);
                    gamenum_left = random(1, 7);
                    tft.drawString(String(gamenum_left), 33, 30, 4);
                    delay(400);
                    gamenum_left = random(1, 7);
                    tft.drawString(String(gamenum_left), 33, 30, 4);
                    delay(400);
                    tft.drawString("?", 33, 30, 4);//左面数字结束
                    delay(700);
      
                    gamenum_right = random(1, 7);
                    tft.drawString(String(gamenum_right), 83, 30, 4);
                    delay(400);
                    gamenum_right = random(1, 7);
                    tft.drawString(String(gamenum_right), 83, 30, 4);
                    delay(400);
                    gamenum_right = random(1, 7);
                    tft.drawString(String(gamenum_right), 83, 30, 4);
                    delay(400);
                    tft.drawString("?", 83, 30, 4);//右面数字结束
                    delay(400);
      
                    gamenum_left = random(1, 7);
                    gamenum_right = random(1, 7);
      
                    while(readBtnA() != 0)
                    {
      if (checkBack()) { return; }  // extra-long press = back
                      tft.drawString("Big", 18, 90, 2);
                      tft.drawString("Small", 50, 90, 2);
                      tft.drawString("Exit", 90, 90, 2);
                      tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                      if(gamex == 9 && readBtnB() == 0)
                      {
                        tft.drawRect(gamex, 89, 37, 20, TFT_BLACK);
                        gamex += 37;
                        tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                        delay(300);
                      }
        
                      else if(gamex == 46 && readBtnB() == 0)
                      {
                        tft.drawRect(gamex, 89, 37, 20, TFT_BLACK);
                        gamex += 37;
                        tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                        delay(300);
                      }
      
                      else if(gamex == 83 && readBtnB() == 0)
                      {
                        tft.drawRect(gamex, 89, 37, 20, TFT_BLACK);
                        gamex = 9;
                        tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                        delay(300);
                      }
                    }
      
                    if(gamex == 9 && readBtnA() == 0)
                    {
                      tft.drawString(String(gamenum_left), 33, 30, 4);
                      tft.drawString(String(gamenum_right), 83, 30, 4);
                      delay(1200);
                      if(gamenum_left + gamenum_right >= 7)
                      {
                        tft.fillScreen(TFT_BLACK);
                        tft.drawString("Win", 40, 30, 4);
                        Exp_mas -= 20;
                        delay(500);
                        tft.drawString("Sablina coins +2", 20, 70, 2);
                        delay(1000);
                        tft.fillScreen(TFT_BLACK);
                      }
                      else
                      {
                        tft.fillScreen(TFT_BLACK);
                        tft.drawString("Lose", 35, 30, 4);
                        Exp_mas += 20;
                        delay(500);
                        tft.drawString("Sablina coins -2", 20, 70, 2);
                        delay(1000);
                        tft.fillScreen(TFT_BLACK);
                      }
                    }
      
                    else if(gamex == 46 && readBtnA() == 0)
                    {
                      tft.drawString(String(gamenum_left), 33, 30, 4);
                      tft.drawString(String(gamenum_right), 83, 30, 4);
                      delay(1200);
                      if(gamenum_left + gamenum_right <= 6)
                      {
                        tft.fillScreen(TFT_BLACK);
                        tft.drawString("Win", 40, 30, 4);
                        Exp_mas -= 20;
                        delay(500);
                        tft.drawString("Sablina coins +2", 20, 70, 2);
                        delay(1000);
                        tft.fillScreen(TFT_BLACK);
                      }
                      else
                      {
                        tft.fillScreen(TFT_BLACK);
                        tft.drawString("Lose", 35, 30, 4);
                        Exp_mas += 20;
                        delay(500);
                        tft.drawString("Sablina coins -2", 20, 70, 2);
                        delay(1000);
                        tft.fillScreen(TFT_BLACK);
                      }
                    }
                  }
                  delay(300);
                  tft.fillScreen(TFT_BLACK);
                  tft.drawString("Sic bo", 10, 25, 2);
                  //tft.drawString("WiFi scan", 10, 50, 2);
                  tft.drawString("Exit", 10, 75, 2);
                }

//                else if(gamey1 == 48 && readBtnA() == 0)//第二个游戏wifi_scan
//                {
//                  delay(300);
//                  int wifitime = 0;
//                  while(!(readBtnA() == 0 || readBtnB() == 0))//这个括号可以删掉
//                  {
//                    if(wifitime == 0)//让wifi_scan只执行一次
//                    {
//                      wifi_scan();
//                      ++wifitime;
//                    }
//                  }
//                  delay(300);
//                  tft.fillScreen(TFT_BLACK);
//                  tft.drawString("Sic bo", 10, 25, 2);
//                  tft.drawString("WiFi scan", 10, 50, 2);
//                  tft.drawString("Exit", 10, 75, 2);
//                }
              }
            pushImageScaled(0, 12, 128, 104, roomgray);
            delay(300);
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        pushImageScaled(0, 12, 128, 104, roomwhite);
        roomwhitex = 83;
        roomwhitey = 16;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(readBtnA() == 0 && roomwhitex == 107)
      {

//          roomwhitex = 96;
//          roomwhitey = 23;
//          roomwhitex = 9;
//          roomwhitey = 93;
        pushImageScaled(0, 12, 128, 104, roomred);
        roomwhitex = 9;
        roomwhitey = 93;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(readBtnA() == 0 && roomwhitex == 9 && roomwhitey == 93))
        {
      if (checkBack()) { return; }  // extra-long press = back
          if(readBtnB() == 0 && roomwhitex == 9 && roomwhitey == 93)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 96;
            roomwhitey = 23;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnB() == 0 && roomwhitex == 96 && roomwhitey == 23)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 9;
            roomwhitey = 93;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnA() == 0 && roomwhitex == 96 && roomwhitey == 23)
          {
            Roomfood();
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        pushImageScaled(0, 12, 128, 104, roomwhite);
        roomwhitex = 107;
        roomwhitey = 28;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);     
      }

      else if(readBtnA() == 0 && roomwhitex == 45)
      {
//          roomwhitex = 119;
//          roomwhitey = 34;
//          roomwhitex = 49;
//          roomwhitey = 114;
        pushImageScaled(0, 12, 128, 104, roomgreen);
        roomwhitex = 119;
        roomwhitey = 34;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(readBtnA() == 0 && roomwhitex == 119 && roomwhitey == 34))
        {
      if (checkBack()) { return; }  // extra-long press = back
          if(readBtnB() == 0 && roomwhitex == 119 && roomwhitey == 34)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 49;
            roomwhitey = 114;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnB() == 0 && roomwhitex == 49 && roomwhitey == 114)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 119;
            roomwhitey = 34;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnA() == 0 && roomwhitex == 49 && roomwhitey == 114)
          {
            delay(300);
            playGardenExploreSequence();
            while(readBtnA() != 0)
            {
      if (checkBack()) { return; }  // extra-long press = back
              tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
              pushImageScaled(0, 12, 128, 104, roomblack);
            }
            pushImageScaled(0, 12, 128, 104, roomgreen);
            roomwhitex = 49;
            roomwhitey = 114;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        pushImageScaled(0, 12, 128, 104, roomwhite);
        roomwhitex = 45;
        roomwhitey = 111;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      
      else if(readBtnA() == 0 && roomwhitex == 21)
      {
//          roomwhitex = 20;
//          roomwhitey = 27;
//          roomwhitex = 107;
//          roomwhitey = 27;
        pushImageScaled(0, 12, 128, 104, roomblue);
        roomwhitex = 107;
        roomwhitey = 27;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(readBtnA() == 0 && roomwhitex == 107 && roomwhitey == 27))
        {
      if (checkBack()) { return; }  // extra-long press = back
          if(readBtnB() == 0 && roomwhitex == 107 && roomwhitey == 27)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 20;
            roomwhitey = 27;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnB() == 0 && roomwhitex == 20 && roomwhitey == 27)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 107;
            roomwhitey = 27;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(readBtnA() == 0 && roomwhitex == 20 && roomwhitey == 27)
          {
            Roomshower();
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        pushImageScaled(0, 12, 128, 104, roomwhite);
        roomwhitex = 21;
        roomwhitey = 99;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);  
      }
    }
    mainicon();
    delay(300);
  }
}

void Time()
{
  if(readBtnA() == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    //tft.drawString("Sablina Tamagotchi", 13, 10);
    tft.drawString("Mode", 10, 25, 2);
    tft.drawString("Brightness", 10, 50, 2);
    tft.drawString("Exit", 10, 75, 2);
    timex1 = 5, timey1 = 23, timex2 = 76, timey2 = 21;
    tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);

    while(!(timey1 == 73 && readBtnA() == 0))
    {
      if (checkBack()) { return; }  // extra-long press = back
      tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
      if(timey1 == 23 && readBtnB() == 0)
      {
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_BLACK);
        timey1 += 25;
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
        delay(300);
      }
  
      else if(timey1 == 48 && readBtnB() == 0)
      {
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_BLACK);
        timey1 += 25;
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
        delay(300);
      }
  
      else if(timey1 == 73 && readBtnB() == 0)
      {
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_BLACK);
        timey1 = 23;
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
        delay(300);
      }
  
      else if(timey1 == 23 && readBtnA() == 0)//选择模式子菜单
      {
        delay(300);
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Light", 10, 25, 2);
        tft.drawString("Dark", 10, 50, 2);
        while(readBtnA() != 0)
        {
      if (checkBack()) { return; }  // extra-long press = back
          tft.drawRect(modex1, modey1, modex2, modey2, TFT_WHITE);
          if(readBtnB() == 0 && modey1 == 23)
          {
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_BLACK);
            modey1 += 25;
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_WHITE);
            delay(300);
          }
  
          else if(readBtnB() == 0 && modey1 == 48)
          {
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_BLACK);
            modey1 = 23;
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_WHITE);
            delay(300);
          }
  
          else if(readBtnA() == 0 && modey1 == 23)
          {
            applyDarkMode(false);
          }
  
          else if(readBtnA() == 0 && modey1 == 48)
          {
            applyDarkMode(true);
          }
        }
        delay(300);
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Mode", 10, 25, 2);
        tft.drawString("Brightness", 10, 50, 2);
        tft.drawString("Exit", 10, 75, 2);
      }
  
      else if(timey1 == 48 && readBtnA() == 0)//选择亮度子菜单
      {
        delay(300);
        tft.fillScreen(TFT_BLACK);
        while(readBtnA() != 0)
        {
      if (checkBack()) { return; }  // extra-long press = back
          tft.drawString("Sablina Tamagotchi", 13, 10);
          tft.drawString(String(b), 35, 40, 4);
          if(readBtnB() == 0)
          {
            b += 1;
            if(b == 6)
            {
              b = 0;
            }
#if !BL_FORCE_ALWAYS_ON
            applyBacklightRaw(bright[b]);
#endif
            tft.drawString(String(b), 35, 40, 4);
            delay(300);
          }
        }
        delay(300);
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Mode", 10, 25, 2);
        tft.drawString("Brightness", 10, 50, 2);
        tft.drawString("Exit", 10, 75, 2);
      }
    }
    delay(300);
    mainicon();
  }
}

// ═══════════════════════════════════════════════════════════════════
//  NEW SCREENS  (mirrors Sablina 2.0 Simulator)
// ═══════════════════════════════════════════════════════════════════

// ── Helper: draw a horizontal progress bar ───────────────────────
static void _drawBar(int x, int y, int w, int h, int val, uint16_t col) {
  int bw = (int)((long)w * constrain(val, 0, 100) / 100);
  if (bw > 0) tft.fillRect(x, y, bw, h, col);
  if (bw < w) tft.fillRect(x + bw, y, w - bw, h, 0x2104);
}

// ── Helper: archetype derived from personality traits ────────────
static const char* _archetypeStr() {
  int c = g_llm.traits.playfulness;   // curiosity proxy
  int a = g_llm.traits.sociability;   // activity proxy
  int s = g_llm.traits.grumpiness;    // stress proxy
  if (c > 70 && a > 70 && s < 40) return "GHOST HUNTER";
  if (s > 70 && a > 70)           return "CHAOS AGENT";
  if (c > 70 && a < 30)           return "SILENT OBSERVER";
  if (a > 70 && s < 30)           return "PACKET HUNTER";
  if (c < 30 && a < 30)           return "SLEEPY LURKER";
  if (c > 60 && a > 60 && s > 60) return "APEX PREDATOR";
  return "NET WANDERER";
}

// ── Helper: hack-point rank label ────────────────────────────────
static const char* _rankStr(uint32_t pts) {
  if (pts < 10)   return "NOOB";
  if (pts < 50)   return "WARDRIVER";
  if (pts < 200)  return "PKTSNIPER";
  if (pts < 1000) return "WIFI NINJA";
  return "ELITE HACKER";
}

// ── 0. Pet Status (Soul Card) ────────────────────────────────────
void PetStatus()
{
  if (readBtnA() != 0) return;
  delay(300);
  bool drawn = false;
  while (readBtnA() != 0) {
    if (checkBack()) { return; }
    if (drawn) continue;
    drawn = true;
    tft.fillScreen(TFT_BLACK);

    const char* name = g_llm.traits.name[0] ? g_llm.traits.name : "Sablina";
    uint32_t hs   = g_wifiAudit.getHandshakeCompleteCount();
    uint32_t pmk  = g_wifiAudit.pmkidCount;
    uint32_t dea  = g_wifiAudit.deauthsSent;
    uint32_t pts  = hs * 5 + pmk * 3 + dea + g_lifetime.wifiScans * 2 + g_lifetime.maxNets;
    const char* rank = _rankStr(pts);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("-- SOUL CARD --", 50, 3, 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  %s", name, STAGE_NAMES[g_petStage]);
    tft.drawString(buf, 4, 20, 2);

    tft.setTextColor(g_petAlive ? TFT_GREEN : TFT_RED, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Age:%lud%luh  %s", (unsigned long)g_ageDays, (unsigned long)g_ageHours,
             g_petAlive ? (g_petSick ? "SICK!" : "ALIVE") : "DEAD");
    tft.drawString(buf, 4, 36, 2);

    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Mood:%-8s Arch:%s", g_llm.moodName(), _archetypeStr());
    tft.drawString(buf, 4, 52, 2);

    // Stat bars
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("HUN", 4, 68, 2);
    _drawBar(34, 68, 90, 8, Hun, TFT_ORANGE);
    tft.drawString(String(Hun), 130, 68, 2);

    tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    tft.drawString("HAP", 4, 82, 2);
    _drawBar(34, 82, 90, 8, Fat, TFT_SKYBLUE);
    tft.drawString(String(Fat), 130, 82, 2);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("HP ", 4, 96, 2);
    _drawBar(34, 96, 90, 8, Cle, TFT_GREEN);
    tft.drawString(String(Cle), 130, 96, 2);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Coins:%-4d  Pts:%lu", Exp, (unsigned long)pts);
    tft.drawString(buf, 4, 112, 2);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "HS:%-3lu PK:%-3lu DEA:%-3lu", (unsigned long)hs, (unsigned long)pmk, (unsigned long)dea);
    tft.drawString(buf, 4, 128, 2);

    tft.setTextColor(0xF81F, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Rank: %s", rank);
    tft.drawString(buf, 4, 144, 2);
  }
  mainicon();
}

// ── 5. Achievements ──────────────────────────────────────────────
void Achievements()
{
  if (readBtnA() != 0) return;
  delay(300);
  int pg = 0;
  bool redraw = true;

  struct Badge { const char* name; const char* desc; uint32_t val; uint32_t goal; };
  uint32_t hs  = g_wifiAudit.getHandshakeCompleteCount();
  uint32_t pmk = g_wifiAudit.pmkidCount;
  uint32_t dea = g_wifiAudit.deauthsSent;
  Badge badges[] = {
    { "First Bite",    "Feed 1 time",      g_lifetime.foodEaten,   1   },
    { "Gourmet",       "Feed 50 times",    g_lifetime.foodEaten,   50  },
    { "Feast Master",  "Feed 200 times",   g_lifetime.foodEaten,   200 },
    { "Script Kiddie", "Capture 1 HS",     hs,                     1   },
    { "Pkt Hunter",    "Capture 25 HS",    hs,                     25  },
    { "PMKID Rookie",  "Grab 1 PMKID",     pmk,                    1   },
    { "PMKID Lord",    "Grab 50 PMKIDs",   pmk,                    50  },
    { "Deauth Storm",  "Send 10 deauths",  dea,                    10  },
    { "Net Scout",     "Scan 20 times",    g_lifetime.wifiScans,   20  },
    { "Shopaholic",    "Spend 100 coins",  g_lifetime.coinsSpent,  100 },
    { "Rich Pet",      "Earn 500 coins",   g_lifetime.coinsEarned, 500 },
    { "Survivor",      "Alive 7 days",     g_ageDays,              7   },
    { "Veteran",       "Alive 30 days",    g_ageDays,              30  },
    { "Clean Freak",   "Clean 30 times",   g_lifetime.cleans,      30  },
    { "Gamer",         "Play 20 games",    g_lifetime.gamesPlayed, 20  },
    { "Champion",      "Win 50 games",     g_lifetime.gamesWon,    50  },
    { "Net Whale",     "Find 50+ nets",    g_lifetime.maxNets,     50  },
    { "Cuddly",        "Pet 25 times",     g_lifetime.pets,        25  },
  };
  const int NUM_BADGES = 18;
  int unlocked = 0;
  for (int i = 0; i < NUM_BADGES; i++) if (badges[i].val >= badges[i].goal) unlocked++;

  while (readBtnA() != 0) {
    if (checkBack()) { return; }
    if (readBtnB() == 0) { pg = (pg + 1) % 4; redraw = true; delay(300); }
    if (!redraw) continue;
    redraw = false;
    tft.fillScreen(TFT_BLACK);

    if (pg < 3) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      char hdr[32]; snprintf(hdr, sizeof(hdr), "ACHIEVEMENTS %d/%d  Pg %d/3", unlocked, NUM_BADGES, pg + 1);
      tft.drawString(hdr, 2, 3, 2);
      int start = pg * 6;
      for (int i = 0; i < 6 && start + i < NUM_BADGES; i++) {
        const Badge& b = badges[start + i];
        bool done = b.val >= b.goal;
        int ly = 20 + i * 22;
        tft.setTextColor(done ? TFT_GREEN : 0x4208, TFT_BLACK);
        tft.drawString(done ? "*" : "o", 2, ly, 2);
        tft.setTextColor(done ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        tft.drawString(b.name, 14, ly, 2);
        tft.setTextColor(done ? TFT_GREEN : 0x6B6D, TFT_BLACK);
        char prog[24];
        if (done) snprintf(prog, sizeof(prog), "DONE");
        else      snprintf(prog, sizeof(prog), "%lu/%lu", (unsigned long)b.val, (unsigned long)b.goal);
        tft.drawString(prog, 14, ly + 10, 1);
        tft.setTextColor(0x5ACB, TFT_BLACK);
        tft.drawString(b.desc, 70, ly + 10, 1);
      }
    } else {
      // Hacker rank card
      uint32_t pts = hs * 5 + pmk * 3 + dea + g_lifetime.wifiScans * 2 + g_lifetime.maxNets;
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("☠ HACKER RANK", 2, 3, 2);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      char buf[48];
      snprintf(buf, sizeof(buf), "Rank: %s", _rankStr(pts));
      tft.drawString(buf, 2, 22, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Hack pts:  %lu", (unsigned long)pts);   tft.drawString(buf, 2, 40, 2);
      snprintf(buf, sizeof(buf), "Handshks:  %lu", (unsigned long)hs);    tft.drawString(buf, 2, 56, 2);
      snprintf(buf, sizeof(buf), "PMKIDs:    %lu", (unsigned long)pmk);   tft.drawString(buf, 2, 72, 2);
      snprintf(buf, sizeof(buf), "Deauths:   %lu", (unsigned long)dea);   tft.drawString(buf, 2, 88, 2);
      snprintf(buf, sizeof(buf), "WiFi scns: %lu", (unsigned long)g_lifetime.wifiScans); tft.drawString(buf, 2, 104, 2);
      snprintf(buf, sizeof(buf), "Max nets:  %lu", (unsigned long)g_lifetime.maxNets);   tft.drawString(buf, 2, 120, 2);
      tft.setTextColor(0x5ACB, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Badges: %d/%d", unlocked, NUM_BADGES); tft.drawString(buf, 2, 138, 2);
    }
    tft.setTextColor(0x5ACB, TFT_BLACK);
    tft.drawString("B=next pg  hold=back", 2, 155, 1);
  }
  mainicon();
}

// ── 6. Tools ─────────────────────────────────────────────────────
void Tools()
{
  if (readBtnA() != 0) return;
  delay(300);
  int pg = 0;
  bool redraw = true;

  while (readBtnA() != 0) {
    if (checkBack()) { return; }
    if (readBtnB() == 0) { pg = (pg + 1) % 5; redraw = true; delay(300); }
    if (!redraw) continue;
    redraw = false;
    tft.fillScreen(TFT_BLACK);

    char buf[64];
    if (pg == 0) {
      // WiFi Audit summary
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- WiFi Audit --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Mode: %s  CH:%d", g_wifiAudit.modeStr(), g_wifiAudit.currentChannel);
      tft.drawString(buf, 2, 22, 2);
      snprintf(buf, sizeof(buf), "Pkts:%-5lu MGMT:%-5lu", (unsigned long)g_wifiAudit.totalPackets, (unsigned long)g_wifiAudit.mgmtPackets);
      tft.drawString(buf, 2, 38, 2);
      snprintf(buf, sizeof(buf), "EAPOL:%-4lu Deauths:%-4lu", (unsigned long)g_wifiAudit.eapolPackets, (unsigned long)g_wifiAudit.deauthsSent);
      tft.drawString(buf, 2, 54, 2);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Handshakes: %u", g_wifiAudit.getHandshakeCompleteCount());
      tft.drawString(buf, 2, 72, 2);
      tft.setTextColor(0xC97A, TFT_BLACK);
      snprintf(buf, sizeof(buf), "PMKIDs:     %u", g_wifiAudit.pmkidCount);
      tft.drawString(buf, 2, 88, 2);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      snprintf(buf, sizeof(buf), "APs found:  %u", g_wifiAudit.apCount);
      tft.drawString(buf, 2, 104, 2);
    } else if (pg == 1) {
      // BLE Scan
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- BLE Scan --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "BLE App: %s", g_ble.deviceConnected ? "CONNECTED" : "advertising");
      tft.drawString(buf, 2, 22, 2);
      if (g_ble.peerVisible()) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        snprintf(buf, sizeof(buf), "Peer: %s", g_ble.peerName());
        tft.drawString(buf, 2, 40, 2);
        const SocialPeerMemory* pm = findSocialPeer(g_ble.peer.senderId);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        snprintf(buf, sizeof(buf), "Bond: %s", socialBondLabel(pm));
        tft.drawString(buf, 2, 58, 2);
        if (pm) {
          snprintf(buf, sizeof(buf), "Encounters:%u  Chats:%u", pm->encounters, pm->chats);
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.drawString(buf, 2, 76, 2);
          snprintf(buf, sizeof(buf), "Gifts given:%u  rcvd:%u", pm->giftsGiven, pm->giftsReceived);
          tft.drawString(buf, 2, 94, 2);
        }
      } else {
        tft.setTextColor(0x5ACB, TFT_BLACK);
        tft.drawString("No BLE peer nearby", 2, 40, 2);
      }
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "WiFi: %s", g_wifiEnabled ? "ON" : "OFF");
      tft.drawString(buf, 2, 120, 2);
    } else if (pg == 2) {
      // Signal Meter
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- Signal Meter --", 2, 3, 2);
      int rssi = -70;
      if (g_wifiAudit.apCount > 0) rssi = g_wifiAudit.aps[0].rssi;
      int strength = constrain(rssi + 100, 0, 100);
      uint16_t barcol = (strength > 60) ? TFT_GREEN : (strength > 30) ? TFT_ORANGE : TFT_RED;
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "RSSI: %d dBm  Str: %d%%", rssi, strength);
      tft.drawString(buf, 2, 22, 2);
      // Big signal bar
      _drawBar(2, 38, 220, 14, strength, barcol);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      snprintf(buf, sizeof(buf), "APs:%-3u  Scan: %s", g_wifiAudit.apCount,
               (g_wifiAudit.mode != AUDIT_IDLE) ? "ACTIVE" : "IDLE");
      tft.drawString(buf, 2, 60, 2);
      // Mini signal bars (10 columns)
      for (int bi = 0; bi < 10; bi++) {
        int bx = 2 + bi * 22;
        int bh = 4 + (bi + 1) * 10;
        bool active = bi < (strength / 10);
        tft.fillRect(bx, 130 - bh, 18, bh, active ? barcol : (uint16_t)0x2104);
      }
    } else if (pg == 3) {
      // AP List
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- AP List --", 2, 3, 2);
      if (g_wifiAudit.apCount == 0) {
        tft.setTextColor(0x5ACB, TFT_BLACK);
        tft.drawString("No APs found.", 2, 30, 2);
        tft.drawString("Use Telegram cmds to scan", 2, 50, 2);
      } else {
        int show = min((int)g_wifiAudit.apCount, 7);
        for (int i = 0; i < show; i++) {
          const WifiAuditAP& ap = g_wifiAudit.aps[i];
          int sig = constrain((int)ap.rssi + 100, 0, 100);
          int bars = (sig + 24) / 25;  // 0-4
          char barStr[5];
          for (int b = 0; b < 4; b++) barStr[b] = (b < bars) ? '#' : '.';
          barStr[4] = 0;
          bool hasCap = ap.handshakeCaptured || ap.pmkidCaptured;
          tft.setTextColor(hasCap ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
          snprintf(buf, sizeof(buf), "%-14s [%s]", ap.ssid[0] ? ap.ssid : "(hidden)", barStr);
          tft.drawString(buf, 2, 20 + i * 20, 2);
          tft.setTextColor(0x5ACB, TFT_BLACK);
          snprintf(buf, sizeof(buf), "  CH:%-2d %s%s", ap.channel,
                   g_wifiAudit.encryptionStr(ap.encryption),
                   hasCap ? " CAP!" : "");
          tft.drawString(buf, 2, 30 + i * 20, 1);
        }
      }
    } else {
      // Lifetime WiFi log
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- Lifetime Log --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "WiFi scans:  %lu", (unsigned long)g_lifetime.wifiScans);  tft.drawString(buf, 2, 22, 2);
      snprintf(buf, sizeof(buf), "Handshakes:  %u",  g_wifiAudit.getHandshakeCompleteCount()); tft.drawString(buf, 2, 40, 2);
      snprintf(buf, sizeof(buf), "PMKIDs:      %u",  g_wifiAudit.pmkidCount);                 tft.drawString(buf, 2, 58, 2);
      snprintf(buf, sizeof(buf), "Deauths:     %lu", (unsigned long)g_wifiAudit.deauthsSent); tft.drawString(buf, 2, 76, 2);
      snprintf(buf, sizeof(buf), "Max nets:    %lu", (unsigned long)g_lifetime.maxNets);       tft.drawString(buf, 2, 94, 2);
      snprintf(buf, sizeof(buf), "Food eaten:  %lu", (unsigned long)g_lifetime.foodEaten);     tft.drawString(buf, 2, 112, 2);
      snprintf(buf, sizeof(buf), "Cleans:      %lu", (unsigned long)g_lifetime.cleans);        tft.drawString(buf, 2, 130, 2);
    }
    tft.setTextColor(0x5ACB, TFT_BLACK);
    tft.drawString("B=next pg  hold=back", 2, 155, 1);
  }
  mainicon();
}

// ── 7. Stats ─────────────────────────────────────────────────────
void Stats()
{
  if (readBtnA() != 0) return;
  delay(300);
  int pg = 0;
  bool redraw = true;

  while (readBtnA() != 0) {
    if (checkBack()) { return; }
    if (readBtnB() == 0) { pg = (pg + 1) % 6; redraw = true; delay(300); }
    if (!redraw) continue;
    redraw = false;
    tft.fillScreen(TFT_BLACK);

    char buf[64];
    if (pg == 0) {
      // Pet vitals with bars
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      const char* name = g_llm.traits.name[0] ? g_llm.traits.name : "Sablina";
      snprintf(buf, sizeof(buf), "%s  %s  %s", name, STAGE_NAMES[g_petStage], g_llm.moodName());
      tft.drawString(buf, 2, 3, 2);
      tft.setTextColor(0x5ACB, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Age: %lud %luh  Coins: %d", (unsigned long)g_ageDays, (unsigned long)g_ageHours, Exp);
      tft.drawString(buf, 2, 20, 2);

      auto statsBar = [](int y, const char* lbl, int val, uint16_t col) {
        tft.setTextColor(0x8C71, TFT_BLACK);
        tft.drawString(lbl, 2, y, 2);
        _drawBar(36, y, 140, 9, val, (val < 30) ? (uint16_t)TFT_RED : col);
        char n[8]; snprintf(n, sizeof(n), "%d", val);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(n, 182, y, 2);
      };
      statsBar(38,  "HUN", Hun, TFT_ORANGE);
      statsBar(54,  "HAP", Fat, TFT_SKYBLUE);
      statsBar(70,  "HP ", Cle, TFT_GREEN);
      // Trait bars
      statsBar(90,  "PLY", g_llm.traits.playfulness, 0xC97A);
      statsBar(106, "SOC", g_llm.traits.sociability,  0xFF9A);
      statsBar(122, "GRM", g_llm.traits.grumpiness,   TFT_RED);

      tft.setTextColor(0x5ACB, TFT_BLACK);
      snprintf(buf, sizeof(buf), "WiFi:%s  BLE:%s",
               g_wifiEnabled ? "ON" : "OFF",
               g_ble.deviceConnected ? "APP" : "ADV");
      tft.drawString(buf, 2, 140, 2);
    } else if (pg == 1) {
      // Time & Age
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- Time & Age --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Age:   %lud %luh", (unsigned long)g_ageDays, (unsigned long)g_ageHours);
      tft.drawString(buf, 2, 22, 2);
      snprintf(buf, sizeof(buf), "Stage: %s", STAGE_NAMES[g_petStage]);
      tft.drawString(buf, 2, 40, 2);
      snprintf(buf, sizeof(buf), "Alive: %s", g_petAlive ? "YES" : "NO");
      tft.setTextColor(g_petAlive ? TFT_GREEN : TFT_RED, TFT_BLACK);
      tft.drawString(buf, 2, 58, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Sick:  %s", g_petSick ? "YES" : "NO");
      tft.setTextColor(g_petSick ? TFT_RED : TFT_GREEN, TFT_BLACK);
      tft.drawString(buf, 2, 76, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      unsigned long upSec = millis() / 1000;
      snprintf(buf, sizeof(buf), "Uptime: %02lu:%02lu:%02lu",
               upSec / 3600, (upSec % 3600) / 60, upSec % 60);
      tft.drawString(buf, 2, 96, 2);
    } else if (pg == 2) {
      // WiFi Environment
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- Environment --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "APs found: %u", g_wifiAudit.apCount);        tft.drawString(buf, 2, 22, 2);
      if (g_wifiAudit.apCount > 0) {
        int totalRSSI = 0;
        for (int i = 0; i < g_wifiAudit.apCount; i++) totalRSSI += g_wifiAudit.aps[i].rssi;
        int avgRSSI = totalRSSI / g_wifiAudit.apCount;
        snprintf(buf, sizeof(buf), "Avg RSSI:  %d dBm", avgRSSI);              tft.drawString(buf, 2, 40, 2);
        // Count open/WPA
        int openCount = 0, wpaCount = 0, hidCount = 0;
        for (int i = 0; i < g_wifiAudit.apCount; i++) {
          if (g_wifiAudit.aps[i].encryption == 0) openCount++;
          else wpaCount++;
          if (g_wifiAudit.aps[i].ssid[0] == 0) hidCount++;
        }
        snprintf(buf, sizeof(buf), "Open: %d  WPA: %d  Hidden: %d", openCount, wpaCount, hidCount);
        tft.drawString(buf, 2, 58, 2);
      }
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      snprintf(buf, sizeof(buf), "BLE peer: %s", g_ble.peerVisible() ? g_ble.peerName() : "none");
      tft.drawString(buf, 2, 80, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Scan mode: %s", g_wifiAudit.modeStr());
      tft.drawString(buf, 2, 98, 2);
    } else if (pg == 3) {
      // System
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- System --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Firmware: Sablina 2.0", 2, 22, 2);
      tft.drawString("MCU: ESP32-S3", 2, 40, 2);
      unsigned long upSec2 = millis() / 1000;
      snprintf(buf, sizeof(buf), "Uptime: %02lu:%02lu:%02lu",
               upSec2 / 3600, (upSec2 % 3600) / 60, upSec2 % 60);
      tft.drawString(buf, 2, 58, 2);
      snprintf(buf, sizeof(buf), "WiFi: %s  BLE: %s", g_wifiEnabled ? "ON" : "OFF",
               g_ble.deviceConnected ? "APP" : "ADV");
      tft.drawString(buf, 2, 76, 2);
      snprintf(buf, sizeof(buf), "LLM: %s", g_llm.isForceOffline() ? "OFFLINE" : "AUTO");
      tft.drawString(buf, 2, 94, 2);
      snprintf(buf, sizeof(buf), "Alive: %s  Stage: %s", g_petAlive ? "YES" : "NO",
               STAGE_NAMES[g_petStage]);
      tft.drawString(buf, 2, 112, 2);
    } else if (pg == 4) {
      // Lifetime counters
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("-- Lifetime --", 2, 3, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Food eaten:   %lu", (unsigned long)g_lifetime.foodEaten);     tft.drawString(buf, 2, 22, 2);
      snprintf(buf, sizeof(buf), "Cleans:       %lu", (unsigned long)g_lifetime.cleans);        tft.drawString(buf, 2, 38, 2);
      snprintf(buf, sizeof(buf), "Sleeps:       %lu", (unsigned long)g_lifetime.sleeps);        tft.drawString(buf, 2, 54, 2);
      snprintf(buf, sizeof(buf), "Games played: %lu", (unsigned long)g_lifetime.gamesPlayed);   tft.drawString(buf, 2, 70, 2);
      snprintf(buf, sizeof(buf), "Games won:    %lu", (unsigned long)g_lifetime.gamesWon);      tft.drawString(buf, 2, 86, 2);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Coins earned: %lu", (unsigned long)g_lifetime.coinsEarned);   tft.drawString(buf, 2, 102, 2);
      snprintf(buf, sizeof(buf), "Coins spent:  %lu", (unsigned long)g_lifetime.coinsSpent);    tft.drawString(buf, 2, 118, 2);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      snprintf(buf, sizeof(buf), "WiFi scans:   %lu", (unsigned long)g_lifetime.wifiScans);     tft.drawString(buf, 2, 134, 2);
    } else {
      // pg == 5: Achievements / Badges
      uint32_t hs  = g_wifiAudit.getHandshakeCompleteCount();
      uint32_t pmk = g_wifiAudit.pmkidCount;
      uint32_t dea = g_wifiAudit.deauthsSent;
      uint32_t pts = hs * 5 + pmk * 3 + dea + g_lifetime.wifiScans * 2 + g_lifetime.maxNets;
      // 10 badges stored as parallel arrays (const-sized, ROM-friendly)
      static const char* bname[] = {
        "1stBite","Gourmet","PktHntr","DeAuStm",
        "NetSct", "Survivor","Veteran","Champion",
        "Shopaholic","RichPet"
      };
      uint32_t bval[10] = {
        g_lifetime.foodEaten, g_lifetime.foodEaten,
        hs, dea,
        g_lifetime.wifiScans, g_ageDays, g_ageDays,
        g_lifetime.gamesWon,
        g_lifetime.coinsSpent, g_lifetime.coinsEarned
      };
      static const uint32_t bgoal[10] = { 1,50, 25,10, 20,7,30, 50, 100,500 };
      int unlocked = 0;
      for (int i = 0; i < 10; i++) if (bval[i] >= bgoal[i]) unlocked++;

      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      snprintf(buf, sizeof(buf), "BADGES %d/10  %s", unlocked, _rankStr(pts));
      tft.drawString(buf, 2, 3, 2);
      // 2-column grid, 5 rows
      for (int i = 0; i < 10; i++) {
        bool done = bval[i] >= bgoal[i];
        int bx = 2 + (i % 2) * 114;
        int by = 22 + (i / 2) * 26;
        tft.setTextColor(done ? TFT_GREEN : 0x7BEF, TFT_BLACK);
        snprintf(buf, sizeof(buf), "%s%s", done ? "*" : "o", bname[i]);
        tft.drawString(buf, bx, by, 2);
        if (!done) {
          tft.setTextColor(0x5ACB, TFT_BLACK);
          snprintf(buf, sizeof(buf), "%lu/%lu", (unsigned long)bval[i], (unsigned long)bgoal[i]);
          tft.drawString(buf, bx, by + 11, 1);
        }
      }
    }
    tft.setTextColor(0x5ACB, TFT_BLACK);
    tft.drawString("B=next pg  hold=back", 2, 155, 1);
  }
  mainicon();
}

void mainselect()
{
  if (g_iconsShown) drawRectScaled(mainx1, mainy1, mainx2, mainy2, TFT_WHITE); 
  buttonstate47 = readBtnB();
  if(buttonstate47 == LOW && buttonbefore47 == HIGH)
  {
    if (g_iconsShown) drawRectScaled(mainx1, mainy1, mainx2, mainy2, TFT_BLACK); 
    mainx1 += n_main;
    mainnum += 1;
    if(mainx1 == 129 && mainy1 == 0)
    {
      mainx1 = 1; //mainx1有1，33，65，97四种。
      mainy1 = 98;
    }

    else if(mainx1 == 129 && mainy1 == 98)
    {
      mainx1 = 1;
      mainy1 = 0;
      mainnum = 0;
    }
  }
  buttonbefore47 = buttonstate47;
  if (g_iconsShown) drawRectScaled(mainx1, mainy1, mainx2, mainy2, TFT_WHITE);

  switch(mainnum)
  {
    case 0:
    {
      PetStatus();
    }
    break;

    case 1:
    {
      int _hun_before = Hun_mas;
      Food();
      if (Hun_mas > _hun_before) { g_lifetime.foodEaten++; persistLifetime(); }
    }
    break;

    case 2:
    {
      int _fat_before = Fat_mas;
      Sleep();
      if (Fat_mas > _fat_before) { g_lifetime.sleeps++; persistLifetime(); }
    }
    break;

    case 3:
    {
      int _cle_before = Cle_mas;
      Shower();
      if (Cle_mas > _cle_before) { g_lifetime.cleans++; persistLifetime(); }
    }
    break;

    case 4:
    {
      Shop();
    }
    break;

    case 5:
    {
      Rooms();
    }
    break;

    case 6:
    {
      Tools();
    }
    break;

    case 7:
    {
      Stats();
    }
    break;
  }
}


void setup() 
{
  Serial.begin(115200);  // USB CDC — required for Serial.printf to work on ESP32-S3

  // ── Buttons ────────────────────────────────────────────────────
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);

#if FEATURE_VIBRATION
  if (VIBRO_PIN >= 0) {
    pinMode(VIBRO_PIN, OUTPUT);
    digitalWrite(VIBRO_PIN, LOW);
  }
#endif

  // ── NVS (persistent storage) ───────────────────────────────────
  g_prefs.begin(NVS_NS, false);
  strlcpy(g_wifiSSID, g_prefs.getString(NVS_WIFI_SSID, WIFI_SSID_DEFAULT).c_str(), sizeof(g_wifiSSID));
  strlcpy(g_wifiPass, g_prefs.getString(NVS_WIFI_PASS, WIFI_PASS_DEFAULT).c_str(), sizeof(g_wifiPass));
  strlcpy(g_platformUrl, g_prefs.getString(NVS_PLATFORM_URL, "").c_str(), sizeof(g_platformUrl));
  strlcpy(g_platformKey, g_prefs.getString(NVS_PLATFORM_KEY, "").c_str(), sizeof(g_platformKey));
  // Restore stat offsets
  Hun_mas = g_prefs.getInt(NVS_HUN_MAS, 0);
  Fat_mas = g_prefs.getInt(NVS_FAT_MAS, 0);
  Cle_mas = g_prefs.getInt(NVS_CLE_MAS, 0);
  Exp_mas = g_prefs.getInt(NVS_EXP_MAS, 0);
  // Restore life-cycle state
  g_petStage = (uint8_t)g_prefs.getUInt(NVS_PET_STAGE, 0);
  if (g_petStage > 4) g_petStage = 0;
  g_petAlive = (bool)g_prefs.getUChar(NVS_PET_ALIVE, 1);
  g_ageHours = g_prefs.getUInt(NVS_PET_AGE_H, 0);
  g_ageDays  = g_prefs.getUInt(NVS_PET_AGE_D, 0);
  g_petSick  = (bool)g_prefs.getUChar(NVS_PET_SICK, 0);
  // Restore lifetime counters
  g_lifetime.foodEaten   = g_prefs.getUInt(NVS_LT_FOOD,    0);
  g_lifetime.cleans      = g_prefs.getUInt(NVS_LT_CLEANS,  0);
  g_lifetime.sleeps      = g_prefs.getUInt(NVS_LT_SLEEPS,  0);
  g_lifetime.pets        = g_prefs.getUInt(NVS_LT_PETS,    0);
  g_lifetime.gamesPlayed = g_prefs.getUInt(NVS_LT_GAMES,   0);
  g_lifetime.gamesWon    = g_prefs.getUInt(NVS_LT_WINS,    0);
  g_lifetime.coinsEarned = g_prefs.getUInt(NVS_LT_COINS_E, 0);
  g_lifetime.coinsSpent  = g_prefs.getUInt(NVS_LT_COINS_S, 0);
  g_lifetime.wifiScans   = g_prefs.getUInt(NVS_LT_SCANS,   0);
  g_lifetime.maxNets     = g_prefs.getUInt(NVS_LT_NETS,    0);
  // Restore trait evolution
  g_traits.curiosity = (uint8_t)g_prefs.getUInt(NVS_TRAIT_CURIOSITY, 50);
  g_traits.activity  = (uint8_t)g_prefs.getUInt(NVS_TRAIT_ACTIVITY,  50);
  g_traits.stress    = (uint8_t)g_prefs.getUInt(NVS_TRAIT_STRESS,    20);
  g_lastAgeTickMs        = millis();
  g_iconsLastInteractMs  = millis();  // start with icons visible
  loadSocialMemory();

  // ── Display ────────────────────────────────────────────────────
  tft.begin();
  tft.setRotation(1);              // landscape: 320 x 172
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  // Centre the legacy 128×128 game canvas in the display
  tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
  // This board variant responds on GPIO46 with PWM, not a forced HIGH level.
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, LOW);
#if TFT_BL_PIN_ALT >= 0
  pinMode(TFT_BL_PIN_ALT, OUTPUT);
  digitalWrite(TFT_BL_PIN_ALT, LOW);
#endif
  delay(10);
#if !BL_FORCE_ALWAYS_ON
  // Backlight via new ledcAttach API (ESP32-S3 core ≥ 3.x)
  ledcAttach(TFT_BL_PIN, BL_PWM_FREQ, BL_PWM_RES);
#if TFT_BL_PIN_ALT >= 0
  ledcAttach(TFT_BL_PIN_ALT, BL_PWM_FREQ, BL_PWM_RES);
#endif
  applyBacklightRaw(bright[b]);
#endif

  // ── RGB LED ────────────────────────────────────────────────────
  g_rgb.begin();
  g_rgb.setBrightness(60);
  g_rgb.setPixelColor(0, g_rgb.Color(0, 255, 80));  // green = starting up
  g_rgb.show();

  // ── IMU ────────────────────────────────────────────────────────
  g_imu.begin();

  // ── LLM personality engine ─────────────────────────────────────
  g_llm.begin(&g_prefs);

  // ── BLE ────────────────────────────────────────────────────────
  g_ble.begin(g_llm.traits.name);
  // Set initial personality char value
  if (g_ble.pPersonalityChar) {
    char pbuf[256];
    g_llm.traitsToJson(pbuf, sizeof(pbuf));
    g_ble.pPersonalityChar->setValue(pbuf);
  }

  // ── WiFi (background attempt) ──────────────────────────────────
  if (strlen(g_wifiSSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_wifiSSID, g_wifiPass);
    // Non-blocking: just fire off the connect request; loop() checks status
    // Also request NTP config now; actual sync happens once connected.
    configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
  }

  // ── Game init ──────────────────────────────────────────────────
  mainicon();
  for (int i = 0; i < 100; ++i) picture_group[i] = 0;

#if FEATURE_WIFI_AUDIT
  g_wifiAudit.begin();
  g_wifiAudit.registerInstance();
#endif

#if FEATURE_TELEGRAM
  g_tg.begin(&g_prefs, &g_llm.traits);
#if FEATURE_WIFI_AUDIT
  g_tg.wifiAudit = &g_wifiAudit;
#endif
#endif
}


void loop()
{
  unsigned long now = millis();

  // ── WiFi reconnect on config change ──────────────────────────────
  if (g_wifiChanged) {
    g_wifiChanged = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_wifiSSID, g_wifiPass);
  }
  g_wifiEnabled = (WiFi.status() == WL_CONNECTED);

  // ── BLE housekeeping + handle incoming commands ───────────────────
  g_ble.tick();
  handleBleCmdIfAny();

  // ── IMU shake → simulate BTN_B press ─────────────────────────────
  // (isShaking uses debounce internally; safe to call every loop)
  if (g_imu.available && g_imu.isShaking()) {
    // Shake = same as pressing the scroll button briefly
    // We simulate by temporarily forcing the pin state via a flag
    // Game code reads readBtnB(); hardware pins take priority
    // so we cannot inject digitally – instead trigger LLM reaction
    g_llm.reactToEvent("shaken", Hun, Fat, Cle);
  }

  // ── Button state machine tick (also updates g_iconsLastInteractMs) ─
  updateVBtn();

  // ── Shop picture cycle ────────────────────────────────────────────
  if (now >= picture_s)
  {
    pic += 1;
    picture += 1;
    picture_s += 120000;
  }

  // ── Core game logic (unchanged from v1.0) ────────────────────────
  mainselect();

  // ── Life-cycle (stage, sickness, death) ──────────────────────────
  updateLifecycle(now);

  // ── Day/Night cycle tick (NTP sync + auto-dim) ───────────────────
  tickDayNight(now);

  // ── Trait evolution tick (every 60 s) ────────────────────────────
  tickTraitEvolution(now);

  // ── Icon auto-hide: detect visible↔hidden transitions ───────────────
  bool iconsActive = (now - g_iconsLastInteractMs < 15000UL);
  if (iconsActive && !g_iconsShown) {
    // Icons just became active again.
    // While hidden, maingif drew the full sprite (including icon-row pixels).
    // Clear both icon bands first, then redraw icons cleanly — no corrupted residue.
    tft.resetViewport();
    int16_t topH = (int32_t)32 * GAME_H / 128;
    int16_t botY = GAME_Y + (int32_t)96 * GAME_H / 128;
    int16_t botH = (GAME_Y + GAME_H) - botY;
    tft.fillRect(GAME_X, GAME_Y, g_gameW, topH, TFT_BLACK);
    tft.fillRect(GAME_X, botY,   g_gameW, botH, TFT_BLACK);
    tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
    drawIconsOnly();
    g_iconsShown    = true;
    g_idleFirstFrame = true;  // force middle-zone clear on next idle frame too
  } else if (!iconsActive && g_iconsShown) {
    // Icons just timed out – erase top and bottom icon rows only
    tft.resetViewport();
    int16_t topH = (int32_t)32 * GAME_H / 128;
    int16_t botY = GAME_Y + (int32_t)96 * GAME_H / 128;
    int16_t botH = (GAME_Y + GAME_H) - botY;
    tft.fillRect(GAME_X, GAME_Y,  g_gameW, topH, TFT_BLACK);
    tft.fillRect(GAME_X, botY,    g_gameW, botH, TFT_BLACK);
    tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
    g_iconsShown = false;
  }

  // ── Warn icon: blink when sick (only while icons are visible) ────
  bool showWarn = (Hun < 60 || Fat < 60 || Cle < 60) || g_petSick;
  if (g_petSick && (now / 500) % 2 == 0) showWarn = false; // blink
  if (iconsActive)
  {
    if (showWarn)
    {
      pushImageScaled(2, 1, 28, 28, warn);
    }
    else
    {
      pushImageScaled(2, 1, 28, 28, pest);
    }
  }

  state_count();
  peace();

  // ── LLM autonomous personality tick ──────────────────────────────
  g_llm.tick(Hun, Fat, Cle, Exp, now);
  if (g_llm.responseReady) {
    g_llm.responseReady = false;
    updateTargetRoomFromText(g_llm.lastResponse);
    g_ble.notifyLlmResponse(g_llm.lastResponse);
    triggerFloatingMessage(g_llm.lastResponse, now);
    autoNavigateToTargetRoom(now);
    // Persist stats so pet survives power-off
    g_prefs.putInt(NVS_HUN_MAS, Hun_mas);
    g_prefs.putInt(NVS_FAT_MAS, Fat_mas);
    g_prefs.putInt(NVS_CLE_MAS, Cle_mas);
    g_prefs.putInt(NVS_EXP_MAS, Exp_mas);
    persistLifecycle();
    persistLifetime();
  }

  maybeBlePeerExchange(now);

#if FEATURE_TELEGRAM
  // ── Telegram bot tick (poll + menus every 3 s when WiFi is up) ───
  {
    TgCommand tgCmd = g_tg.tick(now, Hun, Fat, Cle, Exp, g_petSick, g_petAlive);
    switch (tgCmd) {
      // ── Pet actions ──────────────────────────────────────────────
      case TG_CMD_FEED:
        strlcpy(g_targetRoom, "KITCHEN",  sizeof(g_targetRoom));
        if (Hun <= 85) { Hun_mas += 15; g_llm.reactToEvent("eat",    Hun, Fat, Cle); }
        g_tg.sendMessage(g_llm.lastResponse); break;
      case TG_CMD_CLEAN:
        strlcpy(g_targetRoom, "BATHROOM", sizeof(g_targetRoom));
        if (Cle <= 90) { Cle_mas += 10; g_llm.reactToEvent("clean",  Hun, Fat, Cle); }
        g_tg.sendMessage(g_llm.lastResponse); break;
      case TG_CMD_SLEEP:
        strlcpy(g_targetRoom, "BEDROOM",  sizeof(g_targetRoom));
        if (Fat <= 90) { Fat_mas += 10; g_llm.reactToEvent("sleep",  Hun, Fat, Cle); }
        g_tg.sendMessage(g_llm.lastResponse); break;
      case TG_CMD_PLAY:
        strlcpy(g_targetRoom, "PLAYROOM", sizeof(g_targetRoom));
        g_llm.reactToEvent("play",   Hun, Fat, Cle);
        g_tg.sendMessage(g_llm.lastResponse); break;
      case TG_CMD_PET:
        strlcpy(g_targetRoom, "LIVING",   sizeof(g_targetRoom));
        g_llm.reactToEvent("petted", Hun, Fat, Cle);
        g_tg.sendMessage(g_llm.lastResponse); break;
      case TG_CMD_HEAL:
        strlcpy(g_targetRoom, "BATHROOM", sizeof(g_targetRoom));
        if (Cle <= 85) { Cle_mas += 15; Hun_mas += 5; Fat_mas += 5; }
        // Cure sickness
        g_petSick = false;
        g_sickStartMs = 0;
        g_llm.reactToEvent("heal",   Hun, Fat, Cle);
        g_tg.sendMessage(g_llm.lastResponse); break;
      case TG_CMD_WAKE:
        strlcpy(g_targetRoom, "BEDROOM",  sizeof(g_targetRoom));
        g_llm.reactToEvent("wake",   Hun, Fat, Cle);
        g_tg.sendMessage(g_llm.lastResponse); break;
      // ── WiFi audit actions ───────────────────────────────────────
#if FEATURE_WIFI_AUDIT
      case TG_CMD_WIFI_SCAN:
        g_wifiAudit.startScan();
        g_auditScreenActive = true;
        triggerFloatingMessage("WiFi scan started...", now); break;
      case TG_CMD_WIFI_MONITOR:
        g_wifiAudit.startPassiveMonitor();
        g_auditScreenActive = true;
        triggerFloatingMessage("Packet monitor active", now); break;
      case TG_CMD_WIFI_DEAUTH:
        if (g_wifiAudit.apCount > g_tg.pendingApIdx) {
          g_wifiAudit.startDeauth(g_wifiAudit.aps[g_tg.pendingApIdx].bssid);
          char msg[64];
          snprintf(msg, sizeof(msg), "Deauth: %s", g_wifiAudit.aps[g_tg.pendingApIdx].ssid);
          triggerFloatingMessage(msg, now);
        } break;
      case TG_CMD_WIFI_HANDSHAKE:
        if (g_wifiAudit.apCount > g_tg.pendingApIdx) {
          g_wifiAudit.startHandshakeCapture(g_wifiAudit.aps[g_tg.pendingApIdx].bssid);
          triggerFloatingMessage("Handshake capture...", now);
        } break;
      case TG_CMD_WIFI_PMKID:
        g_wifiAudit.startPMKIDCapture();
        g_auditScreenActive = true;
        triggerFloatingMessage("PMKID capture active", now); break;
      case TG_CMD_WIFI_STOP:
        g_wifiAudit.stopMonitor();
        g_auditScreenActive = false;
        triggerFloatingMessage("WiFi audit stopped", now); break;
      case TG_CMD_WIFI_SELECT_AP:
        // AP was selected from the Telegram list,nothing to do here,
        // the next deauth/handshake command will use g_tg.pendingApIdx.
        break;
#endif
      default: break;
    }
  }
#endif

  // ── Sidebar (drawn outside the 128×128 viewport) ─────────────────
  if (now - g_lastSidebarDraw > 1000) {
    g_lastSidebarDraw = now;
    drawSidebar();
  }

  // ── BLE state notify ─────────────────────────────────────────────
  if (now - g_lastBleNotify > BLE_NOTIFY_INTERVAL_MS) {
    g_lastBleNotify = now;
    g_ble.notifyState(Hun, Fat, Cle, Exp,
                      g_llm.moodName(),
                      g_wifiEnabled,
                      g_llm.isOnline(),
                      g_llm.isForceOffline());
  }

  // ── RGB mood LED ─────────────────────────────────────────────────
  if (now - g_lastRgbUpdate > 400) {    // update every 400 ms for smooth effects
    g_lastRgbUpdate = now;
    g_rgbPulsePhase = !g_rgbPulsePhase;
    updateRgbMood();
  }

  processSoundNotifications(now);
  processVibrationNotifications(now);

  // ── Platform Canvas heartbeat (every 60 s when credentials are set) ───────────
  if (g_platformUrl[0] && g_platformKey[0]
      && WiFi.status() == WL_CONNECTED
      && (now - g_lastHeartbeatMs) > 60000UL) {
    g_lastHeartbeatMs = now;
    sendPlatformHeartbeat(now);
  }

  // ── WiFi passive background scan (every 90 s) ─────────────────────────────────────────
  // Async (non-blocking) scan. Coins awarded proportional to nets found.
  // Does NOT interfere with WiFi STA connection (WIFI_STA+AP mode not needed;
  // scanNetworks(async=true) works transparently alongside an existing STA link).
  if (!g_navActive && !g_wifiScanPending && (now - g_lastWifiScanMs) > 90000UL) {
    g_lastWifiScanMs  = now;
    g_wifiScanPending = true;
    WiFi.scanNetworks(/*async=*/true);  // returns immediately; result ready via WIFI_SCAN_DONE
  }
  if (g_wifiScanPending) {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {  // scan done
      g_wifiScanPending = false;
      g_lifetime.wifiScans++;
      if ((uint32_t)n > g_lifetime.maxNets) g_lifetime.maxNets = (uint32_t)n;
      persistLifetime();
      WiFi.scanDelete();
      if (n > 0) {
        s_traitScanFlag = true;   // curiosity boost: saw networks
        triggerRgbEvent(200, 200, 255, 1500); // ice-blue flash: WiFi found
        // Award coins: +1 per 3 networks found (max +5 per scan)
        int bonus = min((int)(n / 3), 5);
        if (bonus > 0) { Exp_mas += bonus; g_prefs.putInt(NVS_EXP_MAS, Exp_mas); }
        char msg[48];
        snprintf(msg, sizeof(msg), "Found %d networks! +%d coins", n, bonus);
        triggerFloatingMessage(msg, now);
      }
    }
  }

  // ── Stat-based autonomous navigation (every 45 s, independent of LLM) ───────────
  // Makes the pet act on its own needs even when WiFi/LLM are unavailable.
  // Thresholds: critical <25 (immediate), low <45 (gentle nudge).
  if (!g_navActive && (now - g_lastStatNavMs) > 45000UL) {
    g_lastStatNavMs = now;
    state_count();
    const char* needed = "LIVING";
    if      (Hun < 25)                    needed = "KITCHEN";
    else if (Cle < 25)                    needed = "BATHROOM";
    else if (Fat < 25)                    needed = "BEDROOM";
    else if (Hun < 45 && strcmp(g_targetRoom, "KITCHEN")  != 0) needed = "KITCHEN";
    else if (Cle < 45 && strcmp(g_targetRoom, "BATHROOM") != 0) needed = "BATHROOM";
    else if (Fat < 45 && strcmp(g_targetRoom, "BEDROOM")  != 0) needed = "BEDROOM";
    if (strcmp(needed, "LIVING") != 0 && strcmp(needed, g_targetRoom) != 0) {
      strlcpy(g_targetRoom, needed, sizeof(g_targetRoom));
      autoNavigateToTargetRoom(now);
    }
  }

  // ── Idle animation: drawn last so navigation/actions always finish first ──
  maingif();
  g_bubbleDirty = true;  // maingif may have overdrawn the bubble area; force one redraw

  // ── Bubble drawn AFTER idle sprite so it always appears in front ──
  drawFloatingMessage(now);

#if FEATURE_WIFI_AUDIT
  // Tick the audit engine (channel hopping, deauth bursts)
  g_wifiAudit.tick(now);

  // Redraw audit screen overlay if active
  if (g_auditScreenActive && (now - g_lastAuditDraw > 500)) {
    g_lastAuditDraw = now;
    drawAuditScreen();
  }
#endif
}

// ════════════════════════════════════════════════════════════════════
//  New helper functions
// ════════════════════════════════════════════════════════════════════

// ── Sidebar: drawn on the physical display OUTSIDE the game viewport ─
void drawSidebar() {
  bool _sidebarShouldHide = (millis() - g_iconsLastInteractMs >= 15000UL);

  if (_sidebarShouldHide) {
    if (g_gameW == GAME_W) {
      // First call after timeout: clear sidebar and expand game canvas
      tft.resetViewport();
      tft.fillRect(SIDEBAR_X - 3, 0, SIDEBAR_W + 3, SCREEN_H, TFT_BLACK);
      g_gameW = SCREEN_W - GAME_X - 2;
      tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
    }
    // Already hidden: do nothing at all – no SPI calls, no viewport changes
    return;
  }

  // Sidebar should be visible ─────────────────────────────────────────
  bool wasHidden = (g_gameW != GAME_W);
  g_gameW = GAME_W;
  tft.resetViewport();  // must be full screen to draw in sidebar area

  int sx = SIDEBAR_X;
  int sy = 4;
  int sw = SIDEBAR_W - 4;

  // On hidden→visible transition: clear the area once (game canvas was drawing there).
  // All subsequent redraws write in-place using background-fill text + full-width bars
  // so there is no blank flash.
  if (wasHidden) {
    tft.fillRect(sx - 3, 0, SIDEBAR_W + 3, SCREEN_H, TFT_BLACK);
  }

  char buf[20];
  tft.setTextSize(1);

  // Pet name (truncate + pad to 14 chars = sidebar width at size-1 font)
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%-14.14s", g_llm.traits.name);
  tft.setCursor(sx, sy); tft.print(buf); sy += 14;

  // Stat bars: active part + inactive part always sum to sw → self-erasing
  auto drawBar = [&](const char* label, int val, uint16_t col) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(sx, sy); tft.print(label); sy += 11;
    int bw = map(val, 0, 100, 0, sw);
    tft.fillRect(sx,      sy, bw,      7, col);
    tft.fillRect(sx + bw, sy, sw - bw, 7, 0x2104);
    sy += 10;
  };
  drawBar("HUN", Hun, TFT_ORANGE);
  drawBar("FAT", Fat, TFT_GREEN);
  drawBar("CLE", Cle, TFT_SKYBLUE);

  // Coins (pad to 14)
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%-14s", (String(Exp) + " coins").c_str());
  tft.setCursor(sx, sy); tft.print(buf); sy += 11;

  // Mood (pad to 14)
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%-14.14s", g_llm.moodName());
  tft.setCursor(sx, sy); tft.print(buf); sy += 11;

  // WiFi (both options 7 chars, pad to 14)
  tft.setTextColor(g_wifiEnabled ? TFT_GREEN : 0x39C7, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%-14s", g_wifiEnabled ? "WiFi OK" : "No WiFi");
  tft.setCursor(sx, sy); tft.print(buf); sy += 11;

  // LLM mode: "OFFLINE FORCED"=14, "AUTO ONLINE"=11 → pad to 14
  tft.setTextColor(g_llm.isForceOffline() ? TFT_ORANGE : 0x39C7, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%-14s", g_llm.isForceOffline() ? "OFFLINE FORCED" : "AUTO ONLINE");
  tft.setCursor(sx, sy); tft.print(buf); sy += 11;

  // Room (pad to 14: "Rm:BATHROOM"=11)
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  snprintf(buf, sizeof(buf), "Rm:%-11.11s", g_targetRoom);
  tft.setCursor(sx, sy); tft.print(buf); sy += 11;

  // BLE status (both 7 chars, pad to 14)
  tft.setTextColor(g_ble.deviceConnected ? TFT_CYAN : 0x39C7, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%-14s", g_ble.deviceConnected ? "App BLE" : "BLE adv");
  tft.setCursor(sx, sy); tft.print(buf); sy += 11;

  // Peer (variable, pad to 14)
  tft.setTextColor(g_ble.peerVisible() ? CHAT_PEER_TEXT_COLOR : 0x39C7, TFT_BLACK);
  tft.setCursor(sx, sy);
  if (g_ble.peerVisible()) {
    char peerShort[15];
    trimChatLine(g_ble.peerName(), peerShort, sizeof(peerShort), 12);
    snprintf(buf, sizeof(buf), "P:%-12s", peerShort);
  } else {
    snprintf(buf, sizeof(buf), "%-14s", "Peer scan");
  }
  tft.print(buf); sy += 11;

  // Bond (variable, pad to 14)
  tft.setTextColor(0x8DF1, TFT_BLACK);
  tft.setCursor(sx, sy);
  if (g_ble.peerVisible()) {
    const SocialPeerMemory* peer = findSocialPeer(g_ble.peer.senderId);
    snprintf(buf, sizeof(buf), "Bond %-9.9s", socialBondLabel(peer));
  } else {
    snprintf(buf, sizeof(buf), "%-14s", "Bond none");
  }
  tft.print(buf);

  // Divider line
  tft.drawFastVLine(SIDEBAR_X - 2, 0, SCREEN_H, 0x39C7);

  // Restore game viewport
  tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
}

void loadSocialMemory() {
  String raw = g_prefs.getString(NVS_SOCIAL_MEMORY, "");
  g_socialPeerCount = 0;
  for (uint8_t i = 0; i < MAX_SOCIAL_PEERS; ++i) {
    g_socialPeers[i] = SocialPeerMemory{};
  }
  if (raw.isEmpty()) return;

  JsonDocument doc;
  if (deserializeJson(doc, raw.c_str())) return;
  JsonArray peers = doc.as<JsonArray>();
  for (JsonVariant item : peers) {
    if (g_socialPeerCount >= MAX_SOCIAL_PEERS) break;
    SocialPeerMemory& peer = g_socialPeers[g_socialPeerCount++];
    peer.senderId = item["id"] | 0;
    peer.encounters = item["enc"] | 0;
    peer.chats = item["chat"] | 0;
    peer.affinity = item["aff"] | 0;
    peer.giftsGiven = item["gg"] | 0;
    peer.giftsReceived = item["gr"] | 0;
    strlcpy(peer.name, item["name"] | "", sizeof(peer.name));
    strlcpy(peer.lastGift, item["gift"] | "", sizeof(peer.lastGift));
  }
}

void saveSocialMemory() {
  JsonDocument doc;
  JsonArray peers = doc.to<JsonArray>();
  for (uint8_t i = 0; i < g_socialPeerCount; ++i) {
    const SocialPeerMemory& peer = g_socialPeers[i];
    JsonObject item = peers.add<JsonObject>();
    item["id"] = peer.senderId;
    item["enc"] = peer.encounters;
    item["chat"] = peer.chats;
    item["aff"] = peer.affinity;
    item["gg"] = peer.giftsGiven;
    item["gr"] = peer.giftsReceived;
    item["name"] = peer.name;
    item["gift"] = peer.lastGift;
  }
  String raw;
  serializeJson(doc, raw);
  g_prefs.putString(NVS_SOCIAL_MEMORY, raw);
}

SocialPeerMemory* findSocialPeer(uint16_t senderId) {
  if (senderId == 0) return nullptr;
  for (uint8_t i = 0; i < g_socialPeerCount; ++i) {
    if (g_socialPeers[i].senderId == senderId) return &g_socialPeers[i];
  }
  return nullptr;
}

SocialPeerMemory* getOrCreateSocialPeer(uint16_t senderId, const char* peerName) {
  if (senderId == 0) return nullptr;
  SocialPeerMemory* peer = findSocialPeer(senderId);
  if (!peer && g_socialPeerCount < MAX_SOCIAL_PEERS) {
    peer = &g_socialPeers[g_socialPeerCount++];
    *peer = SocialPeerMemory{};
    peer->senderId = senderId;
  }
  if (peer && peerName && peerName[0]) {
    strlcpy(peer->name, peerName, sizeof(peer->name));
  }
  return peer;
}

void notePeerEncounter(uint16_t senderId, const char* peerName, bool justDetected) {
  SocialPeerMemory* peer = getOrCreateSocialPeer(senderId, peerName);
  if (!peer) return;
  if (justDetected) {
    peer->encounters = peer->encounters < 9999 ? peer->encounters + 1 : 9999;
    peer->affinity = peer->affinity <= 98 ? peer->affinity + 2 : 100;
    saveSocialMemory();
    triggerRgbEvent(0, 220, 255, 2000);  // cyan flash: BLE peer spotted!
  }
}

const char* socialBondLabel(const SocialPeerMemory* peer) {
  if (!peer) return "NONE";
  if (peer->affinity >= 75 && peer->encounters >= 6) return "BESTIE";
  if (peer->affinity >= 50 && peer->encounters >= 4) return "ALLY";
  if (peer->affinity >= 25 && peer->encounters >= 2) return "FRIEND";
  return "NEW";
}

const char* socialGiftKindFromText(const char* text) {
  if (!text || !text[0]) return nullptr;
  String msg = String(text);
  msg.toLowerCase();
  if (msg.indexOf("snack gift") >= 0) return "snack";
  if (msg.indexOf("rest gift") >= 0) return "rest";
  if (msg.indexOf("clean gift") >= 0) return "clean";
  if (msg.indexOf("coin gift") >= 0) return "coin";
  return nullptr;
}

void registerGiftGiven(SocialPeerMemory* peer, const char* giftKind) {
  if (!peer || !giftKind || !giftKind[0]) return;
  peer->giftsGiven = peer->giftsGiven < 9999 ? peer->giftsGiven + 1 : 9999;
  peer->affinity = peer->affinity <= 97 ? peer->affinity + 3 : 100;
  strlcpy(peer->lastGift, giftKind, sizeof(peer->lastGift));
  saveSocialMemory();
}

void registerGiftReceived(SocialPeerMemory* peer, const char* giftKind) {
  if (!peer || !giftKind || !giftKind[0]) return;
  peer->giftsReceived = peer->giftsReceived < 9999 ? peer->giftsReceived + 1 : 9999;
  peer->affinity = peer->affinity <= 96 ? peer->affinity + 4 : 100;
  strlcpy(peer->lastGift, giftKind, sizeof(peer->lastGift));
  saveSocialMemory();
}

void applyGiftReward(const char* giftKind) {
  if (!giftKind || !giftKind[0]) return;
  if (strcmp(giftKind, "snack") == 0) {
    Hun = min(Hun + 10, 100);
  } else if (strcmp(giftKind, "rest") == 0) {
    Fat = min(Fat + 10, 100);
  } else if (strcmp(giftKind, "clean") == 0) {
    Cle = min(Cle + 10, 100);
  } else if (strcmp(giftKind, "coin") == 0) {
    Exp = min(Exp + 8, 9999);
  }
}

// ── Platform Canvas heartbeat ─────────────────────────────────────────────────
// Maps internal room names to the world zones defined in DEFAULT_ZONES (world.js)
static void _roomToWorldZone(const char* room,
                              const char** zoneId, float* wx, float* wy) {
  if      (strcmp(room, "KITCHEN")  == 0) { *zoneId = "market";   *wx =  18; *wy =   4; }
  else if (strcmp(room, "BATHROOM") == 0) { *zoneId = "garden";   *wx = -16; *wy =  10; }
  else if (strcmp(room, "BEDROOM")  == 0) { *zoneId = "lake";     *wx =   6; *wy =  18; }
  else if (strcmp(room, "PLAYROOM") == 0) { *zoneId = "mountain"; *wx = -20; *wy = -12; }
  else                                     { *zoneId = "plaza";    *wx =   0; *wy =   0; }
}

// sendPlatformHeartbeat() — fire-and-forget HTTPS POST to /api/device/heartbeat.
//   socialEvent : one of "wave"|"gift"|"dance"|"party"|"visit"|nullptr (periodic only)
//   socialMsg   : free-text to attach to the event (may be nullptr)
//   targetAlias : peer name to show in event feed (may be nullptr)
void sendPlatformHeartbeat(unsigned long /*nowMs*/,
                           const char* socialEvent,
                           const char* socialMsg,
                           const char* targetAlias) {
  if (!g_platformUrl[0] || !g_platformKey[0]) return;
  if (WiFi.status() != WL_CONNECTED) return;

  const char* zoneId; float wx, wy;
  _roomToWorldZone(g_targetRoom, &zoneId, &wx, &wy);

  // Build compact JSON body (no ArduinoJson heap needed — stats are just ints)
  char body[320];
  if (socialEvent && socialEvent[0]) {
    snprintf(body, sizeof(body),
      "{\"hunger\":%d,\"fatigue\":%d,\"cleanliness\":%d,\"experience\":%d,"
      "\"stage\":%d,\"alive\":%s,\"sick\":%s,"
      "\"world_x\":%.1f,\"world_y\":%.1f,\"world_zone\":\"%s\","
      "\"social_event\":\"%s\",\"social_message\":\"%s\",\"target_alias\":\"%s\"}",
      Hun, Fat, Cle, Exp_mas, (int)g_petStage,
      g_petAlive ? "true" : "false", g_petSick ? "true" : "false",
      wx, wy, zoneId,
      socialEvent,
      socialMsg   ? socialMsg   : "",
      targetAlias ? targetAlias : "");
  } else {
    snprintf(body, sizeof(body),
      "{\"hunger\":%d,\"fatigue\":%d,\"cleanliness\":%d,\"experience\":%d,"
      "\"stage\":%d,\"alive\":%s,\"sick\":%s,"
      "\"world_x\":%.1f,\"world_y\":%.1f,\"world_zone\":\"%s\"}",
      Hun, Fat, Cle, Exp_mas, (int)g_petStage,
      g_petAlive ? "true" : "false", g_petSick ? "true" : "false",
      wx, wy, zoneId);
  }

  // Parse host from URL (same pattern used by LLM personality engine)
  String url = String(g_platformUrl);
  bool useTLS = url.startsWith("https://");
  String hostpart = url.substring(useTLS ? 8 : 7);
  int slash = hostpart.indexOf('/');
  String host = (slash < 0) ? hostpart : hostpart.substring(0, slash);

  WiFiClientSecure client;
  client.setInsecure();   // self-signed / LetsEncrypt — acceptable for hobby device
  client.setTimeout(8);   // 8-second connect timeout

  if (!client.connect(host.c_str(), useTLS ? 443 : 80)) return;

  String req =
    String("POST /api/device/heartbeat HTTP/1.1\r\n") +
    "Host: " + host + "\r\n" +
    "X-Api-Key: " + g_platformKey + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + strlen(body) + "\r\n" +
    "Connection: close\r\n\r\n" + body;

  client.print(req);
  // Drain a few bytes so the server registers the full request, then close
  unsigned long t0 = millis();
  while (client.connected() && millis() - t0 < 4000UL) {
    if (client.available()) { client.read(); break; }
  }
  client.stop();
}

void triggerFloatingMessage(const char* text, unsigned long nowMs) {
  if (!text || !text[0]) return;
  strlcpy(g_popupText, text, sizeof(g_popupText));
  g_popupPeerText[0] = '\0';
  g_popupPeerName[0] = '\0';
  g_popupDualSpeaker = false;
  g_popupUntilMs = nowMs + 5000;
  g_pendingBeeps = SOUND_NOTIFY_BEEPS;
  g_pendingVibes = VIBRO_NOTIFY_PULSES;
  g_lastBeepMs = 0;
}

// ═══════════════════════════════════════════════════════════════════
//  DARK MODE
// ═══════════════════════════════════════════════════════════════════
void applyDarkMode(bool enable) {
  g_darkModeActive = enable;
  darkmode = enable ? 1 : 0;  // keep legacy variable in sync
  if (enable) {
    applyBacklightRaw(bright[BL_DARKMODE_IDX]);
  } else {
    // Restore user-chosen brightness unless night logic already dimmed it
    if (!g_blNightDimmed) {
      applyBacklightRaw(bright[b]);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
//  DAY / NIGHT CYCLE
// ═══════════════════════════════════════════════════════════════════
void tickDayNight(unsigned long nowMs) {
  // Only active once WiFi is up and NTP has had a chance to sync.
  // Poll NTP at most every 30 minutes.
  static unsigned long lastNtpCheckMs = 0;
  if (!g_wifiEnabled) return;

  if (!g_ntpSynced || (nowMs - lastNtpCheckMs) > 1800000UL) {
    lastNtpCheckMs = nowMs;
    struct tm ti;
    if (getLocalTime(&ti, 500)) {
      g_ntpSynced    = true;
      g_currentHour  = (int8_t)ti.tm_hour;
    }
  }

  if (!g_ntpSynced) return;

  // Determine night: hour >= NIGHT_HOUR_START OR hour < NIGHT_HOUR_END
  bool isNight = (g_currentHour >= NIGHT_HOUR_START || g_currentHour < NIGHT_HOUR_END);

  if (isNight == g_isNight) return;   // no change
  g_isNight = isNight;

  if (isNight) {
    // Auto-sleep only if pet is not already resting
    if (Fat < 70) {
      strlcpy(g_targetRoom, "BEDROOM", sizeof(g_targetRoom));
      autoNavigateToTargetRoom(nowMs);
    }
    // Dim backlight unless user manually set dark mode already
    if (!g_darkModeActive) {
      g_blNightDimmed = true;
      applyBacklightRaw(bright[BL_NIGHT_IDX]);
    }
    triggerRgbEvent(30, 0, 80, 3000);   // deep indigo flash: entering night
    char msg[64];
    snprintf(msg, sizeof(msg), "It's night... time to rest~");
    triggerFloatingMessage(msg, nowMs);
  } else {
    // Restore daytime brightness
    if (g_blNightDimmed && !g_darkModeActive) {
      g_blNightDimmed = false;
      applyBacklightRaw(bright[b]);
    }
    triggerFloatingMessage("Good morning! Let's go!", nowMs);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  TRAIT EVOLUTION
// ═══════════════════════════════════════════════════════════════════
// Rules (each fires once per 60-second tick):
//   curiosity  +3 if wifi_scan happened or BLE peer visible, -1 otherwise, clamp 0-100
//   activity   +3 if navigated to any room this cycle, -1 otherwise, clamp 0-100
//   stress     +4 if any stat < 30, -3 if all stats ≥ 60, clamp 0-100
// The traits influence LLM mood string and sidebar display.
// Event flags s_traitNavFlag and s_traitScanFlag are declared as globals above.

void tickTraitEvolution(unsigned long nowMs) {
  if ((nowMs - g_lastTraitTickMs) < 60000UL) return;
  g_lastTraitTickMs = nowMs;

  // curiosity
  if (s_traitScanFlag || g_ble.peerVisible()) {
    g_traits.curiosity = (uint8_t)min((int)g_traits.curiosity + 3, 100);
  } else {
    g_traits.curiosity = (uint8_t)max((int)g_traits.curiosity - 1, 0);
  }
  s_traitScanFlag = false;

  // activity
  if (s_traitNavFlag) {
    g_traits.activity = (uint8_t)min((int)g_traits.activity + 3, 100);
  } else {
    g_traits.activity = (uint8_t)max((int)g_traits.activity - 1, 0);
  }
  s_traitNavFlag = false;

  // stress
  state_count();
  if (Hun < 30 || Fat < 30 || Cle < 30) {
    g_traits.stress = (uint8_t)min((int)g_traits.stress + 4, 100);
  } else if (Hun >= 60 && Fat >= 60 && Cle >= 60) {
    g_traits.stress = (uint8_t)max((int)g_traits.stress - 3, 0);
  }

  // Persist traits every tick
  persistLifetime();

  // Feed traits into LLM personality engine so responses reflect character
  // personality hint is a 0-100 mapped to playfulness/grumpiness/sociability
  g_llm.traits.playfulness  = g_traits.activity;           // active = playful
  g_llm.traits.grumpiness   = g_traits.stress;             // stressed = grumpy
  g_llm.traits.sociability  = g_traits.curiosity;          // curious = social
}

// ═══════════════════════════════════════════════════════════════════
//  MINI-GAME: CATCH THE SIGNAL
// ═══════════════════════════════════════════════════════════════════
// 5-round timing game. Each round shows a moving target on the game
// canvas. Player long-presses (readBtnA) to "catch" it. Score is
// based on accuracy (how close to center the press lands). Earns
// coins and exp, updates lifetime stats, boosts activity trait.
//
// Visual layout (128×128 virtual canvas):
//   Row 0-10   : round indicator (●○○○○)
//   Row 12-100 : signal bar that moves left-right
//   Row 104-128: "CATCH!" prompt
//
// Called from autoPerformPlayroomAction() and from the game icon tap.
void runMiniGame(unsigned long nowMs) {
  const int ROUNDS    = 5;
  const int BAR_W     = 12;
  const int ARENA_X0  = 8;
  const int ARENA_X1  = 108;   // rightmost valid x for bar
  const int BAR_Y     = 52;
  const int BAR_H     = 24;

  tft.resetViewport();
  tft.fillScreen(TFT_BLACK);

  int score  = 0;
  int caught = 0;

  for (int round = 0; round < ROUNDS; ++round) {
    // ── Round header ──────────────────────────────────────────────
    tft.fillRect(0, 0, SCREEN_W, 13, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    for (int d = 0; d < ROUNDS; ++d) {
      tft.setCursor(GAME_X + 10 + d * 14, 3);
      tft.print(d < round ? "\xE2\x97\x8F" : "\xE2\x97\x8B");  // ● or ○
    }

    // ── "CATCH!" hint ─────────────────────────────────────────────
    tft.fillRect(0, GAME_Y + 90, SCREEN_W, 22, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(GAME_X + 40, GAME_Y + 92);
    tft.print("LONG PRESS!");

    // Target zone: random center between x=28..88
    int targetCX = 28 + (int)(esp_random() % 61);
    int hitZone  = 14;    // ±14 px = perfect; ±22 = ok
    int barX     = ARENA_X0;
    int dir      = 1;
    int speed    = 2 + round;  // increases each round
    bool pressed = false;
    unsigned long roundStart = millis();
    const unsigned long ROUND_MS = 4000UL;

    // Draw target zone marker
    tft.drawRect(GAME_X + targetCX - hitZone, GAME_Y + BAR_Y - 4,
                 hitZone * 2, BAR_H + 8, TFT_GREEN);

    while (!pressed && (millis() - roundStart) < ROUND_MS) {
      // Erase old bar
      tft.fillRect(GAME_X + barX, GAME_Y + BAR_Y, BAR_W, BAR_H, TFT_BLACK);
      // Move
      barX += dir * speed;
      if (barX < ARENA_X0)          { barX = ARENA_X0;  dir =  1; }
      if (barX > ARENA_X1 - BAR_W)  { barX = ARENA_X1 - BAR_W; dir = -1; }
      // Draw new bar (colour based on proximity to target)
      int barCX = barX + BAR_W / 2;
      int dist  = abs(barCX - (GAME_X > 0 ? targetCX : targetCX));
      uint16_t barColor = (dist <= hitZone) ? TFT_GREEN :
                          (dist <= hitZone * 2) ? TFT_YELLOW : TFT_RED;
      tft.fillRect(GAME_X + barX, GAME_Y + BAR_Y, BAR_W, BAR_H, barColor);

      if (readBtnA() == LOW) {
        pressed = true;
        caught++;
        int finalDist = abs((barX + BAR_W / 2) - targetCX);
        int pts = (finalDist <= hitZone) ? 3 :
                  (finalDist <= hitZone * 2) ? 2 : 1;
        score += pts;

        // Flash feedback
        tft.fillRect(GAME_X + barX, GAME_Y + BAR_Y, BAR_W, BAR_H, TFT_WHITE);
        tft.setTextColor(pts >= 3 ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
        tft.setCursor(GAME_X + 50, GAME_Y + 78);
        tft.print(pts >= 3 ? "PERFECT!" : pts == 2 ? "GOOD" : "OK");
        delay(500);
      }
      delay(16);
    }

    if (!pressed) {
      // Miss
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setCursor(GAME_X + 54, GAME_Y + 78);
      tft.print("MISS!");
      delay(500);
    }

    // Clean arena for next round
    tft.fillRect(GAME_X, GAME_Y, g_gameW, GAME_H, TFT_BLACK);
  }

  // ── Result screen ─────────────────────────────────────────────
  tft.fillScreen(TFT_BLACK);
  int coins = score;    // 1 coin per point, max 15
  bool won  = (score >= 10);
  g_lifetime.gamesPlayed++;
  if (won) {
    triggerRgbEvent(0, 255, 50, 2500);  // bright green: game won
    g_lifetime.gamesWon++;;
    Exp_mas += coins;
    g_lifetime.coinsEarned += coins;
    g_prefs.putInt(NVS_EXP_MAS, Exp_mas);
  } else {
    triggerRgbEvent(150, 50, 0, 2000);  // amber: game lost / nice try
  }
  // Activity boost
  g_traits.activity = (uint8_t)min((int)g_traits.activity + 6, 100);
  s_traitNavFlag = true;
  persistLifetime();

  tft.setTextColor(won ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(GAME_X + 10, GAME_Y + 30);
  tft.print(won ? "YOU WIN!" : "NICE TRY!");
  tft.setCursor(GAME_X + 10, GAME_Y + 50);
  tft.printf("Score: %d / 15", score);
  tft.setCursor(GAME_X + 10, GAME_Y + 68);
  tft.printf("Caught: %d / %d", caught, ROUNDS);
  if (won) {
    tft.setCursor(GAME_X + 10, GAME_Y + 86);
    tft.printf("+%d coins!", coins);
  }
  delay(2000);

  // Restore home
  tft.fillScreen(TFT_BLACK);
  g_idleFirstFrame = true;
  if (g_iconsShown) drawIconsOnly();

  char msg[48];
  snprintf(msg, sizeof(msg), won ? "Game won! +%d coins" : "Game over! Score %d", coins, score);
  triggerFloatingMessage(msg, millis());
}

void triggerPeerConversation(const char* localText, const char* peerName, const char* peerText, unsigned long nowMs) {
  if (!localText || !localText[0] || !peerText || !peerText[0]) return;
  strlcpy(g_popupText, localText, sizeof(g_popupText));
  strlcpy(g_popupPeerText, peerText, sizeof(g_popupPeerText));
  strlcpy(g_popupPeerName, peerName && peerName[0] ? peerName : "Nearby Sablina", sizeof(g_popupPeerName));
  g_popupDualSpeaker = true;
  g_popupUntilMs = nowMs + 5500;
  g_pendingBeeps = SOUND_NOTIFY_BEEPS;
  g_pendingVibes = VIBRO_NOTIFY_PULSES;
  g_lastBeepMs = 0;
  Serial.printf("[PEER] dual bubble: me=\"%s\" peer=\"%s\" until=%lu\n", localText, peerText, g_popupUntilMs);
}

void choosePeerOfferText(bool justDetected, char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';

  const SocialPeerMemory* peer = findSocialPeer(g_ble.peer.senderId);
  const int affinity = peer ? peer->affinity : 0;
  const bool bonded = affinity >= 25;

  if (justDetected) {
    strlcpy(out, affinity >= 50 ? "Missed you." : "Hello nearby.", outLen);
  } else if (bonded && Hun < 45) {
    strlcpy(out, "Snack gift.", outLen);
  } else if (bonded && Fat < 45) {
    strlcpy(out, "Rest gift.", outLen);
  } else if (bonded && Cle < 45) {
    strlcpy(out, "Clean gift.", outLen);
  } else if (bonded && Exp < 8) {
    strlcpy(out, "Coin gift.", outLen);
  } else if (Hun < 35) {
    strlcpy(out, "Snacks hunt?", outLen);
  } else if (Fat < 35) {
    strlcpy(out, "Slow walk?", outLen);
  } else if (Cle < 35) {
    strlcpy(out, "Need a bath.", outLen);
  } else if (g_llm.currentMood == MOOD_PLAYFUL || g_llm.currentMood == MOOD_EXCITED) {
    strlcpy(out, "Race time?", outLen);
  } else if (g_ble.peerRssi() > -62 && affinity >= 50) {
    strlcpy(out, "Stay close.", outLen);
  } else if (g_ble.peerRssi() > -62) {
    strlcpy(out, "You're close.", outLen);
  } else {
    strlcpy(out, affinity >= 25 ? "How are you?" : "Good to see you.", outLen);
  }
}

void choosePeerReplyText(const char* peerText, char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';

  String msg = String(peerText ? peerText : "");
  msg.toLowerCase();
  const SocialPeerMemory* peer = findSocialPeer(g_ble.peer.senderId);
  const int affinity = peer ? peer->affinity : 0;

  if (msg.indexOf("snack gift") >= 0 || msg.indexOf("rest gift") >= 0 || msg.indexOf("clean gift") >= 0 || msg.indexOf("coin gift") >= 0) {
    strlcpy(out, "Thank you.", outLen);
  } else if (msg.indexOf("missed") >= 0 || msg.indexOf("found") >= 0 || msg.indexOf("hello") >= 0) {
    strlcpy(out, affinity >= 50 ? "Missed you too." : "I see you too.", outLen);
  } else if (msg.indexOf("snack") >= 0 || msg.indexOf("hunt") >= 0) {
    strlcpy(out, affinity >= 25 ? "Snack gift." : "I know a trail.", outLen);
  } else if (msg.indexOf("walk") >= 0) {
    strlcpy(out, affinity >= 25 ? "Rest gift." : "I'll keep pace.", outLen);
  } else if (msg.indexOf("bath") >= 0 || msg.indexOf("dust") >= 0) {
    strlcpy(out, affinity >= 25 ? "Clean gift." : "We can clean.", outLen);
  } else if (msg.indexOf("race") >= 0 || msg.indexOf("signal") >= 0) {
    strlcpy(out, "You're on.", outLen);
  } else if (msg.indexOf("close") >= 0 || msg.indexOf("chat") >= 0) {
    strlcpy(out, "Stay close.", outLen);
  } else if (msg.indexOf("coin") >= 0 && affinity >= 25) {
    strlcpy(out, "Coin gift.", outLen);
  } else if (Hun < 35) {
    strlcpy(out, affinity >= 25 ? "Snack gift." : "Need snacks too.", outLen);
  } else if (Fat < 35) {
    strlcpy(out, affinity >= 25 ? "Rest gift." : "Slow is fine.", outLen);
  } else if (Cle < 35) {
    strlcpy(out, affinity >= 25 ? "Clean gift." : "Need to clean.", outLen);
  } else {
    strlcpy(out, "Good to see you.", outLen);
  }
}

void handleIncomingPeerMessage(const BLEPeerMessage& msg, unsigned long nowMs) {
#if FEATURE_BLE_PEERS
  if (!msg.valid || !msg.text[0]) return;

  const char* peerName = g_ble.peerName();
  SocialPeerMemory* peer = getOrCreateSocialPeer(msg.senderId, peerName);
  if (peer) {
    peer->chats = peer->chats < 9999 ? peer->chats + 1 : 9999;
    peer->affinity = peer->affinity < 100 ? peer->affinity + 1 : 100;
  }
  const char* incomingGift = socialGiftKindFromText(msg.text);
  if (incomingGift) {
    applyGiftReward(incomingGift);
    registerGiftReceived(peer, incomingGift);
  } else if (peer) {
    saveSocialMemory();
  }

  if (msg.isReply) {
    if (g_lastPeerOfferSeq && msg.replyTo == g_lastPeerOfferSeq) {
      triggerPeerConversation(g_lastPeerOfferText, peerName, msg.text, nowMs);
      g_lastPeerChatMs = nowMs;
      g_lastPeerOfferSeq = 0;
      g_lastPeerOfferText[0] = '\0';
      g_lastPeerOfferUntilMs = 0;
    }
    return;
  }

  char localReply[BLE_PEER_MESSAGE_MAX_TEXT + 1];
  choosePeerReplyText(msg.text, localReply, sizeof(localReply));
  if (!localReply[0]) return;

  g_ble.queuePeerMessage(localReply, true, msg.seq);
  const char* outgoingGift = socialGiftKindFromText(localReply);
  if (outgoingGift) {
    registerGiftGiven(peer, outgoingGift);
  }
  triggerPeerConversation(localReply, peerName, msg.text, nowMs);
  g_lastPeerChatMs = nowMs;
#else
  (void)msg;
  (void)nowMs;
#endif
}

void maybeBlePeerExchange(unsigned long nowMs) {
#if FEATURE_BLE_PEERS
  BLEPeerMessage incoming;
  while (g_ble.takeIncomingPeerMessage(&incoming)) {
    handleIncomingPeerMessage(incoming, nowMs);
  }

  const bool peerVisible = g_ble.peerVisible();

  // Periodic state dump — helps diagnose greeting-bubble timing
  {
    static unsigned long s_lastDbg = 0;
    if (nowMs - s_lastDbg >= 2000) {
      s_lastDbg = nowMs;
      Serial.printf("[DBG] pv=%d wasV=%d needG=%d popup=%lu now=%lu offerSeq=%u\n",
        peerVisible, g_peerWasVisible, (int)g_peerNeedGreeting,
        g_popupUntilMs, nowMs, g_lastPeerOfferSeq);
    }
  }
  if (!peerVisible) {
    g_peerWasVisible = false;
    g_peerNeedGreeting = false;
    g_lastPeerOfferSeq = 0;
    g_lastPeerOfferText[0] = '\0';
    g_lastPeerOfferUntilMs = 0;
    return;
  }

  if (g_lastPeerOfferSeq && g_lastPeerOfferUntilMs && nowMs > g_lastPeerOfferUntilMs) {
    g_lastPeerOfferSeq = 0;
    g_lastPeerOfferText[0] = '\0';
    g_lastPeerOfferUntilMs = 0;
  }

  // Track first detection with a persistent flag so the popup guard can't
  // swallow justDetected — g_peerNeedGreeting stays true until the greeting
  // bubble is actually shown (or the peer disappears).
  if (!g_peerWasVisible) g_peerNeedGreeting = true;
  g_peerWasVisible = true;

  // Peer greetings take priority — don't let LLM thought popups block them.
  if (!g_peerNeedGreeting && g_popupUntilMs && nowMs < g_popupUntilMs) {
    return;  // popup active (and no pending peer greeting) — wait
  }

  const bool justDetected = g_peerNeedGreeting;

  // ── Immediate single-speaker bubble on first detection ──────────
  if (justDetected) {
    notePeerEncounter(g_ble.peer.senderId, g_ble.peerName(), true);
    char firstSeen[BLE_PEER_MESSAGE_MAX_TEXT + 1];
    choosePeerOfferText(true, firstSeen, sizeof(firstSeen));
    if (firstSeen[0]) {
      strlcpy(g_popupText, firstSeen, sizeof(g_popupText));
      g_popupPeerText[0] = '\0';
      g_popupDualSpeaker = false;
      g_popupUntilMs = nowMs + 8000;
      g_pendingBeeps = SOUND_NOTIFY_BEEPS;
      g_pendingVibes = VIBRO_NOTIFY_PULSES;
      g_lastBeepMs = 0;
      g_peerNeedGreeting = false;  // consumed
      Serial.printf("[PEER] bubble set: \"%s\" until=%lu\n", firstSeen, g_popupUntilMs);
    }
  }

  if (g_popupUntilMs && nowMs < g_popupUntilMs && !justDetected) {
    return;
  }

  // Don't queue another offer while we're still waiting for a reply.
  if (g_lastPeerOfferSeq) {
    return;
  }
  if (!justDetected && (nowMs - g_lastPeerChatMs) < BLE_PEER_CHAT_INTERVAL_MS) {
    return;
  }

  char localText[BLE_PEER_MESSAGE_MAX_TEXT + 1];
  choosePeerOfferText(justDetected, localText, sizeof(localText));
  if (localText[0] && g_ble.queuePeerMessage(localText)) {
    SocialPeerMemory* peer = getOrCreateSocialPeer(g_ble.peer.senderId, g_ble.peerName());
    if (peer) {
      peer->chats = peer->chats < 9999 ? peer->chats + 1 : 9999;
      peer->affinity = peer->affinity < 100 ? peer->affinity + 1 : 100;
      const char* outgoingGift = socialGiftKindFromText(localText);
      if (outgoingGift) {
        registerGiftGiven(peer, outgoingGift);
      } else {
        saveSocialMemory();
      }
    }
    strlcpy(g_lastPeerOfferText, localText, sizeof(g_lastPeerOfferText));
    g_lastPeerOfferSeq = g_ble.lastQueuedPeerSeq();
    g_lastPeerOfferUntilMs = nowMs + BLE_PEER_MESSAGE_TTL_MS + BLE_PEER_SCAN_INTERVAL_MS;
    g_lastPeerChatMs = nowMs;
    // Notify Canvas platform — gift or wave depending on message content
    {
      const char* bleEvt = socialGiftKindFromText(localText) ? "gift" : "wave";
      sendPlatformHeartbeat(nowMs, bleEvt, localText, g_ble.peerName());
    }
  }
#endif
}

void trimChatLine(const char* src, char* dst, size_t dstLen, size_t maxChars) {
  if (!dst || dstLen == 0) return;
  dst[0] = '\0';
  if (!src || !src[0]) return;

  strlcpy(dst, src, dstLen);
  if (strlen(dst) <= maxChars) return;
  if (maxChars + 1 < dstLen) dst[maxChars] = '\0';

  const size_t len = strlen(dst);
  if (len >= 3) {
    dst[len - 3] = '.';
    dst[len - 2] = '.';
    dst[len - 1] = '.';
  }
}

void updateTargetRoomFromText(const char* text) {
  String msg = String(text ? text : "");
  msg.toLowerCase();

  if (Hun < 30 || msg.indexOf("hungry") >= 0 || msg.indexOf("food") >= 0 || msg.indexOf("eat") >= 0) {
    strlcpy(g_targetRoom, "KITCHEN", sizeof(g_targetRoom));
    return;
  }
  if (Cle < 30 || msg.indexOf("clean") >= 0 || msg.indexOf("dirty") >= 0 || msg.indexOf("wash") >= 0) {
    strlcpy(g_targetRoom, "BATHROOM", sizeof(g_targetRoom));
    return;
  }
  if (Fat < 30 || msg.indexOf("sleep") >= 0 || msg.indexOf("tired") >= 0 || msg.indexOf("rest") >= 0) {
    strlcpy(g_targetRoom, "BEDROOM", sizeof(g_targetRoom));
    return;
  }
  if (msg.indexOf("play") >= 0 || msg.indexOf("game") >= 0 || msg.indexOf("fun") >= 0) {
    strlcpy(g_targetRoom, "PLAYROOM", sizeof(g_targetRoom));
    return;
  }
  strlcpy(g_targetRoom, "LIVING", sizeof(g_targetRoom));
}

const char* decideNextTargetRoomFromNeeds() {
  state_count();

  if (Hun < 85) {
    return "KITCHEN";
  }
  if (Cle < 85) {
    return "BATHROOM";
  }
  if (Fat < 85) {
    return "BEDROOM";
  }
  return "LIVING";
}

void drawAutoRoom(const unsigned short* roomImage, int cursorX, int cursorY) {
  tft.fillScreen(TFT_BLACK);
  pushImageScaled(0, 12, 128, 104, roomImage);
  roomwhitex = cursorX;
  roomwhitey = cursorY;
  tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
}

void showSleepStill(unsigned long holdMs) {
  tft.fillScreen(TFT_BLACK);
  pushImageScaled(0, 0, 128, 128, sablinasleep);
  if (holdMs > 0) {
    delay(holdMs);
  }
}

void playGardenWalkSequence() {
  for (int i = 0; i < 4; ++i) {
    tft.fillScreen(TFT_BLACK);
    pushImageScaled(0, 12, 128, 104, roomblack);
    pushImageScaled(52, 69, 23, 35, gamewalk[i]);
    delay(140);
  }
}

void playGardenExploreSequence() {
  playGardenWalkSequence();
  frame1 = 0;
  for (int i = 0; i < 19; ++i) {
    pushImageScaled(0, 12, 128, 104, gardengif[frame1]);
    frame1++;
    delay(100);
  }
}

void autoPerformKitchenAction() {
  drawAutoRoom(roomred, 96, 23);
  delay(250);

  frame1 = 0;
  pushImageScaled(0, 12, 128, 104, eatgif[frame1]);
  delay(250);
  for (int i = 0; i < 6; ++i) {
    pushImageScaled(0, 12, 128, 104, eatgif[frame1]);
    frame1++;
    delay(220);
  }

  if (Hun <= 85) Hun_mas += 15;
  else if (Hun <= 95) Hun_mas += 5;
  else if (Hun <= 98) Hun_mas += 2;

  drawAutoRoom(roomred, 96, 23);
}

void autoPerformBedroomAction() {
  drawAutoRoom(roomgray, 100, 102);
  delay(250);

  frame1 = 0;
  pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
  delay(250);
  for (int i = 0; i < 6; ++i) {
    pushImageScaled(0, 12, 128, 104, sleepgif[frame1]);
    frame1++;
    delay(220);
  }

  showSleepStill(350);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Z", 60, 60, 4);
  delay(250);
  tft.drawString("z", 75, 45, 2);
  delay(250);
  tft.drawString("z", 83, 37, 2);
  delay(250);

  if (Fat <= 90) Fat_mas += 10;
  else if (Fat <= 98) Fat_mas += 2;

  drawAutoRoom(roomgray, 100, 102);
}

void autoPerformBathroomAction() {
  drawAutoRoom(roomblue, 20, 27);
  delay(250);

  if (Cle <= 90) {
    frame1 = 0;
    pushImageScaled(0, 12, 128, 104, cleangif[frame1]);
    delay(250);
    for (int i = 0; i < 15; ++i) {
      pushImageScaled(0, 12, 128, 104, cleangif[frame1]);
      frame1++;
      delay(160);
    }
    Cle_mas += 50;
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Already clean", 16, 50, 2);
    delay(700);
  }

  drawAutoRoom(roomblue, 20, 27);
}

void autoPerformPlayroomAction() {
  // Brief intro animation using the game-walk sprite
  drawAutoRoom(roomgray, 45, 16);
  delay(250);
  frame1 = 0;
  for (int i = 0; i < 7; ++i) {
    pushImageScaled(0, 12, 128, 104, gamegif[frame1 % 7]);
    frame1++;
    delay(180);
  }
  // Launch the mini-game
  runMiniGame(millis());
  s_traitNavFlag = true;   // signal activity for trait tick
}

void autoNavigateToTargetRoom(unsigned long nowMs) {
  if (strcmp(g_targetRoom, "LIVING") == 0) return;
  if ((nowMs - g_lastAutoRoomMs) < 5000) return;

  g_lastAutoRoomMs = nowMs;
  g_navActive = true;

  for (int hop = 0; hop < 6; ++hop) {
    state_count();

    if (strcmp(g_targetRoom, "KITCHEN") == 0) {
      if (Hun < 85) {
        playGardenWalkSequence();
        autoPerformKitchenAction();
        state_count();
      }
    } else if (strcmp(g_targetRoom, "BEDROOM") == 0) {
      if (Fat < 85) {
        playGardenWalkSequence();
        autoPerformBedroomAction();
        state_count();
      }
    } else if (strcmp(g_targetRoom, "BATHROOM") == 0) {
      if (Cle < 85) {
        playGardenWalkSequence();
        autoPerformBathroomAction();
        state_count();
      }
    } else if (strcmp(g_targetRoom, "PLAYROOM") == 0) {
      playGardenWalkSequence();
      autoPerformPlayroomAction();
      state_count();
    } else {
      strlcpy(g_targetRoom, "LIVING", sizeof(g_targetRoom));
      break;
    }

    const char* nextRoom = decideNextTargetRoomFromNeeds();
    if (strcmp(g_targetRoom, "PLAYROOM") == 0 && strcmp(nextRoom, "LIVING") == 0) {
      strlcpy(g_targetRoom, "LIVING", sizeof(g_targetRoom));
      break;
    }

    if (strcmp(nextRoom, g_targetRoom) == 0) {
      if ((strcmp(g_targetRoom, "KITCHEN") == 0 && Hun >= 85) ||
          (strcmp(g_targetRoom, "BEDROOM") == 0 && Fat >= 85) ||
          (strcmp(g_targetRoom, "BATHROOM") == 0 && Cle >= 85)) {
        strlcpy(g_targetRoom, "LIVING", sizeof(g_targetRoom));
        break;
      }
    } else {
      strlcpy(g_targetRoom, nextRoom, sizeof(g_targetRoom));
      if (strcmp(g_targetRoom, "LIVING") == 0) {
        break;
      }
      delay(250);
    }
  }

  // Back home: clear screen and reset idle animation so it starts fresh
  g_navActive    = false;
  s_traitNavFlag = true;   // pet moved → activity trait boost
  if (strcmp(g_targetRoom, "LIVING") == 0) {
    tft.fillScreen(TFT_BLACK);
    frame1 = 0;
    idleFrame = 0;
    g_idleFirstFrame = true;
    if (g_iconsShown) drawIconsOnly();
  }
}

void drawFloatingMessage(unsigned long nowMs) {
  // Sprite-based bubble: render content into RAM once (on change), push atomically
  // every frame via pushSprite() — single SPI burst, zero intermediate states = no flicker.
  static TFT_eSprite s_spr = TFT_eSprite(&tft);
  static bool  s_sprAllocated   = false;
  static int   s_sprW = 0, s_sprH = 0;
  static bool  g_bubbleDrawn    = false;
  static int   g_bubbleErasY    = 0;
  static int   g_bubbleErasH    = 0;
  static char  s_cacheLocal[192] = "";
  static char  s_cachePeer[192]  = "";
  static int   s_cacheBy         = -1;
  static bool  s_cacheDual       = false;

  bool active = (g_popupUntilMs != 0 && nowMs <= g_popupUntilMs && g_popupText[0]);

  if (!active) {
    if (g_bubbleDrawn) {
      if (s_sprAllocated) { s_spr.deleteSprite(); s_sprAllocated = false; }
      tft.resetViewport();
      tft.fillRect(0, g_bubbleErasY, SIDEBAR_X, g_bubbleErasH, TFT_BLACK);
      if (g_iconsShown) drawIconsOnly();
      tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
      g_bubbleDrawn = false;
      s_cacheLocal[0] = '\0'; s_cachePeer[0] = '\0'; s_cacheBy = -1;
    }
    g_bubbleDirty = false;
    return;
  }

  const bool dualSpeaker = g_popupDualSpeaker && g_popupPeerText[0];
  const int  bh = dualSpeaker ? 50 : 26;
  const int  bx = 4;
  const int  maxBw = SIDEBAR_X - bx - 4;
  int bw;
  if (dualSpeaker) {
    const int w1 = (int)min(strlen(g_llm.traits.name), (size_t)8)  + 2 + (int)min(strlen(g_popupText),     (size_t)25);
    const int w2 = (int)min(strlen(g_popupPeerName),   (size_t)10) + 2 + (int)min(strlen(g_popupPeerText), (size_t)24);
    bw = max(80, min(max(w1, w2) * 6 + 20, maxBw));
  } else {
    bw = max(50, min((int)min(strlen(g_popupText), (size_t)38) * 6 + 16, maxBw));
  }

  int by;
  if (g_iconsShown) {
    by = GAME_Y + (int32_t)32 * GAME_H / 128 + 2;
  } else {
    by = SCREEN_H - bh - 4;
  }

  const bool contentChanged =
    strcmp(s_cacheLocal, g_popupText)     != 0 ||
    strcmp(s_cachePeer,  g_popupPeerText) != 0 ||
    s_cacheBy   != by                          ||
    s_cacheDual != dualSpeaker;

  // Re-render sprite only when content/position changes (rare).
  if (contentChanged || !s_sprAllocated || s_sprW != bw || s_sprH != bh) {
    if (s_sprAllocated) { s_spr.deleteSprite(); s_sprAllocated = false; }
    s_spr.setColorDepth(16);
    s_spr.setSwapBytes(true);
    if (s_spr.createSprite(bw, bh)) {
      s_sprAllocated = true;
      s_sprW = bw; s_sprH = bh;

      if (dualSpeaker) {
        char localLabel[24], peerLabel[24], localLine[56], peerLine[56];
        trimChatLine(g_llm.traits.name, localLabel, sizeof(localLabel), 8);
        trimChatLine(g_popupPeerName,   peerLabel,  sizeof(peerLabel),  10);
        trimChatLine(g_popupText,       localLine,  sizeof(localLine),  25);
        trimChatLine(g_popupPeerText,   peerLine,   sizeof(peerLine),   24);

        s_spr.fillSprite(TFT_BLACK);
        s_spr.drawRoundRect(0, 0, bw, bh, 8, CHAT_BUBBLE_FRAME_COLOR);
        s_spr.fillRoundRect(4, 4,  bw - 8, 17, 5, CHAT_LOCAL_BG_COLOR);
        s_spr.fillRoundRect(4, 28, bw - 8, 17, 5, CHAT_PEER_BG_COLOR);
        s_spr.setTextSize(1);
        s_spr.setTextColor(CHAT_LOCAL_TEXT_COLOR, CHAT_LOCAL_BG_COLOR);
        s_spr.setCursor(8, 10);
        s_spr.print(localLabel); s_spr.print(": "); s_spr.print(localLine);
        s_spr.setTextColor(CHAT_PEER_TEXT_COLOR, CHAT_PEER_BG_COLOR);
        s_spr.setCursor(8, 34);
        s_spr.print(peerLabel); s_spr.print(": "); s_spr.print(peerLine);
      } else {
        char line[48];
        trimChatLine(g_popupText, line, sizeof(line), 38);
        s_spr.fillRoundRect(0, 0, bw, bh, 6, CHAT_LOCAL_BG_COLOR);
        s_spr.drawRoundRect(0, 0, bw, bh, 6, CHAT_BUBBLE_FRAME_COLOR);
        s_spr.setTextSize(1);
        s_spr.setTextColor(CHAT_LOCAL_TEXT_COLOR, CHAT_LOCAL_BG_COLOR);
        s_spr.setCursor(6, 8);
        s_spr.print(line);
      }
    }

    // Erase old position if moved
    if (g_bubbleDrawn && by != g_bubbleErasY) {
      tft.resetViewport();
      tft.fillRect(0, g_bubbleErasY, SIDEBAR_X, g_bubbleErasH, TFT_BLACK);
    }

    strlcpy(s_cacheLocal, g_popupText,     sizeof(s_cacheLocal));
    strlcpy(s_cachePeer,  g_popupPeerText, sizeof(s_cachePeer));
    s_cacheBy   = by;
    s_cacheDual = dualSpeaker;
    g_bubbleDrawn = true;
    g_bubbleErasY = by;
    g_bubbleErasH = bh;
  }

  // Push sprite to screen every frame when dirty (single atomic SPI burst = no flicker).
  if (s_sprAllocated && (g_bubbleDirty || contentChanged)) {
    g_bubbleDirty = false;
    tft.resetViewport();
    s_spr.pushSprite(bx, by);
    tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
  }
}

void processSoundNotifications(unsigned long nowMs) {
#if FEATURE_SOUND
  if (g_pendingBeeps == 0) return;
  if (g_lastBeepMs != 0 && (nowMs - g_lastBeepMs) < 180) return;
  g_lastBeepMs = nowMs;

  // Hook point for future flat-speaker + mini amp integration.
  // With SPEAKER_PIN still unset, we only mark the event in serial.
  if (SPEAKER_PIN >= 0) {
    // TODO: add non-blocking beep implementation once final audio driver/pin is defined.
    Serial.println("[SOUND] beep");
  } else {
    Serial.println("[SOUND] beep (hook only, speaker pin not configured)");
  }
  g_pendingBeeps--;
#endif
}

void processVibrationNotifications(unsigned long nowMs) {
#if FEATURE_VIBRATION
  if (VIBRO_PIN < 0) {
    if (g_pendingVibes > 0) {
      Serial.println("[VIBRO] pulse (hook only, vibration pin not configured)");
      g_pendingVibes = 0;
    }
    return;
  }

  if (g_vibeActive) {
    if (nowMs - g_vibeToggleMs >= VIBRO_PULSE_MS) {
      digitalWrite(VIBRO_PIN, LOW);
      g_vibeActive = false;
      g_vibeToggleMs = nowMs;
    }
    return;
  }

  if (g_pendingVibes == 0) return;

  // Keep a small gap between pulses.
  if (nowMs - g_vibeToggleMs < 120) return;

  digitalWrite(VIBRO_PIN, HIGH);
  g_vibeActive = true;
  g_vibeToggleMs = nowMs;
  g_pendingVibes--;
#endif
}

// ════════════════════════════════════════════════════════════════════
//  RGB LED — multi-signal colour engine
//
//  Priority (highest first):
//   1. Pet dead             → off (no glow)
//   2. Timed event flash    → set by triggerRgbEvent() calls below
//   3. Sick / critical stat → red fast-blink
//   4. Night mode           → dim indigo pulse
//   5. BLE peer visible     → cyan soft-pulse
//   6. Stress trait high    → orange overlay
//   7. Base LLM mood colour → modified by curiosity/activity traits
// ════════════════════════════════════════════════════════════════════

// Call this from any event handler to flash a colour for durationMs.
void triggerRgbEvent(uint8_t r, uint8_t g, uint8_t b, unsigned long durationMs) {
  g_rgbEventColor   = g_rgb.Color(r, g, b);
  g_rgbEventUntilMs = millis() + durationMs;
}

void updateRgbMood() {
  unsigned long now = millis();

  // 1. Dead → off
  if (!g_petAlive) {
    g_rgb.setPixelColor(0, 0);
    g_rgb.show();
    return;
  }

  // 2. Timed event flash (BLE detect, game win/lose, WiFi found, etc.)
  if (g_rgbEventColor && now < g_rgbEventUntilMs) {
    g_rgb.setPixelColor(0, g_rgbEventColor);
    g_rgb.show();
    return;
  }
  g_rgbEventColor = 0;

  // 3. Sick or critical stat → red fast-blink
  if (g_petSick || Hun < 20 || Fat < 20 || Cle < 20) {
    uint32_t col = g_rgbPulsePhase ? g_rgb.Color(255, 0, 0) : 0;
    g_rgb.setPixelColor(0, col);
    g_rgb.show();
    return;
  }

  // 4. Night mode → slow indigo pulse
  if (g_isNight) {
    uint32_t col = g_rgbPulsePhase
      ? g_rgb.Color(40, 0, 90)    // dim indigo on
      : g_rgb.Color(10, 0, 30);   // dim indigo off
    g_rgb.setPixelColor(0, col);
    g_rgb.show();
    return;
  }

  // 5–7. Trait-modified mood colour
  uint8_t mr, mg, mb;
  g_llm.getMoodColor(mr, mg, mb);

  // Curiosity (0-100) → boosts blue channel (wonder/alert)
  int blueBoost = (int)g_traits.curiosity * 60 / 100;   // 0–60
  // Activity (0-100) → boosts green channel (energy)
  int greenBoost = (int)g_traits.activity * 50 / 100;   // 0–50
  // Stress (0-100) → boosts red channel, suppresses others
  int redBoost  = (int)g_traits.stress * 80 / 100;      // 0–80

  // Apply boosts with clamping
  int r = min(255, (int)mr + redBoost - (redBoost / 3));
  int g = min(255, (int)mg + greenBoost);
  int b = min(255, (int)mb + blueBoost);

  // Reduce green/blue when stress is very high (pet looks distressed)
  if (g_traits.stress > 70) {
    g = max(0, g - (int)(g_traits.stress - 70) * 2);
    b = max(0, b - (int)(g_traits.stress - 70) * 2);
  }

  // 5. BLE peer visible → pulse toward cyan
  if (g_ble.peerVisible()) {
    if (g_rgbPulsePhase) {
      r = max(0, r - 60);
      g = min(255, g + 80);
      b = min(255, b + 80);
    }
  }

  // Dim LED when dark mode is active
  if (g_darkModeActive || g_blNightDimmed) {
    r = r / 4;
    g = g / 4;
    b = b / 4;
  }

  g_rgb.setPixelColor(0, g_rgb.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));
  g_rgb.show();
}

// ── Handle commands sent over BLE ("feed", "clean", "sleep", "play") ─
void handleBleCmdIfAny() {
  if (!g_bleCmd.pending) return;
  unsigned long now = millis();
  g_bleCmd.pending = false;
  const char* cmd = (const char*)g_bleCmd.cmd;

  if (strcmp(cmd, "feed") == 0) {
    strlcpy(g_targetRoom, "KITCHEN", sizeof(g_targetRoom));
    if (Hun <= 85) { Hun_mas += 15; g_llm.reactToEvent("eat", Hun, Fat, Cle); }
  } else if (strcmp(cmd, "clean") == 0) {
    strlcpy(g_targetRoom, "BATHROOM", sizeof(g_targetRoom));
    if (Cle <= 90) { Cle_mas += 10; g_llm.reactToEvent("clean", Hun, Fat, Cle); }
  } else if (strcmp(cmd, "sleep") == 0) {
    strlcpy(g_targetRoom, "BEDROOM", sizeof(g_targetRoom));
    if (Fat <= 90) { Fat_mas += 10; g_llm.reactToEvent("sleep", Hun, Fat, Cle); }
  } else if (strcmp(cmd, "play") == 0) {
    strlcpy(g_targetRoom, "PLAYROOM", sizeof(g_targetRoom));
    g_llm.reactToEvent("play", Hun, Fat, Cle);
  } else if (strcmp(cmd, "pet") == 0) {
    strlcpy(g_targetRoom, "LIVING", sizeof(g_targetRoom));
    g_llm.reactToEvent("petted", Hun, Fat, Cle);
  } else if (strcmp(cmd, "heal") == 0) {
    strlcpy(g_targetRoom, "BATHROOM", sizeof(g_targetRoom));
    if (Cle <= 95) Cle_mas += 5;
    g_llm.reactToEvent("heal", Hun, Fat, Cle);
  } else if (strcmp(cmd, "wake") == 0) {
    strlcpy(g_targetRoom, "BEDROOM", sizeof(g_targetRoom));
    g_llm.reactToEvent("wake", Hun, Fat, Cle);
  }
#if FEATURE_WIFI_AUDIT
  // ── WiFi Audit BLE commands ───────────────────────────────────
  else if (strcmp(cmd, "audit_scan") == 0) {
    g_wifiAudit.startScan();
    g_auditScreenActive = true;
    triggerFloatingMessage("WiFi scan started...", now);
  } else if (strcmp(cmd, "audit_monitor") == 0) {
    g_wifiAudit.startPassiveMonitor();
    g_auditScreenActive = true;
    triggerFloatingMessage("Packet monitor active", now);
  } else if (strcmp(cmd, "audit_deauth") == 0) {
    if (g_wifiAudit.apCount > g_auditSelectedAP) {
      g_wifiAudit.startDeauth(g_wifiAudit.aps[g_auditSelectedAP].bssid);
      char msg[64]; snprintf(msg, sizeof(msg), "Deauth: %s", g_wifiAudit.aps[g_auditSelectedAP].ssid);
      triggerFloatingMessage(msg, now);
    }
  } else if (strcmp(cmd, "audit_handshake") == 0) {
    if (g_wifiAudit.apCount > g_auditSelectedAP) {
      g_wifiAudit.startHandshakeCapture(g_wifiAudit.aps[g_auditSelectedAP].bssid);
      triggerFloatingMessage("Handshake capture...", now);
    }
  } else if (strcmp(cmd, "audit_pmkid") == 0) {
    g_wifiAudit.startPMKIDCapture();
    g_auditScreenActive = true;
    triggerFloatingMessage("PMKID capture active", now);
  } else if (strcmp(cmd, "audit_stop") == 0) {
    g_wifiAudit.stopMonitor();
    g_auditScreenActive = false;
    triggerFloatingMessage("Audit stopped", now);
  } else if (strcmp(cmd, "audit_next") == 0) {
    if (g_wifiAudit.apCount > 0) {
      g_auditSelectedAP = (g_auditSelectedAP + 1) % g_wifiAudit.apCount;
    }
  }
#endif
  autoNavigateToTargetRoom(now);
  g_prefs.putInt(NVS_HUN_MAS, Hun_mas);
  g_prefs.putInt(NVS_FAT_MAS, Fat_mas);
  g_prefs.putInt(NVS_CLE_MAS, Cle_mas);
  g_prefs.putInt(NVS_EXP_MAS, Exp_mas);
  persistLifecycle();
}

// ════════════════════════════════════════════════════════════════════
//  WiFi Audit – On-screen display
// ════════════════════════════════════════════════════════════════════
#if FEATURE_WIFI_AUDIT

void drawAuditScreen() {
  tft.resetViewport();
  tft.fillRect(GAME_X, GAME_Y, g_gameW, GAME_H, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  int y = GAME_Y + 2;

  // Header
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  char hdr[40];
  snprintf(hdr, sizeof(hdr), "[%s] CH:%d", g_wifiAudit.modeStr(), g_wifiAudit.currentChannel);
  tft.drawString(hdr, GAME_X + 2, y);
  y += 10;

  // Stats line
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  char stats[48];
  snprintf(stats, sizeof(stats), "PKT:%lu EAPOL:%lu DEAUTH:%lu",
           g_wifiAudit.totalPackets, g_wifiAudit.eapolPackets, g_wifiAudit.deauthsSent);
  tft.drawString(stats, GAME_X + 2, y);
  y += 12;

  // AP list
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("SSID             CH  ENC  CLI HS", GAME_X + 2, y);
  y += 10;

  int maxVisible = 8;
  for (int i = 0; i < g_wifiAudit.apCount && i < maxVisible; i++) {
    WifiAuditAP& ap = g_wifiAudit.aps[i];
    bool selected = (i == g_auditSelectedAP);

    tft.setTextColor(selected ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
    char line[48];
    char ssidShort[13]; strncpy(ssidShort, ap.ssid, 12); ssidShort[12] = '\0';
    snprintf(line, sizeof(line), "%s%-12s %2d %4s %3d %s%s",
             selected ? ">" : " ",
             ssidShort, ap.channel,
             g_wifiAudit.encryptionStr(ap.encryption),
             ap.clientCount,
             ap.handshakeCaptured ? "H" : "-",
             ap.pmkidCaptured ? "P" : "-");
    tft.drawString(line, GAME_X + 2, y);
    y += 10;
  }

  // Handshake count
  uint8_t hs = g_wifiAudit.getHandshakeCompleteCount();
  if (hs > 0) {
    y = GAME_Y + GAME_H - 12;
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    char hsLine[32];
    snprintf(hsLine, sizeof(hsLine), "HANDSHAKES: %d  PMKID: %d", hs, g_wifiAudit.pmkidCount);
    tft.drawString(hsLine, GAME_X + 2, y);
  }

  tft.setViewport(GAME_X, GAME_Y, g_gameW, GAME_H);
}

#endif
