#pragma once
// ═══════════════════════════════════════════════════════════════════
//  QMI8658 6-axis IMU handler
//  Provides:
//    • init via I²C
//    • readAccel()  – raw g values
//    • isShaking()  – triggered when |Δaccel| > SHAKE_THRESHOLD
//    • stepCount()  –  pedometer (if enabled in QMI8658 firmware)
// ═══════════════════════════════════════════════════════════════════
#include "config.h"
#include <Wire.h>
#include <math.h>

// QMI8658 register map (partial – enough for accel + gyro read)
namespace QMI8658Reg {
  constexpr uint8_t WHO_AM_I   = 0x00;
  constexpr uint8_t CTRL1      = 0x02;  // SPI/I2C mode
  constexpr uint8_t CTRL2      = 0x03;  // accel ODR + scale
  constexpr uint8_t CTRL3      = 0x04;  // gyro  ODR + scale
  constexpr uint8_t CTRL7      = 0x08;  // enable accel + gyro
  constexpr uint8_t AX_L       = 0x35;  // accel X low byte
  constexpr uint8_t RESET      = 0x60;
  constexpr uint8_t WHO_AM_I_VAL = 0x05;
}

class IMUHandler {
public:
  float ax = 0, ay = 0, az = 0;  // g
  float gx = 0, gy = 0, gz = 0;  // deg/s
  bool  available = false;

  bool begin(uint8_t addr = QMI8658_I2C_ADDR,
             int sda = IMU_SDA_PIN, int scl = IMU_SCL_PIN) {
    Wire.begin(sda, scl);
    _addr = addr;

    // Soft reset
    _write(QMI8658Reg::RESET, 0xB0);
    delay(15);

    // Verify WHO_AM_I
    uint8_t id = _read(QMI8658Reg::WHO_AM_I);
    if (id != QMI8658Reg::WHO_AM_I_VAL) {
      // Try alternate address
      _addr = (addr == 0x6B) ? 0x6A : 0x6B;
      id = _read(QMI8658Reg::WHO_AM_I);
      if (id != QMI8658Reg::WHO_AM_I_VAL) return false;
    }

    // CTRL1: I2C mode, address auto-increment
    _write(QMI8658Reg::CTRL1, 0x40);
    // CTRL2: accel ±4g, ODR 125Hz
    _write(QMI8658Reg::CTRL2, 0x23);
    // CTRL3: gyro ±512dps, ODR 125Hz
    _write(QMI8658Reg::CTRL3, 0x53);
    // CTRL7: enable accel + gyro
    _write(QMI8658Reg::CTRL7, 0x03);

    available = true;
    return true;
  }

  void update() {
    if (!available) return;
    uint8_t buf[12];
    _readBurst(QMI8658Reg::AX_L, buf, 12);

    int16_t rawAx = (int16_t)(buf[1] << 8 | buf[0]);
    int16_t rawAy = (int16_t)(buf[3] << 8 | buf[2]);
    int16_t rawAz = (int16_t)(buf[5] << 8 | buf[4]);
    int16_t rawGx = (int16_t)(buf[7] << 8 | buf[6]);
    int16_t rawGy = (int16_t)(buf[9] << 8 | buf[8]);
    int16_t rawGz = (int16_t)(buf[11]<< 8 | buf[10]);

    // ±4g range → 8192 LSB/g
    ax = rawAx / 8192.0f;
    ay = rawAy / 8192.0f;
    az = rawAz / 8192.0f;
    // ±512dps range → 64 LSB/dps
    gx = rawGx / 64.0f;
    gy = rawGy / 64.0f;
    gz = rawGz / 64.0f;
  }

  // Returns true once per detected shake (auto-resets)
  bool isShaking() {
    if (!available) return false;
    update();
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    float delta = fabsf(mag - _prevMag);
    _prevMag = mag;
    if (delta > SHAKE_THRESHOLD) {
      unsigned long now = millis();
      if (now - _lastShake > 600) {  // debounce 600ms
        _lastShake = now;
        return true;
      }
    }
    return false;
  }

  // Simple tilt detection: returns -1 left, +1 right, 0 none
  int tilt() {
    if (!available) return 0;
    if (ax >  0.5f) return  1;
    if (ax < -0.5f) return -1;
    return 0;
  }

private:
  uint8_t       _addr       = QMI8658_I2C_ADDR;
  float         _prevMag    = 1.0f;
  unsigned long _lastShake  = 0;

  void _write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  uint8_t _read(uint8_t reg) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
  }

  void _readBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, len);
    for (uint8_t i = 0; i < len && Wire.available(); i++)
      buf[i] = Wire.read();
  }
};
