#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>

namespace {
constexpr int kRgbPin = 38;
constexpr int kPinsToReset[] = {48, 47, 46};
constexpr int kPwmFreq = 5000;
constexpr int kPwmRes = 10;
constexpr int kPwmMax = (1 << kPwmRes) - 1;
constexpr unsigned long kHoldMs = 8000;

enum class ProbeMode {
  DigitalHigh,
  PwmMax,
  DigitalLow,
};

struct ProbeStep {
  int pin;
  const char *pinLabel;
  const char *phaseLabel;
  ProbeMode mode;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint16_t bgColor;
};

Adafruit_NeoPixel rgb(1, kRgbPin, NEO_GRB + NEO_KHZ800);
TFT_eSPI tft = TFT_eSPI();

const ProbeStep kSteps[] = {
    {48, "GPIO48", "HIGH", ProbeMode::DigitalHigh, 0, 64, 0, TFT_DARKGREEN},
    {48, "GPIO48", "PWM MAX", ProbeMode::PwmMax, 64, 32, 0, TFT_ORANGE},
    {48, "GPIO48", "LOW", ProbeMode::DigitalLow, 64, 0, 0, TFT_MAROON},
    {47, "GPIO47", "HIGH", ProbeMode::DigitalHigh, 0, 48, 64, TFT_DARKCYAN},
    {47, "GPIO47", "PWM MAX", ProbeMode::PwmMax, 64, 0, 64, TFT_PURPLE},
    {47, "GPIO47", "LOW", ProbeMode::DigitalLow, 48, 16, 0, TFT_BROWN},
    {46, "GPIO46", "HIGH", ProbeMode::DigitalHigh, 0, 32, 64, TFT_NAVY},
    {46, "GPIO46", "PWM MAX", ProbeMode::PwmMax, 64, 64, 0, TFT_OLIVE},
    {46, "GPIO46", "LOW", ProbeMode::DigitalLow, 64, 0, 16, TFT_RED},
};

constexpr size_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  rgb.setPixelColor(0, rgb.Color(red, green, blue));
  rgb.show();
}

void drivePinHigh(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

void drivePinLow(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void detachPinPwm(int pin) {
  ledcDetach(pin);
  pinMode(pin, OUTPUT);
}

void pwmPinMax(int pin) {
  pinMode(pin, OUTPUT);
  ledcAttach(pin, kPwmFreq, kPwmRes);
  ledcWrite(pin, kPwmMax);
}

void releaseAllPinsLow() {
  for (int pin : kPinsToReset) {
    detachPinPwm(pin);
    drivePinLow(pin);
  }
}

void drawStatusScreen(const ProbeStep &step, size_t stepIndex) {
  const int midX = tft.width() / 2;
  char progress[24];
  char answer[48];

  snprintf(progress, sizeof(progress), "PRUEBA %u/%u",
           static_cast<unsigned>(stepIndex + 1),
           static_cast<unsigned>(kStepCount));
  snprintf(answer, sizeof(answer), "%s %s", step.pinLabel, step.phaseLabel);

  tft.fillScreen(step.bgColor);
  tft.setTextColor(TFT_WHITE, step.bgColor);
  tft.drawCentreString("BACKLIGHT PROBE", midX, 10, 2);
  tft.drawCentreString(progress, midX, 34, 2);
  tft.drawCentreString(step.pinLabel, midX, 62, 4);
  tft.drawCentreString(step.phaseLabel, midX, 104, 4);
  tft.drawCentreString("SI FUNCIONA, DIME:", midX, 140, 2);
  tft.drawCentreString(answer, midX, 156, 2);
}

void applyStep(const ProbeStep &step) {
  switch (step.mode) {
    case ProbeMode::DigitalHigh:
      drivePinHigh(step.pin);
      break;
    case ProbeMode::PwmMax:
      pwmPinMax(step.pin);
      break;
    case ProbeMode::DigitalLow:
      drivePinLow(step.pin);
      break;
  }
}

void logStep(const ProbeStep &step, size_t stepIndex) {
  Serial.printf("[BL-PROBE] step %u/%u: %s %s\n",
                static_cast<unsigned>(stepIndex + 1),
                static_cast<unsigned>(kStepCount),
                step.pinLabel,
                step.phaseLabel);
}
}

void setup() {
  Serial.begin(115200);
  delay(300);

  rgb.begin();
  rgb.setBrightness(40);
  setRgb(0, 0, 32);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("INICIANDO PROBE", tft.width() / 2, 50, 4);
  tft.drawCentreString("GPIO48 / GPIO47 / GPIO46", tft.width() / 2, 100, 2);

  Serial.println();
  Serial.println("[BL-PROBE] starting");
  Serial.println("[BL-PROBE] steps last 8 seconds each");
  Serial.println("[BL-PROBE] testing GPIO48, GPIO47 and GPIO46 only");

  releaseAllPinsLow();
}

void loop() {
  for (size_t index = 0; index < kStepCount; ++index) {
    const ProbeStep &step = kSteps[index];
    releaseAllPinsLow();
    drawStatusScreen(step, index);
    setRgb(step.red, step.green, step.blue);
    logStep(step, index);
    applyStep(step);
    delay(kHoldMs);
  }
}