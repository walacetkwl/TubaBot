// Perfil de placa do TubaBot — ÚNICO lugar com pinos, polaridades e capacidades.
// Mapa PROVISÓRIO: confirmar na placa ESP32-C3 Super Mini recebida (docs/HARDWARE.md).
#pragma once

// ── Modelo físico: vem do BUILD (env do PlatformIO), nunca de um botão no portal ──
#ifndef TB_MODEL_FULL
#error "Defina TB_MODEL_FULL=0 (TubaBot Lite) ou 1 (TubaBot) no platformio.ini"
#endif
#define TB_HAS_BUZZER TB_MODEL_FULL
#define TB_HAS_MOTOR  TB_MODEL_FULL

// ── Pinos (ESP32-C3) ───────────────────────────────────────────────────────
// Evitados de propósito:
//   GPIO2, GPIO8, GPIO9 -> strapping (GPIO8 também tem o LED da Super Mini; GPIO9 é o botão BOOT)
//   GPIO18, GPIO19      -> USB nativo (gravação e serial)
//   GPIO20, GPIO21      -> UART0 (reserva para depuração)
// A referência (YETI) usa SDA 8 / SCL 9: NÃO adotado aqui por serem strapping.
#ifndef TB_PIN_SDA
#define TB_PIN_SDA 4
#endif
#ifndef TB_PIN_SCL
#define TB_PIN_SCL 6
#endif
#ifndef TB_PIN_TOUCH
#define TB_PIN_TOUCH 7      // saída digital do TTP223 (modo momentâneo)
#endif
#ifndef TB_PIN_BUZZER
#define TB_PIN_BUZZER 5     // sinal do módulo de buzzer ATIVO (somente modelo completo)
#endif
#ifndef TB_PIN_MOTOR
#define TB_PIN_MOTOR 10     // sinal do módulo MV50 (somente modelo completo; ver HARDWARE.md)
#endif
// GPIO5 está reservado ao buzzer: o "fallback" do touch para GPIO5 da referência não se aplica.

// ── Polaridades (a confirmar com multímetro em cada módulo) ─────────────────
#ifndef TB_TOUCH_ACTIVE_HIGH
#define TB_TOUCH_ACTIVE_HIGH 1   // TTP223 padrão (jumper A aberto): saída ALTA ao tocar
#endif
#ifndef TB_BUZZER_ACTIVE_HIGH
#define TB_BUZZER_ACTIVE_HIGH 1  // muitos módulos de buzzer ativo com transistor PNP são ATIVOS EM BAIXO
#endif
#ifndef TB_MOTOR_ACTIVE_HIGH
#define TB_MOTOR_ACTIVE_HIGH 1
#endif

// ── OLED ───────────────────────────────────────────────────────────────────
#ifndef TB_OLED_SH1106
#define TB_OLED_SH1106 0         // 1 se o controlador for SH1106 (desloca 2 colunas)
#endif
#ifndef TB_I2C_HZ
#define TB_I2C_HZ 400000
#endif

// ── Rede própria ───────────────────────────────────────────────────────────
#define TB_AP_IP_OCTETS 192, 168, 4, 1
#define TB_HTTP_MAX_BODY 4096
#define TB_STA_CONNECT_TIMEOUT_MS 20000
