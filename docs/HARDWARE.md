# Hardware do TubaBot v2 — placa, corpo, ligações e montagem

O TubaBot v2 troca o conjunto da v1 (ESP32-C3 + OLED 0,96" + TTP223 + buzzer) por **uma única placa pronta**, a mesma usada pelo corpo de referência (Neo Deskimon, da Creative Chance):

**Waveshare ESP32-S3-Touch-AMOLED-1.75** — SKU 31261 (sem GPS) · página oficial: <https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm>

> Atenção na compra: a Waveshare vende três versões desta placa — **31261** (padrão, a que o corpo Neo usa), **31262** ("-B", com case próprio) e **31264** ("-G", com GPS). Existe ainda a **1.75C**, um modelo diferente com caixa de alumínio. O firmware serve nas versões 31261, 31262 e 31264 (mesmos periféricos; a 1.75C não foi verificada), mas **compre a 31261 "pelada"** para caber no corpo. Confira a foto do anúncio: tela redonda com moldura preta, placa atrás com USB-C, dois botões laterais (BOOT e PWR), slot microSD e conectores MX1.25 para bateria e alto-falante.

A lista de compras completa, com links do Mercado Livre e da Shopee, está em [`lista_de_materiais.xlsx`](lista_de_materiais.xlsx).

## 1. O que vem na placa (e o que o TubaBot usa)

| Bloco | Componente | Endereço / pinos | Uso no TubaBot v2 |
|---|---|---|---|
| Processador | ESP32-S3R8, 240 MHz, 2 núcleos, **8 MB PSRAM** octal, **16 MB flash** | — | tudo; framebuffer 466×466 na PSRAM |
| Tela | AMOLED 1,75" **466×466**, controlador **CO5300** (QSPI) | CS 12, SCLK 38, D0–D3 4/5/6/7, RST 39, TE 13 | rosto colorido, relógio, apps |
| Toque | **CST9217** capacitivo (substitui o TTP223 da cabeça) | I²C 0x5A, INT 11, RST 40 | todos os gestos: toque, duplo, longo, deslizar, carinho |
| Movimento | **QMI8658** acelerômetro + giroscópio | I²C 0x6B, INT2 21 | chacoalhar, pegar no colo, virar de cara, nível, olhar inclinado |
| Energia | **AXP2101** PMU + carregador de LiPo | I²C 0x34 | % da bateria, carregando, USB presente |
| Relógio | **PCF85063** RTC | I²C 0x51 | hora mantida sem Wi-Fi (com bateria) |
| Alto-falante | **ES8311** codec + amplificador **NS4150B** | I²C 0x18, PA GPIO46, I²S MCLK 42 BCLK 9 WS 45 DOUT 8 | sons sintetizados (16 kHz) |
| Microfones | **ES7210** ADC, 2 microfones | I²C 0x40, I²S DIN 10 | nível de som: palmas, barulho, música (sem gravar voz) |
| Expansor | **TCA9554** | I²C 0x20 (P4 = botão PWR) | botão PWR |
| Botões | BOOT (GPIO0) e PWR | — | BOOT curto = acordar/voltar; BOOT 3 s = rede de configuração; PWR = dormir/acordar |
| Barramento | I²C SDA **15**, SCL **14**, 400 kHz | — | compartilhado por todos acima |

Todos os pinos e polaridades ficam em **um só lugar**: [`firmware/include/board_profile.h`](../firmware/include/board_profile.h). Fonte: o `HARDWARE_REFERENCE.md` e os exemplos do repositório oficial `waveshareteam/ESP32-S3-Touch-AMOLED-1.75`.

## 2. O que você conecta

| Item | Conector na placa | Cuidados |
|---|---|---|
| Bateria LiPo 3,7 V **803040 1000 mAh** (cabe no corpo Neo) | **MX1.25 2 pinos** "BAT" | **Confira a polaridade antes de plugar.** Baterias de anúncio vêm com fios em ordem diferente; o vermelho tem de ir no `+` serigrafado na placa. Se estiver invertido, troque os pinos do plugue com uma agulha (levante a trava). Exija bateria **com circuito de proteção (PCM)**. Uma bateria até ~1200 mAh com até 8 × 30 × 40 mm entra. |
| Alto-falante **8 Ω, 1–2 W**, ~28 mm (ou cápsula retangular 2030/2835) | **MX1.25 2 pinos** "SPK" | Sem polaridade crítica. Se vier com outro plugue (PH2.0, JST), corte e solde um rabicho MX1.25. |
| Motor de vibração MV50 (opcional, **da v1**) | Barra de expansão: **GPIO16**, 3V3, GND | Só no firmware `tubabot_vibra`. O MV50 tem transistor próprio; ligue IN→GPIO16, VCC→3V3, GND→GND. |
| Cabo USB-C de **dados** (o Ugreen da v1 serve) | USB-C | Alimenta, carrega a bateria e grava o firmware. |

> **MX1.25 ≠ PH2.0 ≠ JST-SH 1.0.** São parecidos e não encaixam entre si. O kit de rabichos MX1.25 2 pinos da lista resolve qualquer diferença.

Não é preciso soldar nada na placa para o uso básico. O ferro de solda da v1 só entra se precisar adaptar plugue da bateria/alto-falante ou ligar o motor.

## 3. Corpo (Neo Deskimon)

- Arquivos 3D do corpo **Neo** (capuz com fones de ouvido) vendidos pela Creative Chance no Thangs (~US$ 8). Imprimir em **PLA** (0,2 mm, 15–20 % de preenchimento, suporte só onde o fatiador indicar — normalmente na abertura da tela e nos fones).
- Sem impressora: serviços de impressão 3D sob encomenda no Mercado Livre/Shopee (envie o STL; peça PLA, a cor que quiser).
- O corpo foi feito para a placa 1.75 com bateria 803040 e alto-falante pequeno atrás da tela. Os ímãs de neodímio 8×2 mm da v1 podem fixar a tampa traseira (cole com cianoacrilato nos furos, se o modelo prever) — opcional.

### Montagem sugerida

1. **Antes de montar**, grave o firmware e rode o roteiro de [`BANCADA.md`](BANCADA.md) com a placa solta (tela, toque, sensores, som, bateria).
2. Plugue o alto-falante (SPK) e a bateria (BAT, **polaridade conferida**). Prenda a bateria com fita dupla-face fina no fundo do corpo, longe de parafusos e cantos vivos.
3. Encaixe a placa pela frente, tela para fora, USB-C alinhado com a abertura do corpo e os dois botões alinhados aos furos laterais.
4. Passe os fios sem esmagar contra a placa. O alto-falante vai na cavidade com furos (os "fones").
5. Feche a tampa traseira. Ligue com PWR (ou plugando o USB-C).
6. Se o corpo deixar a tela de cabeça para baixo, ative **Ajustes → Girar 180°** (gira imagem e toque juntos).

## 4. Peças da v1 que continuam úteis

| Peça da v1 | Na v2 |
|---|---|
| Cabo Ugreen USB-C | sim — alimentação/gravação |
| MV50 + jumpers fêmea-fêmea | sim — vibração opcional no GPIO16 |
| Multímetro | sim — conferir polaridade da bateria |
| Ferro de solda + estanho | só se precisar adaptar plugues |
| Ímãs 8×2 mm | opcional, fecho da tampa |
| ESP32-C3, OLED, TTP223, buzzer ativo, barra de pinos | não são usados (a v1 continua funcionando com eles) |

## 5. Energia

- Consumo estimado (tela AMOLED tem consumo proporcional ao brilho e aos pixels acesos; o rosto é majoritariamente preto): ~90–150 mA ativo com Wi-Fi, menos com brilho baixo; tela apagada no modo dormir.
- 1000 mAh ≈ 6–10 h de uso contínuo, mais com "Brilho reduzido à noite" e virando o TubaBot de tela para baixo (apaga a tela). **Valores estimados — medir na bancada** (o `status` mostra mV e % lidos do AXP2101).
- O carregamento é feito pelo AXP2101 quando o USB-C está ligado; o TubaBot mostra o raio na bateria e uma animação ao plugar.

## 6. Pontos a confirmar na bancada

| Item | Por quê | Como conferir | Onde ajustar |
|---|---|---|---|
| Orientação do toque | o CST9217 pode vir espelhado em relação à imagem | comando `toque`: canto superior esquerdo ≈ (0,0) | `TB_TOUCH_MIRROR_X/Y`, `TB_TOUCH_SWAP_XY` |
| Eixos do IMU | o mapeamento foi deduzido do esquema, não medido | comando `imu`: em pé y≈+1000; deitado com a tela para cima z≈−1000 | `TB_IMU_MAP` |
| Volume do alto-falante | depende do alto-falante comprado | Ajustes → Volume; comando `som` | ganho do ES8311 em `hw.cpp` |
| Sensibilidade dos microfones | idem | comando `som` com palmas | ganho do ES7210 em `hw.cpp` (30 dB) |
| Polaridade da bateria | depende do anúncio | multímetro antes de plugar | — |
