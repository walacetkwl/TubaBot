// Driver mínimo SSD1306/SH1106 por I²C, sem bibliotecas externas.
// Envia só as páginas alteradas, detecta 0x3C/0x3D e se recupera de falhas do barramento.
#pragma once
#include <Arduino.h>
#include "board_profile.h"

class Oled {
 public:
  bool begin();                       // varre 0x3C/0x3D e inicializa
  bool ok() const { return ok_; }
  uint8_t address() const { return addr_; }
  // envia o framebuffer (layout de páginas). Retorna false em erro de I²C.
  bool push(const uint8_t* fb, bool force = false);
  void setContrast(uint8_t c);
  void setRotation180(bool r);
  void setPower(bool on);
  // tentativa periódica de recuperação quando o visor some
  void service(uint32_t nowMs);
  uint32_t lastPushUs() const { return lastPushUs_; }
  uint32_t errors() const { return errors_; }
  static int scan(uint8_t* found, int max);  // diagnóstico: lista endereços que respondem
  static void busRecover();

 private:
  bool cmd(uint8_t c);
  bool cmd2(uint8_t c, uint8_t a);
  bool init();
  bool ok_ = false;
  uint8_t addr_ = 0;
  uint8_t shadow_[1024];
  bool shadowValid_ = false;
  bool power_ = true;
  uint8_t contrast_ = 200;
  bool rot_ = false;
  uint32_t errors_ = 0, consecutive_ = 0, lastRetry_ = 0, lastPushUs_ = 0;
};
