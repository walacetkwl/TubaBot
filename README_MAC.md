# TubaBot no Mac (Apple Silicon ou Intel)

## 1. Pré-requisitos

```bash
xcode-select --install          # clang++, make, git e python3 da Apple
```

Opcionais:

| Ferramenta | Para quê | Instalação |
|---|---|---|
| PlatformIO | compilar e gravar o firmware | `brew install platformio` (ou `python3 -m pip install --user platformio`) |
| Node.js 18+ | teste de equivalência nativo×WASM e teste de ponta a ponta | `brew install node` |
| LLVM + LLD | **recompilar** o simulador depois de mudar `core/` | `brew install llvm lld` |

O clang da Apple não tem o alvo WebAssembly; por isso o simulador já vem com o `.wasm` compilado (`simulator/tubabot_core.wasm.js`). Você só precisa do LLVM do Homebrew se alterar o núcleo.

## 2. Simulador (agora, sem as peças)

```bash
open simulator/index.html
```

Funciona com duplo clique, sem servidor e sem internet. Se o navegador bloquear algo em `file://` (Safari às vezes restringe o armazenamento local), use um servidor local:

```bash
python3 -m http.server 8000
# abra http://localhost:8000/simulator/
```

No simulador:

- **Cabeça**: clique/toque e segure, ou use a **barra de espaço**. Segurar reproduz pressão longa; a repetição automática do teclado é ignorada; soltar fora da área, trocar de aba ou perder o foco solta o sensor virtual.
- **Controle**: modelo Lite/TubaBot (no aparelho real isso vem da gravação), som/silencioso/vibração, áudio do buzzer simulado (tom fixo liga/desliga, como o buzzer ativo comprado). *Ferramentas de desenvolvedor* ficam recolhidas e usam um perfil de teste que não é gravado.
- **Tempo**: relógio virtual 1×/10×/60×/300× (jogos sempre em 1×), saltos de +10 s a +25 min e "Enviar hora do Mac".
- **Falhas**: reiniciar, perda de energia, Wi-Fi de casa indisponível, preferências corrompidas, OLED sem resposta, sensor preso, três timers terminando juntos.
- **Portal**: a mesma página que o ESP32 serve em `192.168.4.1`, falando com o núcleo simulado pela mesma API.
- **Eventos**: log com horário virtual.

Para recompilar o núcleo do simulador depois de editar `core/`:

```bash
make sim          # gera simulator/tubabot_core.wasm e .wasm.js
```

## 3. Testes

```bash
make test         # testes nativos do núcleo (34 testes)
make crosscheck   # mesmo cenário no nativo e no WebAssembly: framebuffers idênticos (requer node)
make screens      # gera docs/telas.png com telas reais (requer python3 + Pillow)
make e2e          # simulador no Chromium via Playwright (requer: npm i -D playwright && npx playwright install chromium)
```

## 4. Firmware (quando as peças chegarem)

Ambientes do PlatformIO:

| Ambiente | Modelo |
|---|---|
| `tubabot` | TubaBot (buzzer ativo + vibração) |
| `tubabot_lite` | TubaBot Lite (só visor e toque; pinos de buzzer/motor nunca são configurados) |

```bash
pio run -e tubabot -e tubabot_lite     # compila os dois (o portal é recompactado antes)
pio device list                        # descubra a porta (algo como /dev/cu.usbmodem…)
pio run -e tubabot -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -b 115200 -p /dev/cu.usbmodemXXXX
```

Use a porta que **o seu** `pio device list` mostrar; o nome muda de Mac para Mac. Também funciona `ls /dev/cu.*` antes e depois de plugar a placa.

Os binários ficam em `.pio/build/<ambiente>/firmware.bin`. O atalho `make flash ENV=tubabot_lite PORT=/dev/cu.usbmodemXXXX` faz o mesmo.

### Modo de gravação (BOOT/RESET)

Isto é do **chip**, não do firmware TubaBot (funciona mesmo com firmware travado ou apagado):

1. Segure o botão **BOOT** da Super Mini (GPIO9).
2. Toque e solte **RESET** (RST/EN).
3. Solte **BOOT**. A placa entra em modo de download e aparece como porta USB.
4. Grave com o comando de upload; depois toque **RESET** para rodar.

Use quando a porta não aparecer, quando o upload falhar com "No serial data received" ou depois de um firmware que reinicia em laço.

### Console serial de recuperação

Com o monitor aberto (115200), digite e tecle Enter:

| Comando | Efeito |
|---|---|
| `config` | reabre a rede própria do TubaBot (não apaga nada) — use se o toque estiver com defeito |
| `wifi-esquecer` | apaga só a rede de casa |
| `i2c` | lista endereços I²C e o estado do OLED (0x3C/0x3D) |
| `toque` | nível atual do TTP223 e contagem de bordas |
| `oled-teste` | padrão de teste no visor |
| `status` | estado completo em JSON + memória livre |
| `reiniciar` | salva e reinicia |

### Apagar tudo

`pio run -e tubabot -t erase` apaga a flash inteira (inclusive preferências e conquistas). Depois grave de novo.

## 5. Solução de problemas

| Sintoma | O que verificar |
|---|---|
| Porta USB não aparece | Cabo de **dados** (o Ugreen US287 é de dados), outra porta do Mac, modo BOOT/RESET |
| Visor preto, serial diz "OLED AUSENTE" | `i2c` no console; VCC/GND/SDA(GPIO4)/SCL(GPIO6); endereço 0x3C/0x3D; módulo alimentado em 3,3 V |
| Imagem deslocada 2 colunas | controlador SH1106: compile com `-DTB_OLED_SH1106=1` |
| Toque não responde | `toque` no console; jumper do TTP223 em modo momentâneo; polaridade em `board_profile.h` |
| Toque "liga e desliga" a cada toque | TTP223 em modo **toggle/retenção** (jumper B): não é defeito de software |
| Bipe contínuo ao ligar | módulo de buzzer ativo em **nível baixo**: `TB_BUZZER_ACTIVE_HIGH=0` (veja HARDWARE.md) |
| Celular não abre a página | conecte na rede `TubaBot-XXXX` e digite `http://192.168.4.1` (o aviso "sem internet" é esperado) |
| Rede de casa não conecta | o TubaBot segue offline e tenta de novo de tempos em tempos; confira nome/senha no portal |
