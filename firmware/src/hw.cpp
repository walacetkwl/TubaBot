#include "hw.h"
#include <Wire.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <time.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include "TouchDrvCSTXXX.hpp"
#include "SensorQMI8658.hpp"
#include "SensorPCF85063.hpp"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "board_profile.h"
#include "codec/es7210_min.h"
extern "C" {
#include "codec/es8311.h"
}

uint64_t nowMs() { return (uint64_t)(esp_timer_get_time() / 1000); }

// ════════ Tela ════════
static Arduino_DataBus* bus = nullptr;
static Arduino_CO5300* gfx = nullptr;
static uint16_t* strip = nullptr;            // buffer DMA (RAM interna) para faixas de linhas
static const int kStripRows = 24;

bool Display::begin() {
  bus = new Arduino_ESP32QSPI(TB_LCD_CS, TB_LCD_SCLK, TB_LCD_D0, TB_LCD_D1, TB_LCD_D2, TB_LCD_D3);
  gfx = new Arduino_CO5300(bus, TB_LCD_RST, 0 /*rotação*/, tb::SCR_W, tb::SCR_H, TB_LCD_COL_OFFSET, 0, 0, 0);
  ok_ = gfx->begin(TB_LCD_QSPI_HZ);
  strip = (uint16_t*)heap_caps_malloc(tb::SCR_W * kStripRows * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!strip) ok_ = false;
  if (ok_) { gfx->fillScreen(0x0000); gfx->setBrightness(bright_); }
  return ok_;
}

void Display::setBrightness(uint8_t b) {
  if (!ok_) return;
  if (b == 0) { if (on_) { gfx->displayOff(); on_ = false; } bright_ = 0; return; }
  if (!on_) { gfx->displayOn(); on_ = true; }
  if (b != bright_) { gfx->setBrightness(b); bright_ = b; }
}

bool Display::push(const uint16_t* fb, const tb::IRect& r, bool rot) {
  if (!ok_ || r.empty()) return false;
  uint32_t t0 = micros();
  const int W = tb::SCR_W, H = tb::SCR_H;
  int w = r.w();
  // tela girada: a área de destino é espelhada nos dois eixos (continua par)
  int dx0 = rot ? W - r.x1 : r.x0;
  int dy = rot ? H - r.y1 : r.y0;
  for (int y = 0; y < r.h(); y += kStripRows) {
    int rows = min(kStripRows, r.h() - y);
    for (int k = 0; k < rows; k++) {
      uint16_t* dst = strip + k * w;
      if (!rot) memcpy(dst, fb + (r.y0 + y + k) * W + r.x0, w * 2);
      else { const uint16_t* src = fb + (r.y1 - 1 - y - k) * W + r.x1 - 1; for (int i = 0; i < w; i++) dst[i] = *src--; }
    }
    gfx->draw16bitRGBBitmap(dx0, dy + y, strip, w, rows);
  }
  lastUs_ = micros() - t0;
  return true;
}

void Display::testPattern() {
  if (!ok_) return;
  static const uint16_t C[7] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F};
  for (int i = 0; i < 7; i++) gfx->fillRect(33 + i * 57, 60, 57, 340, C[i]);
}

// ════════ Sensores ════════
static TouchDrvCST92xx touch;
static SensorQMI8658 imu;
static SensorPCF85063 rtc;
static XPowersAXP2101 pmu;

static bool i2cProbe(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

int Sensors::scanI2C(uint8_t* found, int max) {
  int n = 0;
  for (uint8_t a = 1; a < 127 && n < max; a++) if (i2cProbe(a)) found[n++] = a;
  return n;
}

void Sensors::begin() {
  Wire.begin(TB_I2C_SDA, TB_I2C_SCL, TB_I2C_HZ);
  touch.setPins(TB_TP_RST, TB_TP_INT);
  touchOk_ = touch.begin(Wire, TB_ADDR_CST9217, TB_I2C_SDA, TB_I2C_SCL);
  if (touchOk_) {
    touch.setMaxCoordinates(tb::SCR_W, tb::SCR_H);
    touch.setMirrorXY(TB_TOUCH_MIRROR_X, TB_TOUCH_MIRROR_Y);
    touch.setSwapXY(TB_TOUCH_SWAP_XY);
  }
  imuOk_ = imu.begin(Wire, TB_ADDR_QMI8658, TB_I2C_SDA, TB_I2C_SCL);
  if (imuOk_) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_62_5Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  }
  rtcOk_ = rtc.begin(Wire, TB_I2C_SDA, TB_I2C_SCL);
  pmuOk_ = pmu.begin(Wire, TB_ADDR_AXP2101, TB_I2C_SDA, TB_I2C_SCL);
  if (pmuOk_) {
    // só medições: os trilhos de energia ficam como a placa configura de fábrica
    pmu.enableBattDetection();
    pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure();
    pmu.enableSystemVoltageMeasure();
  }
  expOk_ = i2cProbe(TB_ADDR_TCA9554);
  pinMode(TB_BTN_BOOT, INPUT_PULLUP);
}

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

// O RTC guarda a hora em UTC.
bool Sensors::rtcRead(int64_t& epoch) {
  if (!rtcOk_) return false;
  RTC_DateTime dt = rtc.getDateTime();
  struct tm t = dt.toUnixTime();
  if (t.tm_year + 1900 < 2024) return false;
  epoch = daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  return true;
}

void Sensors::rtcWrite(int64_t epoch) {
  if (!rtcOk_) return;
  time_t e = (time_t)epoch;
  struct tm t;
  gmtime_r(&e, &t);
  rtc.setDateTime(RTC_DateTime(t));
}

static bool expanderPwr(bool& pressed) {
  Wire.beginTransmission(TB_ADDR_TCA9554);
  Wire.write(0x00);  // registrador de entrada
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(TB_ADDR_TCA9554, 1) != 1) return false;
  pressed = (Wire.read() >> TB_EXIO_PWR_BIT) & 1;
  return true;
}

void Sensors::poll(tb::App& app, uint64_t now) {
  // toque a 100 Hz
  if (touchOk_ && now >= nextTouch_) {
    nextTouch_ = now + 10;
    int16_t x[1], y[1];
    uint8_t n = touch.getPoint(x, y, 1);
    touching_ = n > 0;
    if (touching_) { lx_ = x[0]; ly_ = y[0]; }
    app.touchSample(touching_, lx_, ly_, now);
  }
  // movimento a ~50 Hz (gravidade em mg no referencial da tela)
  if (imuOk_ && now >= nextImu_) {
    nextImu_ = now + 20;
    float fx, fy, fz;
    if (imu.getDataReady() && imu.getAccelerometer(fx, fy, fz)) {
      int ix = (int)lroundf(fx * 1000.f), iy = (int)lroundf(fy * 1000.f), iz = (int)lroundf(fz * 1000.f);
      int gx, gy, gz;
      TB_IMU_MAP(ix, iy, iz, gx, gy, gz);
      ax_ = gx; ay_ = gy; az_ = gz;
      app.motionSample(gx, gy, gz, now);
    }
  }
  // energia a cada 2 s
  if (pmuOk_ && now >= nextPmu_) {
    nextPmu_ = now + 2000;
    tb::PowerState p;
    p.known = true;
    p.battery = pmu.isBatteryConnect();
    p.percent = (int8_t)(p.battery ? constrain(pmu.getBatteryPercent(), 0, 100) : -1);
    p.charging = pmu.isCharging();
    p.usb = pmu.isVbusIn();
    p.mv = p.battery ? pmu.getBattVoltage() : 0;
    app.powerUpdate(p, now);
  }
  // botões a 20 Hz
  if (now >= nextBtn_) {
    nextBtn_ = now + 50;
    bool boot = digitalRead(TB_BTN_BOOT) == LOW;
    if (boot && !bootDownAt_) { bootDownAt_ = now; bootLongSent_ = false; }
    if (boot && bootDownAt_ && !bootLongSent_ && now - bootDownAt_ >= 3000) { bootLongSent_ = true; app.hwButton(tb::HwButton::Boot, true, now); }
    if (!boot && bootDownAt_) { if (!bootLongSent_ && now - bootDownAt_ >= 30) app.hwButton(tb::HwButton::Boot, false, now); bootDownAt_ = 0; }
    bool pwr = false;
    if (expOk_ && expanderPwr(pwr)) {
      if (pwr && !pwrDownAt_) pwrDownAt_ = now;
      if (!pwr && pwrDownAt_) { if (now - pwrDownAt_ < 1500) app.hwButton(tb::HwButton::Power, false, now); pwrDownAt_ = 0; }
    }
  }
}

// ════════ Áudio ════════
static I2SClass i2s;
static es8311_handle_t es = nullptr;

bool Audio::begin(tb::App* app) {
  app_ = app;
  pinMode(TB_PA_CTRL, OUTPUT);
  digitalWrite(TB_PA_CTRL, LOW);  // amplificador desligado até o codec estar pronto (sem estalo)
  i2s.setPins(TB_I2S_BCLK, TB_I2S_WS, TB_I2S_DOUT, TB_I2S_DIN, TB_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, TB_AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return false;
  es = es8311_create(0, TB_ADDR_ES8311);
  if (es) {
    const es8311_clock_config_t clk = {false, false, true, TB_AUDIO_RATE * 256, TB_AUDIO_RATE};
    spkOk_ = es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) == ESP_OK &&
             es8311_sample_frequency_config(es, clk.mclk_frequency, clk.sample_frequency) == ESP_OK &&
             es8311_voice_volume_set(es, 82, nullptr) == ESP_OK;  // o volume do usuário é aplicado no sintetizador
  }
  micOk_ = es7210MinInit(TB_ADDR_ES7210, 10 /* 30 dB */);
  if (spkOk_) digitalWrite(TB_PA_CTRL, HIGH);
  xTaskCreatePinnedToCore(task, "tb_audio", 6144, this, 5, nullptr, 0);
  return spkOk_;
}

void Audio::task(void* arg) {
  Audio* self = (Audio*)arg;
  static int16_t mono[256];
  static int16_t st[512];
  static int16_t in[512];
  uint64_t acc = 0; uint32_t accN = 0, lastLevel = millis();
  for (;;) {
    self->app_->audioRender(mono, 256);      // sintetizador do núcleo (fila sem travas)
    for (int i = 0; i < 256; i++) st[2 * i] = st[2 * i + 1] = mono[i];
    i2s.write((uint8_t*)st, sizeof st);       // bloqueia ~16 ms: dita o ritmo da tarefa
    if (self->micOk_) {
      size_t n = i2s.readBytes((char*)in, sizeof in);
      for (size_t i = 0; i < n / 2; i += 2) { int32_t v = in[i]; acc += (uint64_t)((int64_t)v * v); accN++; }
      if (millis() - lastLevel >= 40 && accN) {
        double rms = sqrt((double)acc / accN) / 32768.0;
        int lv = (int)lround(20.0 * log10(rms + 1e-9) + 100.0);
        self->level_ = constrain(lv, 0, 100);
        acc = 0; accN = 0; lastLevel = millis();
      }
    }
  }
}

// ════════ Motor opcional ════════
void motorBegin() {
#if TB_HAS_MOTOR
  digitalWrite(TB_PIN_MOTOR, TB_MOTOR_ACTIVE_HIGH ? LOW : HIGH);  // inativo antes de virar saída
  pinMode(TB_PIN_MOTOR, OUTPUT);
  digitalWrite(TB_PIN_MOTOR, TB_MOTOR_ACTIVE_HIGH ? LOW : HIGH);
#endif
}
void motorWrite(bool on) {
#if TB_HAS_MOTOR
  static int last = -1;
  if ((int)on == last) return;
  last = on;
  digitalWrite(TB_PIN_MOTOR, (on == (bool)TB_MOTOR_ACTIVE_HIGH) ? HIGH : LOW);
#else
  (void)on;
#endif
}
