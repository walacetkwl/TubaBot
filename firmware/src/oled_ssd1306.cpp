#include "oled_ssd1306.h"
#include <Wire.h>

static const uint8_t kColOffset = TB_OLED_SH1106 ? 2 : 0;

void Oled::busRecover() {
  // Libera um escravo travado segurando SDA: até 9 pulsos de clock + STOP manual.
  Wire.end();
  pinMode(TB_PIN_SDA, INPUT_PULLUP);
  pinMode(TB_PIN_SCL, OUTPUT_OPEN_DRAIN);
  for (int i = 0; i < 9 && digitalRead(TB_PIN_SDA) == LOW; i++) {
    digitalWrite(TB_PIN_SCL, LOW); delayMicroseconds(5);
    digitalWrite(TB_PIN_SCL, HIGH); delayMicroseconds(5);
  }
  pinMode(TB_PIN_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(TB_PIN_SDA, LOW); delayMicroseconds(5);
  digitalWrite(TB_PIN_SCL, HIGH); delayMicroseconds(5);
  digitalWrite(TB_PIN_SDA, HIGH); delayMicroseconds(5);
  Wire.begin(TB_PIN_SDA, TB_PIN_SCL, TB_I2C_HZ);
  Wire.setTimeOut(20);
}

int Oled::scan(uint8_t* found, int max) {
  int n = 0;
  for (uint8_t a = 1; a < 127 && n < max; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) found[n++] = a;
  }
  return n;
}

bool Oled::cmd(uint8_t c) {
  Wire.beginTransmission(addr_);
  Wire.write((uint8_t)0x00);
  Wire.write(c);
  return Wire.endTransmission() == 0;
}
bool Oled::cmd2(uint8_t c, uint8_t a) { return cmd(c) && cmd(a); }

bool Oled::init() {
  bool r = true;
  r &= cmd(0xAE);                     // display off
  r &= cmd2(0xD5, 0x80);              // clock
  r &= cmd2(0xA8, 0x3F);              // multiplex 64
  r &= cmd2(0xD3, 0x00);              // offset
  r &= cmd(0x40);                     // start line 0
  r &= cmd2(0x8D, 0x14);              // charge pump (SSD1306; ignorado pelo SH1106)
  r &= cmd2(0x20, 0x02);              // modo de página (compatível com SSD1306 e SH1106)
  r &= cmd(rot_ ? 0xA0 : 0xA1);       // segment remap
  r &= cmd(rot_ ? 0xC0 : 0xC8);       // COM scan
  r &= cmd2(0xDA, 0x12);              // COM pins
  r &= cmd2(0x81, contrast_);
  r &= cmd2(0xD9, 0xF1);              // pre-charge
  r &= cmd2(0xDB, 0x40);              // VCOMH
  r &= cmd(0xA4);                     // segue a RAM
  r &= cmd(0xA6);                     // normal (não invertido)
  r &= cmd(power_ ? 0xAF : 0xAE);
  shadowValid_ = false;
  return r;
}

bool Oled::begin() {
  Wire.begin(TB_PIN_SDA, TB_PIN_SCL, TB_I2C_HZ);
  Wire.setTimeOut(20);
  ok_ = false;
  for (uint8_t a : {(uint8_t)0x3C, (uint8_t)0x3D}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { addr_ = a; break; }
  }
  if (!addr_) { busRecover(); for (uint8_t a : {(uint8_t)0x3C, (uint8_t)0x3D}) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) { addr_ = a; break; } } }
  if (!addr_) return false;
  ok_ = init();
  return ok_;
}

bool Oled::push(const uint8_t* fb, bool force) {
  if (!ok_) return false;
  uint32_t t0 = micros();
  for (int p = 0; p < 8; p++) {
    const uint8_t* src = fb + p * 128;
    if (!force && shadowValid_ && memcmp(src, shadow_ + p * 128, 128) == 0) continue;  // página igual: não envia
    bool r = cmd(0xB0 + p) && cmd(0x00 | (kColOffset & 0x0F)) && cmd(0x10 | (kColOffset >> 4));
    for (int i = 0; r && i < 128; i += 30) {   // buffer do Wire: blocos pequenos
      Wire.beginTransmission(addr_);
      Wire.write((uint8_t)0x40);
      int n = i + 30 > 128 ? 128 - i : 30;
      Wire.write(src + i, n);
      r = Wire.endTransmission() == 0;
    }
    if (!r) {
      errors_++;
      if (++consecutive_ >= 3) { ok_ = false; shadowValid_ = false; }
      return false;
    }
    memcpy(shadow_ + p * 128, src, 128);
  }
  consecutive_ = 0;
  shadowValid_ = true;
  lastPushUs_ = micros() - t0;
  return true;
}

void Oled::setContrast(uint8_t c) { contrast_ = c; if (ok_) cmd2(0x81, c); }
void Oled::setRotation180(bool r) {
  if (r == rot_) return;
  rot_ = r;
  if (ok_) { cmd(rot_ ? 0xA0 : 0xA1); cmd(rot_ ? 0xC0 : 0xC8); shadowValid_ = false; }
}
void Oled::setPower(bool on) { if (on == power_) return; power_ = on; if (ok_) cmd(on ? 0xAF : 0xAE); }

void Oled::service(uint32_t nowMs) {
  if (ok_ || nowMs - lastRetry_ < 5000) return;
  lastRetry_ = nowMs;
  busRecover();
  addr_ = 0;
  begin();  // o núcleo continua rodando mesmo sem visor
}
