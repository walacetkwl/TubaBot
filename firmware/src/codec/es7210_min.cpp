// SPDX-License-Identifier: Apache-2.0
// Sequência baseada em esp_codec_dev/device/es7210/es7210.c
// (Copyright 2023 Espressif Systems (Shanghai) CO LTD, Apache-2.0).
#include "es7210_min.h"
#include <Wire.h>
#include "es7210_reg.h"

static uint8_t g_addr = 0x40;

static bool wr(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(g_addr);
  Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}
static int rd(uint8_t reg) {
  Wire.beginTransmission(g_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)g_addr, 1) != 1) return -1;
  return Wire.read();
}
static bool upd(uint8_t reg, uint8_t mask, uint8_t data) {
  int v = rd(reg);
  if (v < 0) return false;
  return wr(reg, (uint8_t)((v & ~mask) | (data & mask)));
}

bool es7210MinInit(uint8_t addr, uint8_t gainCode) {
  g_addr = addr;
  bool ok = true;
  // abertura (open)
  ok &= wr(ES7210_RESET_REG00, 0xff);
  ok &= wr(ES7210_RESET_REG00, 0x41);
  ok &= wr(ES7210_CLOCK_OFF_REG01, 0x3f);
  ok &= wr(ES7210_TIME_CONTROL0_REG09, 0x30);
  ok &= wr(ES7210_TIME_CONTROL1_REG0A, 0x30);
  ok &= wr(ES7210_ADC12_HPF2_REG23, 0x2a);
  ok &= wr(ES7210_ADC12_HPF1_REG22, 0x0a);
  ok &= wr(ES7210_ADC34_HPF2_REG20, 0x0a);
  ok &= wr(ES7210_ADC34_HPF1_REG21, 0x2a);
  if (!ok) return false;
  ok &= upd(ES7210_MODE_CONFIG_REG08, 0x01, 0x00);  // escravo: o ESP32 gera MCLK/BCLK/WS
  ok &= wr(ES7210_ANALOG_REG40, 0x43);
  ok &= wr(ES7210_MIC12_BIAS_REG41, 0x70);  // 2,87 V
  ok &= wr(ES7210_MIC34_BIAS_REG42, 0x70);
  ok &= wr(ES7210_OSR_REG07, 0x20);
  ok &= wr(ES7210_MAINCLK_REG02, 0xc1);
  // seleção MIC1 + MIC2 (os dois microfones frontais)
  for (int i = 0; i < 4; i++) ok &= upd(ES7210_MIC1_GAIN_REG43 + i, 0x10, 0x00);
  ok &= wr(ES7210_MIC12_POWER_REG4B, 0xff);
  ok &= wr(ES7210_MIC34_POWER_REG4C, 0xff);
  ok &= upd(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
  ok &= wr(ES7210_MIC12_POWER_REG4B, 0x00);
  gainCode = gainCode > 14 ? 14 : gainCode;
  ok &= upd(ES7210_MIC1_GAIN_REG43, 0x10, 0x10); ok &= upd(ES7210_MIC1_GAIN_REG43, 0x0f, gainCode);
  ok &= upd(ES7210_MIC2_GAIN_REG44, 0x10, 0x10); ok &= upd(ES7210_MIC2_GAIN_REG44, 0x0f, gainCode);
  ok &= wr(ES7210_SDP_INTERFACE2_REG12, 0x00);  // sem TDM (2 microfones)
  // formato: I²S padrão, 16 bits
  int iface = rd(ES7210_SDP_INTERFACE1_REG11);
  if (iface < 0) return false;
  iface = (iface & 0x1f) | 0x60;
  iface &= 0xfc;
  ok &= wr(ES7210_SDP_INTERFACE1_REG11, (uint8_t)iface);
  // início (start)
  int off = rd(ES7210_CLOCK_OFF_REG01);
  ok &= wr(ES7210_CLOCK_OFF_REG01, (uint8_t)(off < 0 ? 0x00 : off));
  ok &= wr(ES7210_POWER_DOWN_REG06, 0x00);
  ok &= wr(ES7210_ANALOG_REG40, 0x43);
  ok &= wr(ES7210_MIC1_POWER_REG47, 0x08);
  ok &= wr(ES7210_MIC2_POWER_REG48, 0x08);
  ok &= wr(ES7210_MIC3_POWER_REG49, 0x08);
  ok &= wr(ES7210_MIC4_POWER_REG4A, 0x08);
  ok &= wr(ES7210_ANALOG_REG40, 0x43);
  ok &= wr(ES7210_RESET_REG00, 0x71);
  ok &= wr(ES7210_RESET_REG00, 0x41);
  return ok;
}
