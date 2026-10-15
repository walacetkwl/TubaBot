// Perfil de placa do TubaBot v3 — ÚNICO lugar com pinos e capacidades.
// Placa: ES3C28P (LCDWIKI) — ESP32-S3R8 (16 MB flash, 8 MB PSRAM), tela IPS 2,8"
// ILI9341V 240×320 por SPI, toque capacitivo FT6336G, codec ES8311 + amplificador
// FM8002E (alto-falante) com microfone, LED RGB WS2812, bateria LiPo opcional.
// Fontes: LCDWIKI (2.8inch_ESP32-S3_Display), BSP ngttai/esp32_s3_es3c28p e o
// perfil ES3C28P do bmorcelli/Launcher (conferidos entre si).
//
// A TELA É USADA EM PAISAGEM: interface lógica 320×240 (painel girado 90°).
#pragma once

// ── Tela ILI9341V (SPI) ─────────────────────────────────────────────────────
#define TB_LCD_CS 10
#define TB_LCD_DC 46
#define TB_LCD_SCK 12
#define TB_LCD_MOSI 11
#define TB_LCD_MISO 13
#define TB_LCD_BL 45            // luz de fundo (PWM, alto = acesa)
#ifndef TB_LCD_SPI_HZ
#define TB_LCD_SPI_HZ 40000000  // 40 MHz (o BSP oficial usa o mesmo)
#endif
#ifndef TB_LCD_IPS
#define TB_LCD_IPS 1            // painel IPS: inversão de cores ligada
#endif
// Rotação do controlador (Arduino_GFX): 1 = paisagem (USB à esquerda), 3 = paisagem invertida.
// "Girar 180°" nas configurações alterna entre 1 e 3 sem recompilar.
#ifndef TB_LCD_ROTATION
#define TB_LCD_ROTATION 1
#endif

// ── Barramento I²C (toque + codec) ──────────────────────────────────────────
#define TB_I2C_SDA 16
#define TB_I2C_SCL 15
#define TB_I2C_HZ 400000
#define TB_ADDR_FT6336 0x38
#define TB_ADDR_ES8311 0x18

// ── Toque FT6336G ───────────────────────────────────────────────────────────
#define TB_TP_RST 18
#define TB_TP_INT 17
// Coordenadas cruas do FT6336G estão no referencial RETRATO do painel (240×320).
// Conversão para a interface em paisagem 320×240 (rotação 1). Conferir na bancada
// com o comando "toque" (canto superior esquerdo da tela deve dar ~0,0).
#ifndef TB_TOUCH_MAP
#define TB_TOUCH_MAP(rx, ry, x, y) do { x = (ry); y = 239 - (rx); } while (0)
#endif

// ── Áudio ES8311 (alto-falante + microfone), I²S ────────────────────────────
#define TB_I2S_MCLK 4
#define TB_I2S_BCLK 5
#define TB_I2S_WS 7
#define TB_I2S_DOUT 6    // ESP32 -> ES8311 (alto-falante)
#define TB_I2S_DIN 8     // ES8311 -> ESP32 (microfone)
#define TB_PA_EN 1       // habilita o amplificador FM8002E
#ifndef TB_PA_ACTIVE_LOW
#define TB_PA_ACTIVE_LOW 1   // LCDWIKI: nível baixo liga o áudio
#endif
#define TB_AUDIO_RATE 16000

// ── Bateria (só no TubaBot Pro; carregador TP4054 na placa) ─────────────────
#define TB_BAT_ADC 9     // divisor 1:2 (R14/R15)
#define TB_BAT_DIV 2

// ── Outros ──────────────────────────────────────────────────────────────────
#define TB_BTN_BOOT 0    // ativo em BAIXO
#define TB_RGB_LED 42    // WS2812 (mantido apagado)

// ── Partição do perfil de fábrica (ver firmware/partitions_tubabot.csv) ─────
#define TB_FACTORY_PART_NAME "fabrica"
#define TB_FACTORY_PART_SUBTYPE 0x40

// ── Rede própria ───────────────────────────────────────────────────────────
#define TB_AP_IP_OCTETS 192, 168, 4, 1
#define TB_HTTP_MAX_BODY 4096
#define TB_STA_CONNECT_TIMEOUT_MS 20000
