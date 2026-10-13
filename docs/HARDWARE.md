# Hardware do TubaBot — compras, ligação provisória e pendências

> **Mapa PROVISÓRIO.** Os anúncios do Mercado Livre não puderam ser abertos durante a preparação do briefing nem nesta implementação. Tudo marcado "a confirmar no módulo recebido" deve ser medido na bancada antes de ligar o módulo ao ESP32. Todos os pinos e polaridades estão centralizados em `firmware/include/board_profile.h`.

## 1. Itens comprados (11 links)

| Item | Papel | Link |
|---|---|---|
| ESP32-C3 Super Mini, Wi-Fi/Bluetooth, USB-C, com solda | Controlador | https://produto.mercadolivre.com.br/MLB-7631225428-placa-esp32-c3-super-mini-wifi-bluetooth-usb-c-com-solda-_JM?quantity=1&sid=purchases |
| Multímetro Kapbom DT-9205A | Ferramenta de bancada | https://www.mercadolivre.com.br/multimetro-digital-kapbom-dt-9205a-com-capacimetro-e-bipe-sonoro/p/MLB27119761?pdp_filters=item_id:MLB4868916925&sid=purchases |
| Barra de pinos macho 1×40, 2,54 mm (10 un.) | Conexões | https://www.mercadolivre.com.br/barra-de-pinos-macho-1x40-vias-254mm-180-10-pecas/p/MLB42966772?pdp_filters=item_id:MLB5411364604&sid=purchases |
| Módulo de vibração motor DC PWM MV50 | Vibração (modelo completo) | https://www.mercadolivre.com.br/modulo-de-vibracao-motor-dc-pwm-mv50-para-arduino/p/MLB44762532?pdp_filters=item_id:MLB5222231244&sid=purchases |
| Kit ferro de solda 60 W e estanho | Ferramenta | https://produto.mercadolivre.com.br/MLB-1962565547-kit-ferro-de-solda-soldar-soldador-60w-estanho-tubo-_JM?quantity=1&variation_id=92480959303&sid=purchases |
| Cabo Ugreen US287 USB 2.0 → USB-C, 1,5 m | Alimentação, gravação, serial | https://www.mercadolivre.com.br/cabo-de-dados-ugreen-us287-usb-20-p-usb-tipo-c-15m-preto/p/MLB19951731?pdp_filters=item_id:MLB4975518187&sid=purchases |
| Módulo buzzer **ativo** | Bipes rítmicos (modelo completo) | https://www.mercadolivre.com.br/modulo-buzzer-ativo-p-arduino/p/MLB2064185857?pdp_filters=item_id:MLB6523999990&sid=purchases |
| Jumpers fêmea-fêmea 10 cm (40 un.) | Cabeamento | https://www.mercadolivre.com.br/cabo-jumper-10cm-femea-x-femea-40-unid-picarduino/p/MLB35714102?pdp_filters=item_id:MLB3668056791&sid=purchases |
| Kit 10 sensores capacitivos TTP223 | **Um** sensor por robô (cabeça) | https://www.mercadolivre.com.br/10x-teclado-sensor-touch-toque-capacitivo-ttp223-arduino/p/MLB2050795406?pdp_filters=item_id:MLB3339935349&sid=purchases |
| OLED branco 0,96" I²C 128×64 | Visor (SSD1306 presumido) | https://www.mercadolivre.com.br/display-oled-096-branco-grafico-i2c-128x64-arduino/up/MLBU749499347?pdp_filters=item_id:MLB2653122128&sid=purchases |
| Ímãs de neodímio 8×2 mm N35 (20 un.) | Mecânica, sem função eletrônica | https://www.mercadolivre.com.br/ima-neodimio-8x2mm-20-unidades-disco-n35/p/MLB72729571?pdp_filters=item_id:MLB4744742039&sid=purchases |

Não há microfone, alto-falante, câmera, acelerômetro, sensor de temperatura ambiente, bateria nem RTC com bateria — o software não promete nada que dependa deles.

## 2. Mapa de pinos compilado (provisório)

| Sinal | GPIO | Alimentação do módulo | Nível lógico do sinal | Polaridade compilada | Status |
|---|---|---|---|---|---|
| OLED SDA | **4** | 3,3 V (pino 3V3) | 3,3 V (pull-ups do módulo) | — | a confirmar |
| OLED SCL | **6** | 3,3 V | 3,3 V | — | a confirmar |
| TTP223 OUT | **7** | 3,3 V | 3,3 V (saída push-pull do TTP223) | ativo em ALTO | a confirmar (jumpers A/B) |
| Buzzer ativo IN | **5** | ver §3.3 | 3,3 V do GPIO | ativo em ALTO | **crítico confirmar** |
| MV50 IN | **10** | ver §3.4 | 3,3 V do GPIO | ativo em ALTO | **crítico confirmar** |
| USB D−/D+ | 18/19 | — | — | — | reservado (USB nativo) |

Por que este mapa:

- A referência (YETI) usa **SDA 8 / SCL 9**. No ESP32-C3, GPIO2, GPIO8 e GPIO9 são *strapping pins* (definem o modo de boot); na Super Mini, GPIO8 ainda tem o LED azul e GPIO9 é o botão BOOT. Um pull-up/pull-down errado no barramento I²C nesses pinos pode impedir o boot normal. Por isso o mapa usa 4/6.
- GPIO18/19 são o USB nativo (gravação e serial). GPIO20/21 ficam livres para a UART de depuração.
- GPIO4–7 também são os pinos de JTAG por pads; por padrão o C3 usa JTAG via USB e eles funcionam como GPIO comum — confirmar que nenhum módulo pisca/bipa no reset.
- O "fallback" do touch para GPIO5 da referência **não** foi aplicado: GPIO5 é do buzzer.

**Nunca aplique 5 V em um GPIO do ESP32-C3.** Alimente OLED e TTP223 em **3,3 V**, para que os pull-ups do I²C e a saída do TTP223 fiquem em 3,3 V.

## 3. Pendências por módulo (medir antes de ligar)

### 3.1 OLED 0,96"
- [ ] Controlador: SSD1306 (presumido) ou SH1106? Se a imagem sair deslocada 2 colunas, compilar com `-DTB_OLED_SH1106=1`.
- [ ] Endereço: o firmware testa 0x3C e 0x3D (`i2c` no console).
- [ ] Faixa de VCC do módulo (muitos têm regulador e aceitam 3,3–5 V) — usar 3,3 V.
- [ ] Orientação na carcaça: ajuste "Girar 180°" no portal.
- [ ] Custo real de envio ao I²C a 400 kHz (`i2c` mostra o último envio em µs). Estimativa teórica: ~23 ms para a tela inteira; o driver envia só as páginas alteradas.

### 3.2 TTP223
- [ ] Jumper **A**: aberto = saída ALTA ao tocar (padrão compilado). Fechado = ativa em BAIXO → `TB_TOUCH_ACTIVE_HIGH=0`.
- [ ] Jumper **B**: aberto = momentâneo (necessário). Fechado = toggle/retenção — o toque "trava" ligado; isso **não é defeito de software**.
- [ ] Sensibilidade através do plástico da cabeça (espessura da parede).
- [ ] Tempo de resposta do módulo (ver roteiro de bancada).

### 3.3 Buzzer ativo
- [ ] Tensão de trabalho (comum: 3,3–5 V).
- [ ] Existe transistor no módulo? NPN → ativo em ALTO; **PNP → ativo em BAIXO** (muito comum em módulos "low level trigger").
- [ ] **Atenção:** módulo com PNP alimentado em 5 V e comandado por GPIO de 3,3 V pode ficar **sempre ligado** (o 3,3 V não corta o PNP). Nesse caso alimente o módulo em 3,3 V ou use outro estágio.
- [ ] O buzzer é ATIVO: o firmware só liga/desliga em padrões rítmicos. Não há melodias, voz, volume ou "ronronar" sonoro realista.

### 3.4 MV50 (vibração)
- [ ] O módulo tem transistor de acionamento **e** diodo de roda-livre? (anúncio não confirmado)
- [ ] Tensão e corrente do motor (meça com o multímetro em série). Motores desse tipo costumam puxar dezenas de mA — mais do que um GPIO deve fornecer.
- [ ] Polaridade do sinal IN (ativo em ALTO compilado).
- [ ] Se o módulo for apenas o motor (sem driver): **não ligue ao GPIO**. Itens adicionais necessários (não comprados): transistor NPN (ex.: S8050/2N2222) ou MOSFET de nível lógico, resistor de base ~1 kΩ, diodo 1N4148/1N5819 em antiparalelo com o motor, e alimentação do motor pelo pino 5V da placa.
- [ ] Vibração gerando toques falsos no TTP223 (testar com o motor ligado).

### 3.5 ESP32-C3 Super Mini
- [ ] Flash real (o projeto assume **4 MB**, sem PSRAM): `pio run -t upload` mostra o tamanho detectado; o portal mostra a memória livre.
- [ ] LED onboard no GPIO8 (não usado pelo firmware).
- [ ] Botões BOOT (GPIO9) e RESET.

## 4. Partições (4 MB)

`firmware/partitions_tubabot.csv`: NVS 20 KB, otadata 8 KB, **duas apps de 1,875 MB** (OTA), coredump 64 KB. O tamanho do binário real só será conhecido no primeiro `pio run`; a verificação de compilação desta entrega mediu ~76 KB de código próprio (núcleo + adaptadores), sem contar o framework (Wi-Fi/HTTP), que tipicamente ocupa a maior parte de uma imagem de ~0,9–1,1 MB nesta plataforma (estimativa, não medida aqui).
