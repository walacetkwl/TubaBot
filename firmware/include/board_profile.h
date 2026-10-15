// Perfil de placa do TubaBot v2 — ÚNICO lugar com pinos e capacidades.
// Placa: Waveshare ESP32-S3-Touch-AMOLED-1.75 (SKU 31261/31262).
// Fonte: HARDWARE_REFERENCE.md do repositório oficial da Waveshare
// (waveshareteam/ESP32-S3-Touch-AMOLED-1.75), conferido com o esquema e o BSP.
#pragma once

// ── Tela AMOLED 1,75" 466×466 (CO5300, QSPI) ────────────────────────────────
#define TB_LCD_CS 12
#define TB_LCD_SCLK 38
#define TB_LCD_D0 4
#define TB_LCD_D1 5
#define TB_LCD_D2 6
#define TB_LCD_D3 7
#define TB_LCD_RST 39
#define TB_LCD_TE 13
#define TB_LCD_COL_OFFSET 6       // deslocamento de coluna do painel (exemplos oficiais)
#ifndef TB_LCD_QSPI_HZ
#define TB_LCD_QSPI_HZ 80000000   // 80 MHz
#endif

// ── Barramento I²C compartilhado ────────────────────────────────────────────
#define TB_I2C_SDA 15
#define TB_I2C_SCL 14
#define TB_I2C_HZ 400000
// endereços (7 bits)
#define TB_ADDR_ES8311 0x18
#define TB_ADDR_TCA9554 0x20
#define TB_ADDR_AXP2101 0x34
#define TB_ADDR_ES7210 0x40
#define TB_ADDR_PCF85063 0x51
#define TB_ADDR_CST9217 0x5A
#define TB_ADDR_QMI8658 0x6B

// ── Toque (CST9217) ─────────────────────────────────────────────────────────
#define TB_TP_INT 11
#define TB_TP_RST 40
// Orientação do toque em relação à imagem (conferir na bancada com o comando "toque").
#ifndef TB_TOUCH_MIRROR_X
#define TB_TOUCH_MIRROR_X 1
#endif
#ifndef TB_TOUCH_MIRROR_Y
#define TB_TOUCH_MIRROR_Y 1
#endif
#ifndef TB_TOUCH_SWAP_XY
#define TB_TOUCH_SWAP_XY 0
#endif

// ── Movimento (QMI8658) ─────────────────────────────────────────────────────
#define TB_IMU_INT2 21
// Eixos do IMU -> referencial da TELA usado pelo núcleo (x direita, y baixo,
// z saindo da tela). Conferir com o comando "imu" e ajustar se preciso.
#ifndef TB_IMU_MAP
#define TB_IMU_MAP(ax, ay, az, gx, gy, gz) do { gx = -(ay); gy = -(ax); gz = (az); } while (0)
#endif

// ── Áudio (ES8311 alto-falante + ES7210 microfones), I²S compartilhado ───────
#define TB_I2S_MCLK 42
#define TB_I2S_BCLK 9
#define TB_I2S_WS 45
#define TB_I2S_DOUT 8    // para o ES8311
#define TB_I2S_DIN 10    // do ES7210
#define TB_PA_CTRL 46    // amplificador NS4150B
#define TB_AUDIO_RATE 16000

// ── Botões ──────────────────────────────────────────────────────────────────
#define TB_BTN_BOOT 0            // ativo em BAIXO
#define TB_EXIO_PWR_BIT 4        // TCA9554 P4: botão PWR (alto = pressionado)

// ── Vibração (opcional): módulo MV50 no conector de expansão (pino 8 = GPIO16) ─
#ifndef TB_HAS_MOTOR
#define TB_HAS_MOTOR 0
#endif
#define TB_PIN_MOTOR 16
#ifndef TB_MOTOR_ACTIVE_HIGH
#define TB_MOTOR_ACTIVE_HIGH 1
#endif

// ── Rede própria ───────────────────────────────────────────────────────────
#define TB_AP_IP_OCTETS 192, 168, 4, 1
#define TB_HTTP_MAX_BODY 4096
#define TB_STA_CONNECT_TIMEOUT_MS 20000
