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

// ── New hardware objects ─────────────────────────────────────────
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
bool          g_wifiEnabled  = false;
unsigned long g_lastBleNotify    = 0;
unsigned long g_lastSidebarDraw  = 0;
unsigned long g_lastRgbUpdate    = 0;
char          g_popupText[192]   = "";
char          g_popupPeerText[96]= "";
char          g_popupPeerName[24]= "";
unsigned long g_popupUntilMs     = 0;
unsigned long g_lastPeerChatMs   = 0;
bool          g_popupDualSpeaker = false;
bool          g_peerWasVisible   = false;
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

int frame1 = 0;
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

  // Keep both PWM and direct digital drive to maximize compatibility
  // with board revisions/clones that wire BL differently.
  ledcWrite(TFT_BL_PIN, level);
  digitalWrite(TFT_BL_PIN, level > (maxLevel / 2) ? HIGH : LOW);

#if TFT_BL_PIN_ALT >= 0
  ledcWrite(TFT_BL_PIN_ALT, level);
  digitalWrite(TFT_BL_PIN_ALT, level > (maxLevel / 2) ? HIGH : LOW);
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

void mainicon()
{
  tft.fillScreen(TFT_BLACK);
  if(Hun < 60 || Fat < 60 || Cle < 60)
  {
    tft.pushImage(2, 1, 28, 28, warn);
  }
  else
  {
    tft.pushImage(2, 1, 28, 28, pest);
  }
  tft.pushImage(34, 1, 29, 29, weber);
  tft.pushImage(66, 1, 29, 29, baby);
  tft.pushImage(98, 1, 29, 29, freg);
  tft.pushImage(2, 99, 29, 29, shop);
  tft.pushImage(34, 99, 29, 29, door);
  tft.pushImage(66, 99, 29, 29, box);
  tft.pushImage(98, 99, 29, 29, computer);
}

void Generalmenu()
{
  if(digitalRead(BTN_A_PIN) == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    while(digitalRead(BTN_A_PIN) == 1 && digitalRead(BTN_B_PIN) == 1)
    {
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
  if(digitalRead(BTN_A_PIN) == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    food_page = 0;
    tft.pushImage(23, 23, 29, 29, f01);
    tft.pushImage(75, 23, 29, 29, f02);
    tft.pushImage(23, 75, 29, 29, f03);
    tft.pushImage(75, 75, 29, 29, f22);

    Hun_s = Hun; //把饥饿值Hun赋值给判断用的Hun_s
    tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
    while(!(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 74))
    {
      if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 22 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 74 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 22 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 74 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 -= n_food;
        if(food_page < 6)
        {
          food_page += 1;
          if(food_page == 1)
          {
            tft.pushImage(23, 23, 29, 29, f04);
            tft.pushImage(75, 23, 29, 29, f05);
            tft.pushImage(23, 75, 29, 29, f06);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 2)
          {
            tft.pushImage(23, 23, 29, 29, f07);
            tft.pushImage(75, 23, 29, 29, f08);
            tft.pushImage(23, 75, 29, 29, f09);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 3)
          {
            tft.pushImage(23, 23, 29, 29, f10);
            tft.pushImage(75, 23, 29, 29, f11);
            tft.pushImage(23, 75, 29, 29, f12);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 4)
          {
            tft.pushImage(23, 23, 29, 29, f13);
            tft.pushImage(75, 23, 29, 29, f14);
            tft.pushImage(23, 75, 29, 29, f15);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 5)
          {
            tft.pushImage(23, 23, 29, 29, f16);
            tft.pushImage(75, 23, 29, 29, f17);
            tft.pushImage(23, 75, 29, 29, f18);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 6)
          {
            tft.pushImage(23, 23, 29, 29, f19);
            tft.pushImage(75, 23, 29, 29, f20);
            tft.pushImage(23, 75, 29, 29, f21);
            tft.pushImage(75, 75, 29, 29, f22);
          }

        }
        else if(food_page >= 6)
        {
          food_page = 0;
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
        }
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
      
      
      //以下为点击确认键（0）时的反应 00
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f01);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f02);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 0)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f03);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f04);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f04);
          tft.pushImage(75, 23, 29, 29, f05);
          tft.pushImage(23, 75, 29, 29, f06);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f05);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f04);
          tft.pushImage(75, 23, 29, 29, f05);
          tft.pushImage(23, 75, 29, 29, f06);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 1)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f06);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f04);
          tft.pushImage(75, 23, 29, 29, f05);
          tft.pushImage(23, 75, 29, 29, f06);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f07);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f07);
          tft.pushImage(75, 23, 29, 29, f08);
          tft.pushImage(23, 75, 29, 29, f09);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f08);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f07);
          tft.pushImage(75, 23, 29, 29, f08);
          tft.pushImage(23, 75, 29, 29, f09);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 2)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f09);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f07);
          tft.pushImage(75, 23, 29, 29, f08);
          tft.pushImage(23, 75, 29, 29, f09);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f10);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f10);
          tft.pushImage(75, 23, 29, 29, f11);
          tft.pushImage(23, 75, 29, 29, f12);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f11);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f10);
          tft.pushImage(75, 23, 29, 29, f11);
          tft.pushImage(23, 75, 29, 29, f12);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 3)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f12);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f10);
          tft.pushImage(75, 23, 29, 29, f11);
          tft.pushImage(23, 75, 29, 29, f12);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f13);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f13);
          tft.pushImage(75, 23, 29, 29, f14);
          tft.pushImage(23, 75, 29, 29, f15);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f14);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f13);
          tft.pushImage(75, 23, 29, 29, f14);
          tft.pushImage(23, 75, 29, 29, f15);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 4)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f15);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f13);
          tft.pushImage(75, 23, 29, 29, f14);
          tft.pushImage(23, 75, 29, 29, f15);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f16);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f16);
          tft.pushImage(75, 23, 29, 29, f17);
          tft.pushImage(23, 75, 29, 29, f18);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f17);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f16);
          tft.pushImage(75, 23, 29, 29, f17);
          tft.pushImage(23, 75, 29, 29, f18);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 5)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f18);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f16);
          tft.pushImage(75, 23, 29, 29, f17);
          tft.pushImage(23, 75, 29, 29, f18);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f19);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f19);
          tft.pushImage(75, 23, 29, 29, f20);
          tft.pushImage(23, 75, 29, 29, f21);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f20);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f19);
          tft.pushImage(75, 23, 29, 29, f20);
          tft.pushImage(23, 75, 29, 29, f21);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 6)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f21);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f19);
          tft.pushImage(75, 23, 29, 29, f20);
          tft.pushImage(23, 75, 29, 29, f21);
          tft.pushImage(75, 75, 29, 29, f22);
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
  if(millis() % 280 == 0 && darkmode == 0)
  {
    tft.pushImage(0, 34, 128, 60, forest1[frame1]);
    frame1 += 1;
    if(frame1 >= 75)
    {
      frame1 = 0;
    }
  }

  else if(millis() % 170 == 0 && darkmode == 1)
  {
    tft.pushImage(0, 35, 128, 58, forest2[frame1]);
    frame1 += 1;
    if(frame1 >= 20)
    {
      frame1 = 0;
    }
  }
}

void Sleep()
{
  if(digitalRead(BTN_A_PIN) == 0)
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
      if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Exit", 10, 15, 2);
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 = 13;
        sleep_page += 1;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }

      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 13 && sleep_page == 1)
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 13 && sleep_page == 0)
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 38 && sleep_page == 0)
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 63 && sleep_page == 0)
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 88 && sleep_page == 0)
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 13 && sleep_page == 1)
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
  if(digitalRead(BTN_A_PIN) == 0)
  {
    delay(300);
    frame1 = 0;
    tft.pushImage(0, 12, 128, 104, eatgif[frame1]);
    delay(300);
    for(int i = 0; i < 6; ++i)
    {
      tft.pushImage(0, 12, 128, 104, eatgif[frame1]);
      frame1++;
      delay(300);
    }
    tft.fillScreen(TFT_BLACK);
    food_page = 0;
    tft.pushImage(23, 23, 29, 29, f01);
    tft.pushImage(75, 23, 29, 29, f02);
    tft.pushImage(23, 75, 29, 29, f03);
    tft.pushImage(75, 75, 29, 29, f22);

    Hun_s = Hun; //把饥饿值Hun赋值给判断用的Hun_s
    tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
    while(!(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 74))
    {
      if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 22 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 74 && foody1 == 22)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 22 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 += n_food;
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && foodx1 == 74 && foody1 == 74)
      {
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_BLACK);
        foodx1 -= n_food;
        foody1 -= n_food;
        if(food_page < 6)
        {
          food_page += 1;
          if(food_page == 1)
          {
            tft.pushImage(23, 23, 29, 29, f04);
            tft.pushImage(75, 23, 29, 29, f05);
            tft.pushImage(23, 75, 29, 29, f06);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 2)
          {
            tft.pushImage(23, 23, 29, 29, f07);
            tft.pushImage(75, 23, 29, 29, f08);
            tft.pushImage(23, 75, 29, 29, f09);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 3)
          {
            tft.pushImage(23, 23, 29, 29, f10);
            tft.pushImage(75, 23, 29, 29, f11);
            tft.pushImage(23, 75, 29, 29, f12);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 4)
          {
            tft.pushImage(23, 23, 29, 29, f13);
            tft.pushImage(75, 23, 29, 29, f14);
            tft.pushImage(23, 75, 29, 29, f15);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 5)
          {
            tft.pushImage(23, 23, 29, 29, f16);
            tft.pushImage(75, 23, 29, 29, f17);
            tft.pushImage(23, 75, 29, 29, f18);
            tft.pushImage(75, 75, 29, 29, f22);
          }

          else if(food_page == 6)
          {
            tft.pushImage(23, 23, 29, 29, f19);
            tft.pushImage(75, 23, 29, 29, f20);
            tft.pushImage(23, 75, 29, 29, f21);
            tft.pushImage(75, 75, 29, 29, f22);
          }

        }
        else if(food_page >= 6)
        {
          food_page = 0;
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
        }
        tft.drawRect(foodx1, foody1, foodx2, foody2, TFT_WHITE);
        delay(300);
      }
      
      
      //以下为点击确认键（0）时的反应 00
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f01);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f01);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 0)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f02);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f02);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 0)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f03);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f03);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f01);
          tft.pushImage(75, 23, 29, 29, f02);
          tft.pushImage(23, 75, 29, 29, f03);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f04);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f04);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f04);
          tft.pushImage(75, 23, 29, 29, f05);
          tft.pushImage(23, 75, 29, 29, f06);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 1)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f05);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f05);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f04);
          tft.pushImage(75, 23, 29, 29, f05);
          tft.pushImage(23, 75, 29, 29, f06);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 1)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f06);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f06);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f04);
          tft.pushImage(75, 23, 29, 29, f05);
          tft.pushImage(23, 75, 29, 29, f06);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f07);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f07);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f07);
          tft.pushImage(75, 23, 29, 29, f08);
          tft.pushImage(23, 75, 29, 29, f09);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 2)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f08);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f08);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f07);
          tft.pushImage(75, 23, 29, 29, f08);
          tft.pushImage(23, 75, 29, 29, f09);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 2)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f09);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f09);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f07);
          tft.pushImage(75, 23, 29, 29, f08);
          tft.pushImage(23, 75, 29, 29, f09);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f10);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f10);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f10);
          tft.pushImage(75, 23, 29, 29, f11);
          tft.pushImage(23, 75, 29, 29, f12);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 3)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f11);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f11);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f10);
          tft.pushImage(75, 23, 29, 29, f11);
          tft.pushImage(23, 75, 29, 29, f12);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 3)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f12);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f12);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f10);
          tft.pushImage(75, 23, 29, 29, f11);
          tft.pushImage(23, 75, 29, 29, f12);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f13);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f13);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f13);
          tft.pushImage(75, 23, 29, 29, f14);
          tft.pushImage(23, 75, 29, 29, f15);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 4)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f14);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f14);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f13);
          tft.pushImage(75, 23, 29, 29, f14);
          tft.pushImage(23, 75, 29, 29, f15);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 4)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f15);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f15);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f13);
          tft.pushImage(75, 23, 29, 29, f14);
          tft.pushImage(23, 75, 29, 29, f15);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f16);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f16);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f16);
          tft.pushImage(75, 23, 29, 29, f17);
          tft.pushImage(23, 75, 29, 29, f18);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 5)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f17);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f17);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f16);
          tft.pushImage(75, 23, 29, 29, f17);
          tft.pushImage(23, 75, 29, 29, f18);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 5)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f18);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f18);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f16);
          tft.pushImage(75, 23, 29, 29, f17);
          tft.pushImage(23, 75, 29, 29, f18);
          tft.pushImage(75, 75, 29, 29, f22);
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
      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 85)
        {
          Hun_mas += 15;
          Hun_s += 15; //在饥饿值增加的同时，判断用的饥饿值也增加

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f19);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f19);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f19);
          tft.pushImage(75, 23, 29, 29, f20);
          tft.pushImage(23, 75, 29, 29, f21);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 74 && foody1 == 22 && food_page == 6)
      {
        if(Hun_s <= 95)
        {
          Hun_mas += 5;
          Hun_s += 5;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f20);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f20);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f19);
          tft.pushImage(75, 23, 29, 29, f20);
          tft.pushImage(23, 75, 29, 29, f21);
          tft.pushImage(75, 75, 29, 29, f22);
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

      else if(digitalRead(BTN_A_PIN) == 0 && foodx1 == 22 && foody1 == 74 && food_page == 6)
      {
        if(Hun_s <= 98)
        {
          Hun_mas += 2;
          Hun_s += 2;

          //以下为吃东西的动画
          tft.fillScreen(TFT_BLACK);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          tft.pushImage(49, 49, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 49, 29, 29, TFT_BLACK);
          tft.pushImage(49, 74, 29, 29, f21);
          delay(500);
          tft.fillRect(49, 74, 29, 29, TFT_BLACK);
          tft.pushImage(49, 92, 29, 29, f21);
          tft.drawRect(0, 88, 128, 40, TFT_WHITE);
          delay(500);
          tft.pushImage(0, 0, 128, 88, sablinaeat);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat2);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat3);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat4);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat5);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat6);
          delay(200);
          tft.fillRect(49, 92, 29, 29, TFT_BLACK);
          tft.pushImage(0, 0, 128, 88, sablinaeat7);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat8);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat9);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat10);
          delay(200);
          tft.pushImage(0, 0, 128, 88, sablinaeat11);
          delay(200);
          tft.fillScreen(TFT_BLACK);
          tft.pushImage(23, 23, 29, 29, f19);
          tft.pushImage(75, 23, 29, 29, f20);
          tft.pushImage(23, 75, 29, 29, f21);
          tft.pushImage(75, 75, 29, 29, f22);
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
    tft.pushImage(0, 12, 128, 104, roomred);
    roomwhitex = 9;
    roomwhitey = 93;
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
    delay(300);
  }
}

void Roomsleep()
{
  if(digitalRead(BTN_A_PIN) == 0)
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
      if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 += 25;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }
  
      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Exit", 10, 15, 2);
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_BLACK);
        sleepy1 = 13;
        sleep_page += 1;
        tft.drawRect(sleepx1, sleepy1, sleepx2, sleepy2, TFT_WHITE);
        delay(300);
      }

      else if(digitalRead(BTN_B_PIN) == 0 && sleepy1 == 13 && sleep_page == 1)
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 13 && sleep_page == 0)
      {
        frame1 = 0;
        tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 38 && sleep_page == 0)
      {
        frame1 = 0;
        tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 63 && sleep_page == 0)
      {
        frame1 = 0;
        tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 88 && sleep_page == 0)
      {
        frame1 = 0;
        tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
        delay(300);
        for(int i = 0; i < 6; ++i)
        {
          tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
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

      else if(digitalRead(BTN_A_PIN) == 0 && sleepy1 == 13 && sleep_page == 1)
      {
        sleeptime = 600;//假装睡觉，实则退出
        delay(300);
      }
    }
    sleeptime = 0;
    tft.pushImage(0, 12, 128, 104, roomgray);
    roomwhitex = 58;
    roomwhitey = 117;
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
    delay(300);
  }
}

void Roomshower()
{
  if(digitalRead(BTN_A_PIN) == 0)
  {
    delay(300);
    if(Cle <= 90)
    {
      frame1 = 0;
      tft.pushImage(0, 12, 128, 104, cleangif[frame1]);
      delay(300);
      for(int i = 0; i < 15; ++i)
      {
        tft.pushImage(0, 12, 128, 104, cleangif[frame1]);
        frame1++;
        delay(200);
      }
      tft.pushImage(0, 12, 128, 104, cleangif[14]);
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
    tft.pushImage(0, 12, 128, 104, roomblue);
    roomwhitex = 107;
    roomwhitey = 27;
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
    delay(300);
  }
}

void Shower()
{
  if(digitalRead(BTN_A_PIN) == 0)
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
  if(digitalRead(BTN_A_PIN) == 0)
  {
    shopx1 = 101;
    shopy1 = 99;
    shopx2 = 26;
    shopy2 = 18;
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
    tft.drawString("Exit", 103, 100, 2);
    delay(300);
    while(!(shopx1 == 101 && shopy1 == 99 && digitalRead(BTN_A_PIN) == 0))
    {
      if(shopx1 == 0 && shopy1 == 0 && shopx2 == 128 && shopy2 == 100 && digitalRead(BTN_B_PIN) == 0)
      {
        tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
        shopx1 = 101;
        shopy1 = 99;
        shopx2 = 26;
        shopy2 = 18;
        tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
        delay(200);
      }

      else if(shopx1 == 101 && shopy1 == 99 && shopx2 == 26 && shopy2 == 18 && digitalRead(BTN_B_PIN) == 0)
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
          tft.pushImage(1, 1, 126, 83, colorgif[0]);
        }
        break;
    
        case 1:
        {
          tft.pushImage(1, 1, 126, 83, colorgif[1]);
        }
        break;
    
        case 2:
        {
          tft.pushImage(1, 1, 126, 83, colorgif[2]);
        }
        break;

        case 3:
        {
          tft.pushImage(1, 1, 126, 83, colorgif[3]);
        }
        break;

        case 4:
        {
          tft.pushImage(1, 1, 126, 83, colorgif[4]);
        }
        break;

        case 5:
        {
          tft.pushImage(1, 1, 126, 83, colorgif[5]);
        }
        break;
      }

      //以下为确认购买图片
      if(shopx1 == 0 && shopy1 == 0 && shopx2 == 128 && shopy2 == 100 && digitalRead(BTN_A_PIN) == 0)
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
            if(shopx1 == 22 && shopy1 == 86 && digitalRead(BTN_B_PIN) == 0)
            {
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
              shopx1 = 90;
              shopy1 = 86;
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
              delay(300);
            }
    
            else if(shopx1 == 90 && shopy1 == 86 && digitalRead(BTN_B_PIN) == 0)
            {
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_BLACK);
              shopx1 = 22;
              shopy1 = 86;
              tft.drawRect(shopx1, shopy1, shopx2, shopy2, TFT_WHITE);
              delay(300);
            }
  
            else if(shopx1 == 22 && shopy1 == 86 && digitalRead(BTN_A_PIN) == 0)//确认购买Yes
            {
              tft.fillScreen(TFT_BLACK);
              tft.drawString("Success", 40, 50, 2);
              Exp_mas += 100;
              buy_j += 1;
              picture_group[pic] += 1;
              delay(1000);
            }
  
            else if(shopx1 == 90 && shopy1 == 86 && digitalRead(BTN_A_PIN) == 0)//放弃购买No
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
  if(digitalRead(BTN_A_PIN) == 0)
  {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Welcome To The", 17, 40, 2);
    tft.drawString("Collection Box", 22, 60, 2);
    tft.drawString("Exit", 100, 100, 2);
    //tft.drawRect(boxx1, boxy1, boxx2, boxy2, TFT_WHITE);
    delay(300);
    while(digitalRead(BTN_A_PIN) != 0)
    {
      if(digitalRead(BTN_B_PIN) == 0)
      {
        if(picture_group[i] == 1)
        {
          tft.pushImage(1, 1, 126, 83, colorgif[i]);
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
  if(digitalRead(BTN_A_PIN) == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    roomwhitex = 83, roomwhitey = 16;
    tft.pushImage(0, 12, 128, 104, roomwhite);
    tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);//上左
    while(!(roomwhitex == 95 && roomwhitey == 105 && digitalRead(BTN_A_PIN) == 0))
    {
      if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 83)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =107 , roomwhitey = 28;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 107)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =95 , roomwhitey = 105;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 95)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =45 , roomwhitey = 110;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 45)
      {
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        roomwhitex =21 , roomwhitey = 98;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 21)
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
      else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 83)
      {
        tft.pushImage(0, 12, 128, 104, roomgray);
        //tft.drawCircle(45, 16, 2, TFT_WHITE);
        roomwhitex = 58;
        roomwhitey = 117;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 58 && roomwhitey == 117))
        {
          tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
          if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 58 && roomwhitey == 117)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 45;
            roomwhitey = 16;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 45 && roomwhitey == 16)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 100;
            roomwhitey = 102;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 100 && roomwhitey == 102)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 58;
            roomwhitey = 117;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 100 && roomwhitey == 102)
          {
            Roomsleep();
          }
          
          else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 45 && roomwhitey == 16)//////////////////这里（游戏）
          {
            frame1 = 0;
            tft.pushImage(0, 12, 128, 104, gamegif[frame1]);
            delay(300);
            for(int i = 0; i < 7; ++i)
            {
              tft.pushImage(0, 12, 128, 104, gamegif[frame1]);
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

              while(!(gamey1 == 73 && digitalRead(BTN_A_PIN) == 0))
              {
                tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
                if(gamey1 == 23 && digitalRead(BTN_B_PIN) == 0)
                {
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_BLACK);
                  gamey1 = 73;
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
                  delay(300);
                }
            
//                else if(gamey1 == 48 && digitalRead(BTN_B_PIN) == 0)
//                {
//                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_BLACK);
//                  gamey1 += 25;
//                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
//                  delay(300);
//                }
            
                else if(gamey1 == 73 && digitalRead(BTN_B_PIN) == 0)
                {
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_BLACK);
                  gamey1 = 23;
                  tft.drawRect(gamex1, gamey1, gamex2, gamey2, TFT_WHITE);
                  delay(300);
                }

                //以下为确认游戏
                else if(gamey1 == 23 && digitalRead(BTN_A_PIN) == 0)//第一个游戏sic_bo
                {
                  tft.fillScreen(TFT_BLACK);
                  tft.drawString("Sic bo", 30, 30, 4);
                  int gamex = 9;
                  delay(1500);
                  while(!(gamex == 83 && digitalRead(BTN_A_PIN) == 0))
                  {
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
      
                    while(digitalRead(BTN_A_PIN) != 0)
                    {
                      tft.drawString("Big", 18, 90, 2);
                      tft.drawString("Small", 50, 90, 2);
                      tft.drawString("Exit", 90, 90, 2);
                      tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                      if(gamex == 9 && digitalRead(BTN_B_PIN) == 0)
                      {
                        tft.drawRect(gamex, 89, 37, 20, TFT_BLACK);
                        gamex += 37;
                        tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                        delay(300);
                      }
        
                      else if(gamex == 46 && digitalRead(BTN_B_PIN) == 0)
                      {
                        tft.drawRect(gamex, 89, 37, 20, TFT_BLACK);
                        gamex += 37;
                        tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                        delay(300);
                      }
      
                      else if(gamex == 83 && digitalRead(BTN_B_PIN) == 0)
                      {
                        tft.drawRect(gamex, 89, 37, 20, TFT_BLACK);
                        gamex = 9;
                        tft.drawRect(gamex, 89, 37, 20, TFT_WHITE);
                        delay(300);
                      }
                    }
      
                    if(gamex == 9 && digitalRead(BTN_A_PIN) == 0)
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
      
                    else if(gamex == 46 && digitalRead(BTN_A_PIN) == 0)
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

//                else if(gamey1 == 48 && digitalRead(BTN_A_PIN) == 0)//第二个游戏wifi_scan
//                {
//                  delay(300);
//                  int wifitime = 0;
//                  while(!(digitalRead(BTN_A_PIN) == 0 || digitalRead(BTN_B_PIN) == 0))//这个括号可以删掉
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
            tft.pushImage(0, 12, 128, 104, roomgray);
            delay(300);
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        tft.pushImage(0, 12, 128, 104, roomwhite);
        roomwhitex = 83;
        roomwhitey = 16;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 107)
      {

//          roomwhitex = 96;
//          roomwhitey = 23;
//          roomwhitex = 9;
//          roomwhitey = 93;
        tft.pushImage(0, 12, 128, 104, roomred);
        roomwhitex = 9;
        roomwhitey = 93;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 9 && roomwhitey == 93))
        {
          if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 9 && roomwhitey == 93)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 96;
            roomwhitey = 23;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 96 && roomwhitey == 23)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 9;
            roomwhitey = 93;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 96 && roomwhitey == 23)
          {
            Roomfood();
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        tft.pushImage(0, 12, 128, 104, roomwhite);
        roomwhitex = 107;
        roomwhitey = 28;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);     
      }

      else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 45)
      {
//          roomwhitex = 119;
//          roomwhitey = 34;
//          roomwhitex = 49;
//          roomwhitey = 114;
        tft.pushImage(0, 12, 128, 104, roomgreen);
        roomwhitex = 119;
        roomwhitey = 34;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 119 && roomwhitey == 34))
        {
          if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 119 && roomwhitey == 34)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 49;
            roomwhitey = 114;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 49 && roomwhitey == 114)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 119;
            roomwhitey = 34;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 49 && roomwhitey == 114)
          {
            delay(300);
            playGardenExploreSequence();
            while(digitalRead(BTN_A_PIN) != 0)
            {
              tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
              tft.pushImage(0, 12, 128, 104, roomblack);
            }
            tft.pushImage(0, 12, 128, 104, roomgreen);
            roomwhitex = 49;
            roomwhitey = 114;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        tft.pushImage(0, 12, 128, 104, roomwhite);
        roomwhitex = 45;
        roomwhitey = 111;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
      }
      
      else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 21)
      {
//          roomwhitex = 20;
//          roomwhitey = 27;
//          roomwhitex = 107;
//          roomwhitey = 27;
        tft.pushImage(0, 12, 128, 104, roomblue);
        roomwhitex = 107;
        roomwhitey = 27;
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
        delay(300);
        while(!(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 107 && roomwhitey == 27))
        {
          if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 107 && roomwhitey == 27)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 20;
            roomwhitey = 27;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_B_PIN) == 0 && roomwhitex == 20 && roomwhitey == 27)
          {
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
            roomwhitex = 107;
            roomwhitey = 27;
            tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
            delay(300);
          }
          else if(digitalRead(BTN_A_PIN) == 0 && roomwhitex == 20 && roomwhitey == 27)
          {
            Roomshower();
          }
        }
        tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_BLACK);
        tft.pushImage(0, 12, 128, 104, roomwhite);
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
  if(digitalRead(BTN_A_PIN) == 0)
  {
    delay(300);
    tft.fillScreen(TFT_BLACK);
    //tft.drawString("Sablina Tamagotchi", 13, 10);
    tft.drawString("Mode", 10, 25, 2);
    tft.drawString("Brightness", 10, 50, 2);
    tft.drawString("Exit", 10, 75, 2);
    timex1 = 5, timey1 = 23, timex2 = 76, timey2 = 21;
    tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);

    while(!(timey1 == 73 && digitalRead(BTN_A_PIN) == 0))
    {
      tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
      if(timey1 == 23 && digitalRead(BTN_B_PIN) == 0)
      {
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_BLACK);
        timey1 += 25;
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
        delay(300);
      }
  
      else if(timey1 == 48 && digitalRead(BTN_B_PIN) == 0)
      {
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_BLACK);
        timey1 += 25;
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
        delay(300);
      }
  
      else if(timey1 == 73 && digitalRead(BTN_B_PIN) == 0)
      {
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_BLACK);
        timey1 = 23;
        tft.drawRect(timex1, timey1, timex2, timey2, TFT_WHITE);
        delay(300);
      }
  
      else if(timey1 == 23 && digitalRead(BTN_A_PIN) == 0)//选择模式子菜单
      {
        delay(300);
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Light", 10, 25, 2);
        tft.drawString("Dark", 10, 50, 2);
        while(digitalRead(BTN_A_PIN) != 0)
        {
          tft.drawRect(modex1, modey1, modex2, modey2, TFT_WHITE);
          if(digitalRead(BTN_B_PIN) == 0 && modey1 == 23)
          {
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_BLACK);
            modey1 += 25;
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_WHITE);
            delay(300);
          }
  
          else if(digitalRead(BTN_B_PIN) == 0 && modey1 == 48)
          {
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_BLACK);
            modey1 = 23;
            tft.drawRect(modex1, modey1, modex2, modey2, TFT_WHITE);
            delay(300);
          }
  
          else if(digitalRead(BTN_A_PIN) == 0 && modey1 == 23)
          {
            darkmode = 0;
          }
  
          else if(digitalRead(BTN_A_PIN) == 0 && modey1 == 48)
          {
            darkmode = 1;
          }
        }
        delay(300);
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Mode", 10, 25, 2);
        tft.drawString("Brightness", 10, 50, 2);
        tft.drawString("Exit", 10, 75, 2);
      }
  
      else if(timey1 == 48 && digitalRead(BTN_A_PIN) == 0)//选择亮度子菜单
      {
        delay(300);
        tft.fillScreen(TFT_BLACK);
        while(digitalRead(BTN_A_PIN) != 0)
        {
          tft.drawString("Sablina Tamagotchi", 13, 10);
          tft.drawString(String(b), 35, 40, 4);
          if(digitalRead(BTN_B_PIN) == 0)
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

void mainselect()
{
  tft.drawRect(mainx1, mainy1, mainx2, mainy2, TFT_WHITE); 
  buttonstate47 = digitalRead(BTN_B_PIN);
  if(buttonstate47 == LOW && buttonbefore47 == HIGH)
  {
    tft.drawRect(mainx1, mainy1, mainx2, mainy2, TFT_BLACK); 
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
  tft.drawRect(mainx1, mainy1, mainx2, mainy2, TFT_WHITE);

  switch(mainnum)
  {
    case 0: 
    {
      Generalmenu(); 
    }
    break;

    case 1:
    {
      Food();
    }
    break;

    case 2:
    {
      Sleep();
    }
    break;

    case 3:
    {
      Shower();
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
      Box();
    }
    break;

    case 7:
    {
      Time();
    }
    break;
  }
}


void setup() 
{
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
  g_lastAgeTickMs = millis();
  loadSocialMemory();

  // ── Display ────────────────────────────────────────────────────
  tft.begin();
  tft.setRotation(1);              // landscape: 320 x 172
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  // Centre the legacy 128×128 game canvas in the display
  tft.setViewport(GAME_X, GAME_Y, GAME_W, GAME_H);
  // Force BL line high first, then attach PWM (matches board behavior better).
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
#if TFT_BL_PIN_ALT >= 0
  pinMode(TFT_BL_PIN_ALT, OUTPUT);
  digitalWrite(TFT_BL_PIN_ALT, HIGH);
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
    // Game code reads digitalRead(BTN_B_PIN); hardware pins take priority
    // so we cannot inject digitally – instead trigger LLM reaction
    g_llm.reactToEvent("shaken", Hun, Fat, Cle);
  }

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

  // ── Warn icon: blink when sick ────────────────────────────────────
  bool showWarn = (Hun < 60 || Fat < 60 || Cle < 60) || g_petSick;
  if (g_petSick && (now / 500) % 2 == 0) showWarn = false; // blink
  if (showWarn)
  {
    tft.pushImage(2, 1, 28, 28, warn);
  }
  else
  {
    tft.pushImage(2, 1, 28, 28, pest);
  }

  state_count();
  maingif();
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
  if (now - g_lastRgbUpdate > 2000) {
    g_lastRgbUpdate = now;
    updateRgbMood();
  }

  drawFloatingMessage(now);
  processSoundNotifications(now);
  processVibrationNotifications(now);

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
  tft.resetViewport();  // access full 320×172 screen

  int sx = SIDEBAR_X;
  int sy = 4;
  int sw = SIDEBAR_W - 4;

  // Background
  tft.fillRect(sx, 0, SIDEBAR_W, SCREEN_H, TFT_BLACK);

  // Pet name
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(sx, sy);
  tft.print(g_llm.traits.name);
  sy += 14;

  // Stat bars
  auto drawBar = [&](const char* label, int val, uint16_t col) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(sx, sy);
    tft.print(label);
    sy += 11;
    int bw = map(val, 0, 100, 0, sw);
    tft.fillRect(sx,     sy, bw,      7, col);
    tft.fillRect(sx + bw, sy, sw - bw, 7, 0x2104);  // dark grey
    sy += 10;
  };

  drawBar("HUN", Hun, TFT_ORANGE);
  drawBar("FAT", Fat, TFT_GREEN);
  drawBar("CLE", Cle, TFT_SKYBLUE);

  // Coins
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(sx, sy); sy += 11;
  tft.printf("%d coins", Exp);

  // Mood
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.setCursor(sx, sy); sy += 11;
  tft.print(g_llm.moodName());

  // WiFi / BLE indicators
  tft.setTextColor(g_wifiEnabled ? TFT_GREEN : 0x39C7, TFT_BLACK);
  tft.setCursor(sx, sy); sy += 11;
  tft.print(g_wifiEnabled ? "WiFi OK" : "No WiFi");

  tft.setTextColor(g_llm.isForceOffline() ? TFT_ORANGE : 0x39C7, TFT_BLACK);
  tft.setCursor(sx, sy); sy += 11;
  tft.print(g_llm.isForceOffline() ? "OFFLINE FORCED" : "AUTO ONLINE");

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(sx, sy); sy += 11;
  tft.print("Room: ");
  tft.print(g_targetRoom);

  tft.setTextColor(g_ble.deviceConnected ? TFT_CYAN : 0x39C7, TFT_BLACK);
  tft.setCursor(sx, sy); sy += 11;
  tft.print(g_ble.deviceConnected ? "App BLE" : "BLE adv");

  tft.setTextColor(g_ble.peerVisible() ? CHAT_PEER_TEXT_COLOR : 0x39C7, TFT_BLACK);
  tft.setCursor(sx, sy);
  if (g_ble.peerVisible()) {
    char peerShort[20];
    trimChatLine(g_ble.peerName(), peerShort, sizeof(peerShort), 14);
    tft.print("Peer ");
    tft.print(peerShort);
  } else {
    tft.print("Peer scan");
  }
  sy += 11;

  tft.setTextColor(0x8DF1, TFT_BLACK);
  tft.setCursor(sx, sy);
  if (g_ble.peerVisible()) {
    const SocialPeerMemory* peer = findSocialPeer(g_ble.peer.senderId);
    tft.print("Bond ");
    tft.print(socialBondLabel(peer));
  } else {
    tft.print("Bond none");
  }

  // Divider line
  tft.drawFastVLine(SIDEBAR_X - 2, 0, SCREEN_H, 0x39C7);

  // Restore game viewport so all original drawing stays in the 128×128 area
  tft.setViewport(GAME_X, GAME_Y, GAME_W, GAME_H);
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
  if (!peerVisible) {
    g_peerWasVisible = false;
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

  if (g_popupUntilMs && nowMs < g_popupUntilMs) {
    return;
  }

  const bool justDetected = !g_peerWasVisible;
  g_peerWasVisible = true;
  notePeerEncounter(g_ble.peer.senderId, g_ble.peerName(), justDetected);
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
  tft.pushImage(0, 12, 128, 104, roomImage);
  roomwhitex = cursorX;
  roomwhitey = cursorY;
  tft.drawCircle(roomwhitex, roomwhitey, 2, TFT_WHITE);
}

void showSleepStill(unsigned long holdMs) {
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, 128, 128, sablinasleep);
  if (holdMs > 0) {
    delay(holdMs);
  }
}

void playGardenWalkSequence() {
  for (int i = 0; i < 4; ++i) {
    tft.fillScreen(TFT_BLACK);
    tft.pushImage(0, 12, 128, 104, roomblack);
    tft.pushImage(52, 69, 23, 35, gamewalk[i]);
    delay(140);
  }
}

void playGardenExploreSequence() {
  playGardenWalkSequence();
  frame1 = 0;
  for (int i = 0; i < 19; ++i) {
    tft.pushImage(0, 12, 128, 104, gardengif[frame1]);
    frame1++;
    delay(100);
  }
}

void autoPerformKitchenAction() {
  drawAutoRoom(roomred, 96, 23);
  delay(250);

  frame1 = 0;
  tft.pushImage(0, 12, 128, 104, eatgif[frame1]);
  delay(250);
  for (int i = 0; i < 6; ++i) {
    tft.pushImage(0, 12, 128, 104, eatgif[frame1]);
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
  tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
  delay(250);
  for (int i = 0; i < 6; ++i) {
    tft.pushImage(0, 12, 128, 104, sleepgif[frame1]);
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
    tft.pushImage(0, 12, 128, 104, cleangif[frame1]);
    delay(250);
    for (int i = 0; i < 15; ++i) {
      tft.pushImage(0, 12, 128, 104, cleangif[frame1]);
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
  drawAutoRoom(roomgray, 45, 16);
  delay(250);

  frame1 = 0;
  tft.pushImage(0, 12, 128, 104, gamegif[frame1]);
  delay(250);
  for (int i = 0; i < 7; ++i) {
    tft.pushImage(0, 12, 128, 104, gamegif[frame1]);
    frame1++;
    delay(180);
  }

  Exp_mas -= 20;

  drawAutoRoom(roomgray, 45, 16);
}

void autoNavigateToTargetRoom(unsigned long nowMs) {
  if (strcmp(g_targetRoom, "LIVING") == 0) return;
  if ((nowMs - g_lastAutoRoomMs) < 5000) return;

  g_lastAutoRoomMs = nowMs;

  for (int hop = 0; hop < 6; ++hop) {
    state_count();

    if (strcmp(g_targetRoom, "KITCHEN") == 0) {
      if (Hun < 85) {
        autoPerformKitchenAction();
        state_count();
      }
    } else if (strcmp(g_targetRoom, "BEDROOM") == 0) {
      if (Fat < 85) {
        autoPerformBedroomAction();
        state_count();
      }
    } else if (strcmp(g_targetRoom, "BATHROOM") == 0) {
      if (Cle < 85) {
        autoPerformBathroomAction();
        state_count();
      }
    } else if (strcmp(g_targetRoom, "PLAYROOM") == 0) {
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
}

void drawFloatingMessage(unsigned long nowMs) {
  if (g_popupUntilMs == 0 || nowMs > g_popupUntilMs || !g_popupText[0]) return;

  tft.resetViewport();

  const int bx = 8;
  const int by = 8;
  const bool dualSpeaker = g_popupDualSpeaker && g_popupPeerText[0];
  const int bw = dualSpeaker ? 250 : 210;
  const int bh = dualSpeaker ? 50 : 34;

  tft.fillRoundRect(bx, by, bw, bh, 8, TFT_BLACK);
  tft.drawRoundRect(bx, by, bw, bh, 8, CHAT_BUBBLE_FRAME_COLOR);
  tft.setTextSize(1);

  if (dualSpeaker) {
    char localLabel[24];
    char peerLabel[24];
    char localLine[56];
    char peerLine[56];

    trimChatLine(g_llm.traits.name, localLabel, sizeof(localLabel), 8);
    trimChatLine(g_popupPeerName, peerLabel, sizeof(peerLabel), 10);
    trimChatLine(g_popupText, localLine, sizeof(localLine), 25);
    trimChatLine(g_popupPeerText, peerLine, sizeof(peerLine), 24);

    tft.fillRoundRect(bx + 4, by + 4, bw - 8, 17, 5, CHAT_LOCAL_BG_COLOR);
    tft.fillRoundRect(bx + 4, by + 28, bw - 8, 17, 5, CHAT_PEER_BG_COLOR);

    tft.setTextColor(CHAT_LOCAL_TEXT_COLOR, CHAT_LOCAL_BG_COLOR);
    tft.setCursor(bx + 8, by + 10);
    tft.print(localLabel);
    tft.print(": ");
    tft.print(localLine);

    tft.setTextColor(CHAT_PEER_TEXT_COLOR, CHAT_PEER_BG_COLOR);
    tft.setCursor(bx + 8, by + 34);
    tft.print(peerLabel);
    tft.print(": ");
    tft.print(peerLine);
  } else {
    tft.fillRoundRect(bx, by, bw, bh, 6, CHAT_LOCAL_BG_COLOR);
    tft.drawRoundRect(bx, by, bw, bh, 6, CHAT_BUBBLE_FRAME_COLOR);
    tft.setTextColor(CHAT_LOCAL_TEXT_COLOR, CHAT_LOCAL_BG_COLOR);
    tft.setCursor(bx + 6, by + 10);

    char line[48];
    trimChatLine(g_popupText, line, sizeof(line), 38);
    tft.print(line);
  }

  tft.setViewport(GAME_X, GAME_Y, GAME_W, GAME_H);
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

// ── RGB LED reflects current mood ────────────────────────────────────
void updateRgbMood() {
  uint8_t r, g, b;
  g_llm.getMoodColor(r, g, b);
  g_rgb.setPixelColor(0, g_rgb.Color(r, g, b));
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
  tft.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H, TFT_BLACK);
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

  tft.setViewport(GAME_X, GAME_Y, GAME_W, GAME_H);
}

#endif
