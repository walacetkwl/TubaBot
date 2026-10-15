# Inventário: referências consultadas, decisões e contradições

## Referências

| Fonte | Acesso | Uso |
|---|---|---|
| Neo Deskimon — Creative Chance (`creative-chance.com/en-brl/products/neo-deskimon`) | a página não abriu deste ambiente (DNS); informações por buscas e páginas de terceiros, out/2026 | corpo de referência, escolha da placa, ideias de comportamento (rosto expressivo, reações a toque/movimento/som, relógio, clima, carregamento) |
| Waveshare ESP32-S3-Touch-AMOLED-1.75 — wiki, página do produto, `waveshareteam/ESP32-S3-Touch-AMOLED-1.75` | GitHub, out/2026 | pinos, endereços I²C, deslocamento de coluna do CO5300, versões das bibliotecas |
| Arduino_GFX `v1.6.4`, SensorLib `v0.3.1`, XPowersLib `v0.2.6` | tags do GitHub | drivers de tela (CO5300), toque/IMU/RTC, PMU |
| esp-adf / esp-bsp: `es8311` (vendorizado) e `es7210` (porte mínimo) | GitHub | codecs de áudio — Apache-2.0, cabeçalhos de licença preservados em `firmware/src/codec/` |
| pioarduino `platform-espressif32` 53.03.13 (Arduino-ESP32 3.1.3) | GitHub releases | plataforma do firmware |
| Nayuki QR Code generator | GitHub | QR na tela — MIT, em `core/third_party/` |
| Nunito (Google Fonts) | GitHub | fonte do sistema — SIL OFL 1.1, `tools/fonts/OFL.txt`; convertida para 4 bpp por `tools/gen_fonts.py` |
| Open-Meteo | API pública, sem chave | clima atual e 3 dias |
| TubaBot v1 (este repositório, commit `58f7b35`) | — | toda a lógica preservada e portada |

Nenhum arquivo, código, imagem ou som do Neo Deskimon foi copiado: o rosto, os sons (sintetizados), as telas e os textos são do TubaBot. Os STLs do corpo são comprados pelo usuário com a Creative Chance.

## O que mudou da v1 para a v2

| v1 | v2 | Por quê |
|---|---|---|
| ESP32-C3 Super Mini + OLED 128×64 monocromático | ESP32-S3 + AMOLED redondo 466×466 colorido (placa única) | mesma placa do corpo Neo; PSRAM para framebuffer de 434 KB |
| Sensor TTP223 na cabeça (1 bit) | toque capacitivo na tela (coordenadas) | pedido; habilita deslizar, botões e carinho com direção |
| Buzzer ativo (liga/desliga) | alto-falante com sintetizador de 2 vozes | melodias, volume, sons por expressão |
| Sem sensores | IMU, microfones (só nível), bateria, RTC | reações a movimento/som, energia, hora sem Wi-Fi |
| Fonte bitmap 5×7 | Nunito antisserrilhada em 6 tamanhos | legibilidade na tela grande |
| 4 jogos | 5 jogos (+ Mergulho, controlado por inclinação) | uso do IMU |
| — | Hoje, Clima, Alarmes, Respiração, Nível, Sorteio, mostradores, ajustes rápidos | funções no estilo da referência e além |

Tudo o que a v1 fazia continua: expressões, carinho/amor, personalidades, desbloqueios, os 4 jogos, Pomodoro, 3 timers, cronômetro, lembretes, política de alertas, portal local com PIN, Wi-Fi de casa opcional, NTP, OTA, backup/restauração (aceita backup da v1), console serial de recuperação.

## Decisões e contradições resolvidas

1. **Toque único para tudo**: o classificador da v1 (decisão exclusiva na soltura) foi mantido e ampliado; gestos de tela não conflitam com carinho (carinho = vai-e-vem curto; deslizar = movimento longo numa direção).
2. **Acordar sem efeito colateral**: o primeiro toque com a tela apagada só acorda (600 ms de guarda), como a v1 fazia com o TTP223.
3. **Microfone sem privacidade em risco**: o firmware calcula só o nível (RMS) no próprio chip; nenhuma amostra é guardada, enviada ou exposta na API.
4. **Clima sem chave e sem servidor próprio**: Open-Meteo direto do aparelho, só quando ligado e com Wi-Fi de casa; cidade escolhida em uma tabela de 27 capitais (ou coordenadas manuais).
5. **HTTPS do clima sem verificação de certificado** (documentado): os dados são públicos e não sensíveis; evita embarcar e atualizar uma cadeia de certificados.
6. **Eixos do IMU e espelhamento do toque** foram deduzidos do esquema e dos exemplos, não medidos: ficam como macros em `board_profile.h` e a bancada confirma.
7. **Arduino-ESP32 3.x**: as bibliotecas da placa exigem o core 3.x; a plataforma oficial do PlatformIO parou no 2.x, por isso o pioarduino com versão fixada.
8. **Determinismo**: o mesmo núcleo gera framebuffers idênticos bit a bit no nativo e no WebAssembly (`-ffp-contract=off` nos dois).

## v3 — ES3C28P em paisagem (out/2026)

| Fonte | Uso |
|---|---|
| LCDWIKI — 2.8inch ESP32-S3 Display (ES3C28P/ES3N28P) | especificação, pinos, alimentação |
| BSP `ngttai/esp32_s3_es3c28p` | pinos, orientação do ILI9341V (IPS, BGR), alto-falante 8 Ω 1,5 W, bateria/TP4054 |
| `bmorcelli/Launcher` (perfil ES3C28P) | pinos I²S (saída 6, entrada 8), habilitação do amplificador ativa em baixo, mapa do toque por rotação |
| Arduino_GFX `v1.6.4` (Arduino_ILI9341) | driver da tela e rotações |

Decisões da v3:

1. **Paisagem 320×240** em todo o sistema (pedido): o controlador gira 90°, o toque é convertido de retrato para paisagem no firmware (`TB_TOUCH_MAP`), e "Girar 180°" usa a rotação 3 do controlador.
2. **Fábrica separada das preferências:** partição própria de 4 KB no fim da flash (fora do alcance da imagem completa), texto legível com CRC, lido pelo mesmo parser no firmware, simulador e testes; a ferramenta Python gera o mesmo texto byte a byte.
3. **Um firmware só:** Lite/Pro é só uma linha do perfil; o núcleo desliga tudo de bateria quando `modelo=lite` (inclusive na API).
4. **Unidade sem perfil** = Lite genérico (grafite, escuro) com aviso — mais seguro do que mostrar uma bateria inexistente.
5. **Tema claro** sem halo nos olhos e com brilhos em tom da cor (o halo em fundo branco vira mancha); mistura de cores com arredondamento ao mais próximo para não gerar tons esverdeados em RGB565.
6. **Sem IMU e sem RTC na placa:** a lógica de movimento continua no núcleo (testada, oculta por `caps.imu`); NTP passa a vir ligado por padrão.
7. Dados de pinos divergentes (habilitação do amplificador: LCDWIKI "baixo liga" × BSP "alto liga") ficam como opção `TB_PA_ACTIVE_LOW` para a bancada.
