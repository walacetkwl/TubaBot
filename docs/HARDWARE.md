# Hardware do TubaBot v3 — ES3C28P

O TubaBot definitivo usa **uma única placa pronta**, a **ES3C28P** (LCDWIKI / QDtech): ESP32-S3 + tela IPS de 2,8" com toque capacitivo, áudio e carregador de bateria. Não usamos mais OLED 0,96", ESP32-C3 separado, TTP223, buzzer nem motor de vibração — **toda a interação é pela tela de toque**.

> Compre a **ES3C28P** (o "C" é de *capacitive*). A **ES3N28P** é a mesma placa **sem toque**. A "ESP32-S3-Touch-LCD-2.8" da Waveshare é outra placa, com outros pinos.

Lista de compras com links: [`lista_de_materiais.xlsx`](lista_de_materiais.xlsx).

## 1. O que vem na placa

| Bloco | Componente | Pinos (GPIO) | Uso no TubaBot |
|---|---|---|---|
| Processador | ESP32-S3R8, 240 MHz, 8 MB PSRAM, 16 MB flash | — | tudo |
| Tela | IPS 2,8" 240×320, **ILI9341V** (SPI) | CS 10, DC 46, SCK 12, MOSI 11, MISO 13, RST = reset da placa, luz de fundo 45 (PWM) | **usada em paisagem 320×240** (controlador girado 90°) |
| Toque | **FT6336G** capacitivo, I²C 0x38 | SDA 16, SCL 15, RST 18, INT 17 | todos os gestos |
| Áudio | **ES8311** (I²C 0x18) + amplificador FM8002E + microfone | I²S MCLK 4, BCLK 5, LRCK 7, saída 6, entrada 8; habilita amplificador 1 (ativo em baixo) | sons e nível do microfone |
| Bateria | conector BAT + carregador **TP4054** (~0,5 A) | tensão no ADC 9 (divisor 1:2) | só no **Pro** |
| LED RGB | WS2812 | 42 | mantido apagado |
| microSD | slot (SDIO) | 38/40/39/41/48/47 | não usado |
| Botões | BOOT (GPIO0) e RESET | — | BOOT: curto = apagar/acender ou voltar ao rosto; 3 s = configuração |
| Expansão | conector com IO2/3/14/21 + I²C | — | livre |

Pinos e polaridades ficam num só lugar: [`firmware/include/board_profile.h`](../firmware/include/board_profile.h). Fontes conferidas entre si: LCDWIKI, BSP `ngttai/esp32_s3_es3c28p` e o perfil ES3C28P do `bmorcelli/Launcher`.

## 2. Lite × Pro

| | TubaBot Lite | TubaBot Pro |
|---|---|---|
| Placa, tela, toque, alto-falante, microfone, Wi-Fi | sim | sim |
| Bateria LiPo 3,7 V (1000–1500 mAh) | **não** | **sim** (conector BAT) |
| Alimentação | USB-C | USB-C + bateria |
| Firmware | **o mesmo** | **o mesmo** |
| Como a unidade "sabe" | perfil de fábrica `modelo=lite` | perfil de fábrica `modelo=pro` |

Veja [`FABRICA.md`](FABRICA.md).

## 3. O que você conecta

| Item | Conector | Cuidados |
|---|---|---|
| Alto-falante 8 Ω 1,5 W (ou 4 Ω 2 W) | SPEAKER (2 vias) | confira se é MX1.25 ou PH2.0 na sua placa |
| Bateria LiPo 3,7 V com proteção (só Pro) | BAT (2 vias) | **meça a polaridade com o multímetro antes de plugar**; o `+` está serigrafado na placa |
| Cabo USB-C de dados | USB-C | gravação, alimentação e carga |

## 4. Montagem no corpo

1. **Antes de montar**, grave firmware + perfil de fábrica ([`FABRICA.md`](FABRICA.md)) e rode o roteiro de [`BANCADA.md`](BANCADA.md) com a placa solta.
2. Prenda a placa pelos 4 furos (parafusos M2/M2,5), tela para fora, **na horizontal**. Se a imagem ficar de cabeça para baixo, use Sistema → **Girar 180°** (gira imagem e toque, sem recompilar).
3. Alto-falante na cavidade com furos do corpo; Pro: bateria com fita dupla-face fina, longe de parafusos e cantos vivos.
4. Deixe o botão BOOT acessível por um furo (é a recuperação sem toque).

## 5. Energia (Pro)

- A placa mede a **tensão** da bateria (ADC); a porcentagem vem de uma curva de LiPo. A placa **não informa se está carregando**, então o TubaBot não mostra o raio de carga.
- Consumo típico da placa (LCDWIKI): ~140 mA só com a tela; luz de fundo ~79 mA no máximo. Com 1000 mAh, espere **algumas horas** de uso contínuo; brilho mais baixo e "Apagar a tela" aumentam bastante. **Medir na bancada.**

## 6. Pontos a confirmar na bancada

| Item | Por quê | Como conferir | Onde ajustar |
|---|---|---|---|
| Orientação da imagem | a rotação 1 do ILI9341 deve dar paisagem com o "topo" no lugar certo | comando `tela-teste`: ponto vermelho no canto superior **esquerdo** | `TB_LCD_ROTATION` (1 ou 3) ou Girar 180° |
| Mapa do toque | as coordenadas cruas do FT6336G são do painel em retrato | comando `toque`: canto superior esquerdo ≈ (0,0), inferior direito ≈ (319,239) | `TB_TOUCH_MAP` |
| Cores | painel IPS precisa de inversão; ordem BGR | `tela-teste`: barras branco, amarelo, ciano, verde, magenta, vermelho, azul | `TB_LCD_IPS` |
| Amplificador | a LCDWIKI diz "nível baixo liga"; um BSP diz o contrário | comando `som` | `TB_PA_ACTIVE_LOW` |
| Microfone | ganho depende da montagem | `som` com palmas | ganho do ES8311 em `hw.cpp` |
| Bateria (Pro) | divisor 1:2 e curva | comparar Sobre → Sensores com o multímetro | `TB_BAT_DIV` |
