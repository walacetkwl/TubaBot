#include "hw.h"
#include <Wire.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include "board_profile.h"
extern "C" {
#include "codec/es8311.h"
}

uint64_t nowMs() { return (uint64_t)(esp_timer_get_time() / 1000); }

// ════════ Tela ════════
static Arduino_DataBus* bus = nullptr;
static Arduino_GFX* gfx = nullptr;
static uint16_t* strip = nullptr;            // buffer DMA (RAM interna) para faixas de linhas
static const int kStripRows = 20;

bool Display::begin() {
  // luz de fundo apagada até a primeira imagem (sem "flash" branco no boot)
  ledcAttach(TB_LCD_BL, 20000, 10);
  ledcWrite(TB_LCD_BL, 0);
  bus = new Arduino_ESP32SPI(TB_LCD_DC, TB_LCD_CS, TB_LCD_SCK, TB_LCD_MOSI, TB_LCD_MISO);
  gfx = new Arduino_ILI9341(bus, GFX_NOT_DEFINED /* RST = reset do ESP32 */, TB_LCD_ROTATION, TB_LCD_IPS);
  ok_ = gfx->begin(TB_LCD_SPI_HZ);
  strip = (uint16_t*)heap_caps_malloc(tb::SCR_W * kStripRows * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!strip) ok_ = false;
  if (ok_) gfx->fillScreen(0x0000);
  ok_ = ok_ && gfx->width() == tb::SCR_W && gfx->height() == tb::SCR_H;
  return ok_;
}

// Brilho 0..255 -> PWM com curva perceptiva (o olho é mais sensível no escuro).
void Display::setBrightness(uint8_t b) {
  bright_ = b;
  uint32_t duty = b ? (uint32_t)(8 + (uint32_t)b * b * 1015 / (255 * 255)) : 0;
  ledcWrite(TB_LCD_BL, duty);
}

void Display::setFlipped(bool flip) {
  if (!ok_ || flip == flipped_) return;
  flipped_ = flip;
  int r = TB_LCD_ROTATION;
  if (flip) r = (r + 2) % 4;
  gfx->setRotation(r);
  fullNext_ = true;
}

bool Display::push(const uint16_t* fb, const tb::IRect& rr) {
  if (!ok_) return false;
  tb::IRect r = fullNext_ ? tb::IRect(0, 0, tb::SCR_W, tb::SCR_H) : rr;
  if (r.empty()) return false;
  fullNext_ = false;
  uint32_t t0 = micros();
  const int W = tb::SCR_W;
  int w = r.w();
  for (int y = 0; y < r.h(); y += kStripRows) {
    int rows = min(kStripRows, r.h() - y);
    for (int k = 0; k < rows; k++) memcpy(strip + k * w, fb + (r.y0 + y + k) * W + r.x0, w * 2);
    gfx->draw16bitRGBBitmap(r.x0, r.y0 + y, strip, w, rows);
  }
  lastUs_ = micros() - t0;
  return true;
}

// ════════ Toque (FT6336G), bateria, botão ════════
static bool i2cProbe(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

int Sensors::scanI2C(uint8_t* found, int max) {
  int n = 0;
  for (uint8_t a = 1; a < 127 && n < max; a++) if (i2cProbe(a)) found[n++] = a;
  return n;
}

void Sensors::begin() {
  Wire.begin(TB_I2C_SDA, TB_I2C_SCL, TB_I2C_HZ);
  pinMode(TB_TP_INT, INPUT_PULLUP);
  pinMode(TB_TP_RST, OUTPUT);
  digitalWrite(TB_TP_RST, LOW); delay(10);
  digitalWrite(TB_TP_RST, HIGH); delay(200);
  touchOk_ = i2cProbe(TB_ADDR_FT6336);
  if (touchOk_) {
    Wire.beginTransmission(TB_ADDR_FT6336); Wire.write(0x80); Wire.write(40); Wire.endTransmission();   // limiar: menos toques fantasmas dentro do corpo
    Wire.beginTransmission(TB_ADDR_FT6336); Wire.write(0xA4); Wire.write(0x00); Wire.endTransmission(); // modo de consulta (polling)
  }
  pinMode(TB_BTN_BOOT, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(TB_BAT_ADC, ADC_11db);
}

// Registradores 0x02..0x06: nº de toques, X (alto/baixo), Y (alto/baixo) — referencial retrato.
bool Sensors::readTouch(int& x, int& y) {
  Wire.beginTransmission(TB_ADDR_FT6336);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TB_ADDR_FT6336, 5) != 5) return false;
  uint8_t d[5];
  for (int i = 0; i < 5; i++) d[i] = Wire.read();
  int n = d[0] & 0x0F;
  if (n < 1 || n > 2) return false;
  rx_ = ((d[1] & 0x0F) << 8) | d[2];
  ry_ = ((d[3] & 0x0F) << 8) | d[4];
  int lx, ly;
  TB_TOUCH_MAP(rx_, ry_, lx, ly);
  x = constrain(lx, 0, tb::SCR_W - 1);
  y = constrain(ly, 0, tb::SCR_H - 1);
  return true;
}

// Curva de descarga típica de LiPo 1S (mV -> %).
static int liPoPercent(int mv) {
  static const int16_t T[][2] = {{4180, 100}, {4100, 92}, {4000, 80}, {3900, 66}, {3820, 54}, {3760, 42},
                                 {3710, 30}, {3670, 20}, {3620, 12}, {3550, 6}, {3450, 2}, {3300, 0}};
  if (mv >= T[0][0]) return 100;
  for (unsigned i = 1; i < sizeof T / sizeof T[0]; i++)
    if (mv >= T[i][0]) return T[i][1] + (mv - T[i][0]) * (T[i - 1][1] - T[i][1]) / (T[i - 1][0] - T[i][0]);
  return 0;
}

void Sensors::poll(tb::App& app, uint64_t now) {
  // toque a 100 Hz
  if (touchOk_ && now >= nextTouch_) {
    nextTouch_ = now + 10;
    int x, y;
    touching_ = readTouch(x, y);
    if (touching_) { lx_ = x; ly_ = y; }
    app.touchSample(touching_, lx_, ly_, now);
  }
  // bateria a cada 2 s (média filtrada; o núcleo ignora no TubaBot Lite)
  if (now >= nextBat_) {
    nextBat_ = now + 2000;
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) acc += analogReadMilliVolts(TB_BAT_ADC);
    float mv = (float)acc / 16.f * TB_BAT_DIV;
    batFilt_ = batFilt_ <= 0.f ? mv : batFilt_ * 0.8f + mv * 0.2f;
    batMv_ = (int)batFilt_;
    tb::PowerState p;
    p.known = true;
    p.battery = batMv_ > 2800;         // sem bateria o divisor lê ~0 V
    p.mv = (uint16_t)batMv_;
    p.percent = (int8_t)(p.battery ? liPoPercent(batMv_) : -1);
    p.charging = false;                // a placa não informa a carga (TP4054 sem sinal ao ESP32)
    p.usb = false;
    app.powerUpdate(p, now);
  }
  // botão BOOT a 20 Hz: curto = acordar/apagar/voltar; 3 s = rede de configuração
  if (now >= nextBtn_) {
    nextBtn_ = now + 50;
    bool boot = digitalRead(TB_BTN_BOOT) == LOW;
    if (boot && !bootDownAt_) { bootDownAt_ = now; bootLongSent_ = false; }
    if (boot && bootDownAt_ && !bootLongSent_ && now - bootDownAt_ >= 3000) { bootLongSent_ = true; app.hwButton(tb::HwButton::Boot, true, now); }
    if (!boot && bootDownAt_) { if (!bootLongSent_ && now - bootDownAt_ >= 30) app.hwButton(tb::HwButton::Boot, false, now); bootDownAt_ = 0; }
  }
}

// ════════ Áudio ════════
static I2SClass i2s;
static es8311_handle_t es = nullptr;

static void paEnable(bool on) { digitalWrite(TB_PA_EN, (on != (bool)TB_PA_ACTIVE_LOW) ? HIGH : LOW); }

bool Audio::begin(tb::App* app) {
  app_ = app;
  pinMode(TB_PA_EN, OUTPUT);
  paEnable(false);  // amplificador desligado até o codec estar pronto (sem estalo)
  i2s.setPins(TB_I2S_BCLK, TB_I2S_WS, TB_I2S_DOUT, TB_I2S_DIN, TB_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, TB_AUDIO_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return false;
  es = es8311_create(0, TB_ADDR_ES8311);
  if (es) {
    const es8311_clock_config_t clk = {false, false, true, TB_AUDIO_RATE * 256, TB_AUDIO_RATE};
    spkOk_ = es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) == ESP_OK &&
             es8311_sample_frequency_config(es, clk.mclk_frequency, clk.sample_frequency) == ESP_OK &&
             es8311_voice_volume_set(es, 80, nullptr) == ESP_OK;  // o volume do usuário é aplicado no sintetizador
    micOk_ = spkOk_ && es8311_microphone_config(es, false) == ESP_OK &&
             es8311_microphone_gain_set(es, ES8311_MIC_GAIN_24DB) == ESP_OK;
  }
  if (spkOk_) paEnable(true);
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
      for (size_t i = 0; i < n / 2; i += 2) { int32_t v = in[i]; acc += (uint64_t)((int64_t)v * v); accN++; }  // canal esquerdo
      if (millis() - lastLevel >= 40 && accN) {
        double rms = sqrt((double)acc / accN) / 32768.0;
        int lv = (int)lround(20.0 * log10(rms + 1e-9) + 100.0);
        self->level_ = constrain(lv, 0, 100);
        acc = 0; accN = 0; lastLevel = millis();
      }
    }
  }
}

void ledOff() { rgbLedWrite(TB_RGB_LED, 0, 0, 0); }
