# Roteiro de bancada — TubaBot v3 (ES3C28P)

Faça com a placa **fora do corpo**, ligada ao computador pelo USB-C. Leva uns 15 minutos. Os itens marcados ⚠ são os que o software não pôde confirmar sem a placa.

## 0. Gravar

- **Navegador (Chrome/Edge):** <https://espressif.github.io/esptool-js/> → *Connect* → arquivos `firmware/dist/tubabot-v3.0.0-completo.bin` em **0x0** e o perfil da unidade (ex.: `firmware/fabrica/fabrica-pro-vermelho.bin`) em **0xFEF000** → *Program*.
- **PlatformIO:** `pio run -e tubabot -t upload` e depois `python3 tools/fabrica.py --modelo pro --cor vermelho --gravar`.

Porta não aparece? Segure **BOOT**, toque **RESET**, solte BOOT e tente de novo.

Abra o monitor serial (115200) e digite `ajuda`.

## 1. Barramento e perfil

| Passo | Esperado | Resultado |
|---|---|---|
| `i2c` | 0x18 (ES8311) e 0x38 (FT6336G); tela/toque/alto-falante/microfone "ok" | |
| `fabrica` | `TubaBot Pro | cor Vermelho | expressões Vermelho | tema escuro` (o que você gravou) | |
| A tela acende com a abertura (mar, nome, olhos) e depois o QR Code | imagem **em paisagem**, nítida, cores certas | |

## 2. Tela ⚠

| Passo | Esperado | Resultado |
|---|---|---|
| `tela-teste` | ponto **vermelho** no canto superior **esquerdo**; barras branco→azul da esquerda para a direita | |
| Imagem de cabeça para baixo? | Sistema → Girar 180° (ou compile com `-DTB_LCD_ROTATION=3`) | |
| Cores trocadas (vermelho↔azul) ou negativas? | me mande uma foto: é ajuste de 1 linha (`TB_LCD_IPS` / ordem BGR) | |

## 3. Toque ⚠

| Passo | Esperado | Resultado |
|---|---|---|
| Encoste no canto superior esquerdo e digite `toque` | `tela x≈0..30 y≈0..30` | |
| Canto inferior direito | `x≈290..319 y≈210..239` | |
| Toque no rosto | reage e aparecem os 3 controles à direita, que somem em ~4 s | |
| Deslize para cima | abre o menu 3×3 | |
| Acaricie (vai-e-vem devagar) | carinho | |

Se x/y saírem trocados ou espelhados, ajuste `TB_TOUCH_MAP` em `board_profile.h` (a saída do `toque` mostra também as coordenadas cruas — me mande os 2 cantos que eu calculo).

## 4. Som ⚠

| Passo | Esperado | Resultado |
|---|---|---|
| Plugue o alto-falante; `som` | toca o som de abertura | |
| Sem som nenhum? | compile com `-DTB_PA_ACTIVE_LOW=0` (o amplificador pode ser ativo em alto) | |
| Menu → Volume no mínimo e no máximo | diferença clara, sem distorção forte | |
| `som` em silêncio / batendo palmas | nível baixo (≈10–35) / alto (≈60+); palmas assustam o Tuba | |
| Música perto por 10 s | o Tuba dança | |

## 5. Energia (só Pro)

| Passo | Esperado | Resultado |
|---|---|---|
| **Antes de plugar**: multímetro no plugue da bateria | 3,6–4,2 V, vermelho no `+` da placa | |
| Bateria plugada, USB desligado | continua ligado; Sobre → Sensores mostra mV e % coerentes com o multímetro | |
| Lite: Sobre → Sensores | "bateria: não se aplica (Lite)"; Aparência → Informações na tela **sem** "Mostrar bateria" | |

## 6. Rede, portal, hora e OTA

| Passo | Esperado | Resultado |
|---|---|---|
| QR Code com o celular | entra em `TubaBot-XXXX` e abre `http://192.168.4.1` | |
| Configure nome, Wi-Fi de casa, cidade | entra na rede de casa; **a hora aparece sozinha** (NTP); clima carrega | |
| Desligue e ligue | a hora volta pela rede em alguns segundos (a placa não tem RTC) | |
| Portal → Aparência → Tema claro / cor | a tela muda na hora | |
| Portal → Sistema → Atualizar firmware com `tubabot-v3.0.0-app-ota.bin` | grava, reinicia, mantém preferências **e o perfil de fábrica** | |
| BOOT 3 s | reabre a rede de configuração | |
| Sistema → Restaurar padrão de fábrica | volta ao primeiro uso com tema e cor **da unidade** | |

## 7. Montado no corpo

Repita rapidamente 2 (orientação), 3 (toque nas bordas) e 4 (volume dentro do corpo).

Mande para mim a tabela preenchida e a saída de `i2c`, `fabrica`, `toque` (dois cantos) e `status`.
