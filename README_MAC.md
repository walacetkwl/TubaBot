# TubaBot v2 no Mac (Apple Silicon ou Intel)

## 1. Pré-requisitos

```bash
xcode-select --install          # clang++, make, git e python3 da Apple
```

Opcionais:

| Ferramenta | Para quê | Instalação |
|---|---|---|
| PlatformIO | compilar e gravar o firmware | `brew install platformio` |
| Node.js 18+ | equivalência nativo×WASM e teste de ponta a ponta | `brew install node` |
| LLVM + LLD | **recompilar** o simulador depois de mudar `core/` | `brew install llvm lld` |
| Pillow | gerar a galeria de telas | `python3 -m pip install --user pillow` |

O simulador já vem compilado (`simulator/tubabot_core.wasm.js`); o LLVM só é necessário se você alterar o núcleo.

## 2. Simulador (sem as peças)

```bash
open simulator/index.html
```

Funciona com duplo clique, sem servidor e sem internet (exceto o clima, que usa a Open-Meteo de verdade se você ligar). Se o navegador restringir algo em `file://`, use `python3 -m http.server 8000` e abra `http://localhost:8000/simulator/`.

No simulador:

- **Tela redonda**: clique = toque; clique duplo; segure = toque longo; arraste = deslizar; vai-e-vem rápido = carinho. Botões **BOOT**, **BOOT (segurar 3 s)** e **PWR** ao lado, como na placa.
- **Controle**: alto-falante, motor opcional (GPIO16) e bateria presentes ou não — "Aplicar e reiniciar". *Ferramentas de desenvolvedor* usam um perfil de teste que não é gravado.
- **Sensores**: arraste para inclinar; sacudir, pegar no colo, virar, deitar, ponta-cabeça; nível de som, palma, barulho, música a 120 bpm, ou o microfone do computador (só o nível; nada é gravado).
- **Energia**: carga da bateria e USB-C conectado.
- **Tempo**: relógio virtual 1×/10×/60×/300× (jogos sempre em 1×), saltos e "Enviar hora do computador". A hora é mantida como pelo RTC.
- **Falhas**: reiniciar, perda de energia, preferências corrompidas, apagar dados, três timers terminando juntos.
- **Portal**: a mesma página que o ESP32 serve em `192.168.4.1`, falando com o núcleo simulado pela mesma API.
- **Eventos**: log com horário virtual.

Para testar o portal como no celular: `node tools/portal_server.mjs` (acrescente `--sem-som` para silenciar).

Recompilar o núcleo do simulador depois de editar `core/`:

```bash
make sim
```

## 3. Testes

```bash
make test         # 55 testes nativos do núcleo
make crosscheck   # nativo × WebAssembly: telas idênticas (requer node)
make screens      # docs/telas.png com 31 telas reais (requer Pillow)
make e2e          # simulador e portal no Chromium via Playwright (npm i -D playwright)
```

## 4. Gravar o firmware

### 4.1 Pelo navegador (mais fácil)

1. Chrome ou Edge → <https://espressif.github.io/esptool-js/>
2. Ligue a placa no USB-C, clique **Connect** e escolha a porta (ex.: "USB JTAG/serial debug unit").
3. Endereço **0x0**, arquivo `firmware/dist/tubabot-v2.0.0-completo.bin` (ou `tubabot_vibra-v2.0.0-completo.bin` se ligou o motor MV50).
4. **Program**. Ao terminar, aperte RESET ou desplugue e plugue.

### 4.2 Pelo PlatformIO

| Ambiente | Modelo |
|---|---|
| `tubabot` | padrão (alto-falante, sem motor) |
| `tubabot_vibra` | com motor MV50 no GPIO16 |

```bash
pio run -e tubabot -e tubabot_vibra      # compila (o portal é recompactado antes)
pio device list                          # descubra a porta (/dev/cu.usbmodem…)
make flash ENV=tubabot PORT=/dev/cu.usbmodemXXXX
make monitor PORT=/dev/cu.usbmodemXXXX
make dist                                # regenera firmware/dist
```

A primeira compilação baixa a plataforma pioarduino 53.03.13 (Arduino-ESP32 3.1.3) e as bibliotecas fixadas em `platformio.ini`.

### 4.3 Atualização pelo portal (OTA)

Com o TubaBot já funcionando: portal → **Atualizar firmware** → `firmware/dist/tubabot-v2.0.0-app-ota.bin`. Preferências, recordes e conquistas são mantidos.

### Modo de gravação (BOOT)

Se a porta não aparecer ou a gravação falhar: segure **BOOT**, plugue o USB (ou toque RESET), solte BOOT e grave de novo. Funciona mesmo com firmware travado.

### Console serial de recuperação (115200)

| Comando | Efeito |
|---|---|
| `config` | reabre a rede própria do TubaBot (não apaga nada) |
| `wifi-esquecer` | apaga só a rede de casa |
| `i2c` | lista os dispositivos I²C e o estado de tela, toque, IMU, PMU, RTC, áudio |
| `toque` | coordenadas do toque atual (conferir orientação) |
| `imu` | gravidade no referencial da tela (conferir eixos) |
| `som` | nível do microfone e toca o som de abertura |
| `tela-teste` | padrão de teste na tela |
| `status` | estado completo em JSON + memória livre |
| `reiniciar` | salva e reinicia |

Também sem computador: **BOOT por 3 s** reabre a rede de configuração.

### Apagar tudo

`pio run -e tubabot -t erase` (ou *Erase Flash* na esptool-js) apaga a flash inteira, inclusive preferências e conquistas. Depois grave de novo.

## 5. Solução de problemas

| Sintoma | O que verificar |
|---|---|
| Porta USB não aparece | cabo de **dados**, outra porta, modo BOOT |
| Tela preta | `i2c` no console; brilho em Ajustes; PWR acorda a tela |
| Toque em lugar errado | `toque`; ajuste `TB_TOUCH_MIRROR_X/Y` / `TB_TOUCH_SWAP_XY` em `board_profile.h` (veja BANCADA.md) |
| Olhos inclinam para o lado errado / reações de movimento estranhas | `imu`; ajuste `TB_IMU_MAP` |
| Sem som | alto-falante no conector SPK; Volume e Silencioso em Ajustes; `som` |
| Não liga só na bateria | polaridade do plugue; bateria com proteção carregada (deixe 1 h no USB) |
| Tela de cabeça para baixo no corpo | Ajustes → Girar 180° |
| Celular não abre a página | conecte em `TubaBot-XXXX` e digite `http://192.168.4.1` (o aviso "sem internet" é esperado) |
| Clima não aparece | Wi-Fi de casa conectado, clima ligado e cidade escolhida no portal |
