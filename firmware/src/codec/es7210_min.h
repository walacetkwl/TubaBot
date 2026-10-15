// Inicialização mínima do ADC de microfones ES7210 (modo escravo, I²S padrão,
// 16 bits, MIC1+MIC2). Sequência de registradores portada do driver oficial
// da Espressif (esp_codec_dev/device/es7210/es7210.c, Apache-2.0).
#pragma once
#include <stdint.h>

bool es7210MinInit(uint8_t addr, uint8_t gainCode /* 0..14 = 0..42 dB, passos de 3 dB */);
