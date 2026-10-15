// QR Code para o assistente de primeiro uso (entrar na rede do Tuba com a câmera).
#pragma once
#include "tb_base.h"

namespace tb {
struct QrCode {
  static const int kMaxVersion = 8;  // até 49×49 módulos: sobra para "WIFI:T:WPA;S:...;P:...;;" e URLs
  uint8_t data[((kMaxVersion * 4 + 17) * (kMaxVersion * 4 + 17) + 7) / 8 + 1];
  int size = 0;
  bool module(int x, int y) const;
};
bool qrEncodeText(const char* text, QrCode& out);
}  // namespace tb
