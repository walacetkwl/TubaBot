# TubaBot v3.1 no Mac (Apple Silicon ou Intel)

## 1. Pré-requisitos

```bash
xcode-select --install          # clang++, make, git e python3 da Apple
```

| Ferramenta | Para quê | Instalação |
|---|---|---|
| PlatformIO | compilar e gravar o firmware | `brew install platformio` |
| Node.js 18+ | equivalência nativo×WASM e teste de ponta a ponta | `brew install node` |
| LLVM + LLD | **recompilar** o simulador depois de mudar `core/` | `brew install llvm lld` |
| Pillow | galeria de telas | `python3 -m pip install --user pillow` |
| esptool | gravar perfis de fábrica pela USB (opcional) | `python3 -m pip install --user esptool` |

## 2. Simulador (sem a placa)

```bash
open simulator/index.html
```

Abre com duplo clique, sem servidor. O TubaBot aparece **em paisagem (320×240)** dentro de um corpo na cor física da unidade.

- **Tela:** clique = toque; arraste = deslizar; vai-e-vem devagar = carinho; segurar = toque longo. Tocar no rosto mostra os controles (Pomodoro, Menu, Jogos), que somem sozinhos. Botões **BOOT** e **BOOT (3 s)** ao lado.
- **Fábrica:** escolha Lite/Pro, cor física, cor das expressões e tema → "Gravar perfil e reiniciar como unidade nova". Mostra o texto exato que vai para a partição (o mesmo do `tools/fabrica.py`). Também "Restaurar padrão de fábrica" e "Apagar perfil".
- **Reações:** ver cada reação lendária ("Ver") ou como seria a primeira vez com medalha ("Descobrir"), ajustar a afinidade de teste, rodar o sorteio natural ("Dia lendário agora"), rever a abertura, ir para 20:30 (sono) ou 08:00 (café), relógio "Tuba" animado, abrir a coleção.
- **Atualização:** conectar um Wi-Fi de casa simulado, "Publicar v3.2.0" no servidor simulado e ver o aviso na tela (Agora / Em 1 hora / Esta noite), o download simulado e a prévia da telemetria.
- **Controle:** idioma do aparelho (pt-BR, English, Español), alto-falante/microfone presentes, som do simulador, ferramentas de desenvolvedor.
- **Som:** nível do ambiente, palma, barulho, música (ou o microfone do computador — só o nível).
- **Energia:** bateria (só aparece no Pro).
- **Tempo:** relógio virtual 1×/10×/60×/300× (jogos sempre 1×), saltos, enviar hora do computador. Sem RTC: ao "religar" a hora volta pela rede de casa (NTP simulado).
- **Falhas, Portal, Eventos:** como antes; o portal é a mesma página do aparelho.

Recompilar o núcleo do simulador após editar `core/`: `make sim`.

## 3. Testes

```bash
make test         # 69 testes nativos do núcleo
make crosscheck   # nativo × WebAssembly: telas idênticas bit a bit (requer node)
make screens      # docs/telas.png com 61 telas reais (requer Pillow)
make i18n-audit   # textos da tela sem tradução (inglês/espanhol)
make e2e          # simulador e portal no Chromium via Playwright (npm i -D playwright)
```

## 4. Gravar uma unidade

Cada unidade = **firmware (igual para todas) + perfil de fábrica (Lite/Pro, cor, tema)**. Detalhes em [`docs/FABRICA.md`](docs/FABRICA.md).

### 4.1 Pelo navegador (mais fácil)

1. Chrome ou Edge → <https://espressif.github.io/esptool-js/>
2. Ligue a placa no USB-C, **Connect**, escolha a porta.
3. Arquivos:
   - `0x0` → `firmware/dist/tubabot-v3.1.0-completo.bin`
   - `0xFEF000` → `firmware/fabrica/fabrica-pro-vermelho.bin` (ou o perfil da unidade)
4. **Program**. Ao terminar, aperte RESET.

### 4.2 Pelo PlatformIO + ferramenta de fábrica

```bash
pio run -e tubabot                       # compila (o portal é recompactado antes)
pio device list                          # porta (/dev/cu.usbmodem…)
make flash PORT=/dev/cu.usbmodemXXXX
python3 tools/fabrica.py --modelo pro --cor vermelho --gravar --porta /dev/cu.usbmodemXXXX
make monitor PORT=/dev/cu.usbmodemXXXX
make dist                                # regenera firmware/dist
make fabrica                             # regenera os 8 perfis padrão
```

### 4.3 Atualização pelo portal (OTA)

Portal → Sistema → **Atualizar firmware** → `firmware/dist/tubabot-v3.1.0-app-ota.bin`. Preferências, recordes **e o perfil de fábrica** são mantidos.

### Modo de gravação (BOOT)

Porta não aparece ou gravação falha: segure **BOOT**, toque **RESET**, solte BOOT e grave de novo.

### Console serial (115200)

| Comando | Efeito |
|---|---|
| `config` | reabre a rede própria do TubaBot |
| `wifi-esquecer` | apaga só a rede de casa |
| `i2c` | dispositivos I²C (0x18 áudio, 0x38 toque) e estado de tela/toque/áudio/bateria |
| `toque` | coordenadas do toque (tela e cruas) |
| `som` | nível do microfone + som de abertura |
| `tela-teste` | padrão de teste (orientação e cores) |
| `fabrica` | mostra o perfil de fábrica |
| `fabrica-gravar modelo=pro cor=vermelho tema=escuro serie=...` | grava o perfil e reinicia |
| `status` | estado completo em JSON + memória |
| `reiniciar` | salva e reinicia |

Sem computador: **BOOT por 3 s** reabre a rede de configuração.

## 5. Solução de problemas

| Sintoma | O que verificar |
|---|---|
| Porta USB não aparece | cabo de **dados**, outra porta, modo BOOT |
| Tela preta | `i2c`; brilho em Menu → Brilho; BOOT acende a tela |
| Imagem de cabeça para baixo | Menu → Sistema → Girar 180° |
| Toque em lugar errado | `toque`; ajuste `TB_TOUCH_MAP` em `board_profile.h` (veja BANCADA.md) |
| Sem som | alto-falante no conector SPEAKER; Volume/Silencioso; `som`; teste `-DTB_PA_ACTIVE_LOW=0` |
| Aparece como "Lite" / sem cor certa | `fabrica`: perfil ausente ou errado → grave o perfil (FABRICA.md) |
| Hora some ao religar | normal sem rede (a placa não tem RTC); configure o Wi-Fi de casa |
| Celular não abre a página | conecte em `TubaBot-XXXX` e digite `http://192.168.4.1` |
