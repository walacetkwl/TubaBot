# Passagem de bastão — leia antes de mexer no TubaBot

Este arquivo é para qualquer pessoa ou IA que continue o projeto. Ele resume **o que foi pedido, o que está pronto, as decisões tomadas e o que falta**. Os detalhes estão em `docs/`.

## 1. O produto (pedido da dona do projeto)

TubaBot é um companheiro de mesa (rosto expressivo + Pomodoro + timers + jogos) da TubaLabs. Requisitos **vigentes** da v3:

- **Placa:** ES3C28P (ESP32-S3, tela 2,8" IPS 240×320 com toque capacitivo FT6336G, ES8311 + alto-falante + microfone, USB-C, Wi-Fi, conector de bateria). **Sem** OLED, ESP32-C3, TTP223, buzzer ou motor: toda a interação é pela tela de toque.
- **Tela em PAISAGEM:** interface lógica **320×240** (painel girado 90°). Nada pode ser desenhado pensando em retrato. O toque é recalibrado para essa rotação.
- **Rosto em tela cheia** no estado normal, sem barra nem informações fixas. Controles aparecem ao tocar e somem sozinhos. Sem interação, tudo volta ao rosto.
- **Tema escuro (padrão) e claro**, salvo; as expressões são adaptadas aos dois temas (não é só inverter).
- **Cor das expressões** independente do tema (vermelho, azul, branco, grafite = cores físicas do produto, + outras).
- **Informações sobre o rosto** (hora, clima, bateria), cada uma com liga/desliga, **todas desligadas de fábrica**; bateria só existe no Pro.
- **Lite × Pro e cor física são propriedade de FÁBRICA**, nunca opção do usuário. Mesmo firmware para todas as unidades. Precisa existir "Restaurar padrão de fábrica" que volta aos padrões **daquela unidade**.
- Menu: Jogos, Pomodoro, Timer, Aparência (tema, cor das expressões, informações na tela), Volume, Brilho, Wi-Fi, Sistema (+ "Mais apps").
- Preservar tudo que já existia: rosto/animações/personalidades, os jogos (Pula Tuba, Tuba Reflexo, Hora do Bote, No Tempo Certo, Mergulho), Pomodoro, timers, sons, Wi-Fi, portal, preferências persistentes, OTA, simulador para Mac.
- Idioma de tudo (interface, docs, commits): **português do Brasil**.

Histórico: v1 = ESP32-C3 + OLED (commit `58f7b35`); v2 = Waveshare AMOLED 1,75" redonda (commit `47bd3e7`) — **abandonada**, a placa definitiva é a ES3C28P.

## 2. Estado atual (v3.0.0)

| Parte | Estado |
|---|---|
| Núcleo `core/` (320×240, temas, cores, fábrica, menu, jogos, API) | pronto, 62 testes passando (também com ASan/UBSan) |
| Simulador `simulator/` (WASM, aba Fábrica, corpo na cor da unidade) | pronto; equivalência nativo×WASM bit a bit; E2E 26/26 |
| Portal `web-ui/portal.html` | pronto (tema, cor, informações na tela, restaurar fábrica) |
| Firmware `firmware/` (ES3C28P) | **compila**, binários em `firmware/dist/`; **nunca rodou na placa física** |
| Ferramenta de fábrica `tools/fabrica.py` + 8 perfis em `firmware/fabrica/` | pronta e testada contra o núcleo (byte a byte) |
| Lista de materiais `docs/lista_de_materiais.xlsx` (gerada por `tools/gen_bom.py`) | pronta; preços são estimativas (ML/Shopee não puderam ser abertos) |

## 3. Próximo passo: bancada com a placa real

Seguir `docs/BANCADA.md`. Itens **não verificados** (cada um é ajuste de 1 linha em `firmware/include/board_profile.h`):

1. Orientação/cores da tela (`TB_LCD_ROTATION`, `TB_LCD_IPS`) — comando serial `tela-teste`.
2. Mapa do toque retrato→paisagem (`TB_TOUCH_MAP`, hoje `x = ry; y = 239 - rx`) — comando `toque` mostra coordenadas cruas e convertidas.
3. Polaridade do amplificador (`TB_PA_ACTIVE_LOW`, hoje 1; as fontes divergem) — comando `som`.
4. Ganho do microfone (ES8311, `hw.cpp`) e divisor/curva da bateria (`TB_BAT_DIV`).
5. Gravação real da partição `fabrica` (0xFEF000) e OTA pelo portal.

## 4. Arquitetura e decisões que não devem ser desfeitas sem motivo

- **Um núcleo C++17 freestanding** (sem STL/heap/exceções) compilado para firmware, WASM e testes. Regras de negócio só em `core/`; `firmware/` e `simulator/` são adaptadores finos.
- **Determinismo:** `-ffp-contract=off` em todos os alvos; `make crosscheck` exige CRC idêntico nativo×WASM. Não introduza diferenças de ponto flutuante entre alvos.
- **Perfil de fábrica** (`core/factory.*`): texto `chave=valor` com CRC-32 (zlib), partição `fabrica` de 4 KB em **0xFEF000** (fim da flash, fora do alcance do `…-completo.bin`). Padrões de fábrica são aplicados **antes** de carregar as preferências. Unidade sem perfil = Lite genérico (grafite, escuro). Chaves desconhecidas são ignoradas (compatibilidade futura). O `tools/fabrica.py` e o núcleo precisam gerar o **mesmo texto** (teste `factory_profile_from_python_tool`).
- **Lite:** `caps.battery = false` derivado do perfil → bateria some da UI, da API (`power.applicable:false`) e do portal.
- **Tema:** `ui::setTheme()` antes de cada quadro; no claro o halo dos olhos é desligado. A mistura de cores RGB565 arredonda ao mais próximo (evita tons esverdeados no fundo branco).
- **Sem IMU e sem RTC na ES3C28P:** a lógica de movimento continua no núcleo e nos testes, oculta por `caps.imu`; NTP vem ligado por padrão.
- **Cuidado (bug já encontrado):** em `s.put("crc=").hex(crc32(buf, s.len), 8)` o `s.len` já inclui `"crc="` (ordem de avaliação do C++17). Calcule o CRC numa variável antes.

## 5. Como trabalhar

```bash
make test         # testes nativos (rodar sempre)
make sim          # recompila o WASM do simulador após mudar core/ (precisa clang com alvo wasm32 + wasm-ld)
make crosscheck   # nativo × WASM (node)
make e2e          # simulador + portal no Chromium (playwright)
make screens      # docs/telas.png (Pillow)
make portal       # recompacta o portal para o firmware (o pio também faz)
pio run -e tubabot && ./tools/make_dist.sh   # firmware + firmware/dist
python3 tools/fabrica.py --todos             # perfis de fábrica
python3 tools/gen_bom.py docs/lista_de_materiais.xlsx
```

Ao alterar `core/`: rode `make test`, `make sim`, `make crosscheck` e, se mexer em tela, `make screens` e confira as imagens. Ao alterar o portal ou o simulador: `make e2e`. Firmware: pioarduino 53.03.13 (Arduino-ESP32 3.1.3), versões fixadas em `platformio.ini`.

## 6. Mapa da documentação

- `docs/FUNCIONALIDADES.md` — tudo que o TubaBot faz (gestos, menu, regras)
- `docs/FABRICA.md` — preparar cada unidade
- `docs/HARDWARE.md`, `docs/BANCADA.md` — placa, pinos, montagem, roteiro de teste
- `docs/API.md` — API local (mesma no ESP32 e no simulador)
- `docs/VALIDACAO.md` — o que foi provado e como
- `docs/INVENTARIO.md` — fontes consultadas e decisões
- `docs/IDEIAS.md` — próximas ideias (LED RGB, IMU opcional, voz, BLE...)
