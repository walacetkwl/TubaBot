# Roteiro de bancada — TubaBot v2

Faça com a placa **fora do corpo**, sobre a mesa, ligada ao Mac pelo USB-C. Leva uns 20 minutos. Anote o resultado de cada passo; os itens marcados ⚠ são os que o software não pôde confirmar sem a peça.

## 0. Gravar

Escolha um caminho:

- **Pelo navegador (sem instalar nada)** — Chrome/Edge: abra <https://espressif.github.io/esptool-js/>, *Connect*, escolha a porta da placa, endereço **0x0**, arquivo `firmware/dist/tubabot-v2.0.0-completo.bin` (ou `tubabot_vibra-…` se ligou o motor), *Program*.
- **Pelo PlatformIO**: `pio run -e tubabot -t upload` (veja `README_MAC.md`).

Se a porta não aparecer: segure **BOOT**, toque **RESET**/plugue o USB, solte BOOT, e tente de novo.

Abra o monitor serial (115200; no navegador, a própria esptool-js tem *Console*). Digite `ajuda`.

## 1. Barramento e periféricos

| Passo | Esperado | Resultado |
|---|---|---|
| `i2c` | 0x18, 0x20, 0x34, 0x40, 0x51, 0x5A, 0x6B; linha final com tudo "ok" | |
| A tela acende com a abertura (mar, nome, olhos) e depois o QR do primeiro uso | imagem nítida, cores corretas, sem faixa deslocada | |
| `tela-teste` | padrão de teste por alguns segundos; volta sozinho | |

Se as cores saírem trocadas (azul↔vermelho) ou a imagem deslocada alguns pixels, anote e me avise: é ajuste de 1 linha no adaptador da tela.

## 2. Toque ⚠

| Passo | Esperado | Resultado |
|---|---|---|
| Encoste no canto **superior esquerdo** e digite `toque` | `TOCANDO em x≈0..60 y≈0..60` | |
| Canto inferior direito | x≈400..466, y≈400..466 | |
| Deslize da direita para a esquerda no rosto | abre o próximo cartão (relógio/hoje) | |
| Toque longo no centro | abre os apps | |
| Faça carinho (vai-e-vem) | rosto fica feliz, coraçãozinho | |

Se x estiver invertido: compile com `-DTB_TOUCH_MIRROR_X=0` (o mesmo para Y; se x e y estiverem trocados, `-DTB_TOUCH_SWAP_XY=1`). Fica em `firmware/include/board_profile.h`.

## 3. Movimento ⚠

| Passo | Esperado | Resultado |
|---|---|---|
| Placa em pé (como ficará no corpo), `imu` | y ≈ +1000, x e z perto de 0 | |
| Deitada, tela para cima | z ≈ −1000 | |
| Incline para a esquerda/direita | os olhos acompanham (opção do portal, ligada por padrão) | |
| Chacoalhe | expressão tonta | |
| Vire com a tela para baixo | a tela apaga; com alarme tocando, adia (opção do portal) | |
| App **Nível** | bolha centralizada na mesa | |

Se os eixos não baterem, ajuste `TB_IMU_MAP` (ex.: trocar sinais). Me mande a saída de `imu` nas posições acima que eu calculo o mapeamento.

## 4. Som ⚠

| Passo | Esperado | Resultado |
|---|---|---|
| Plugue o alto-falante; `som` | toca o som de abertura; volume confortável | |
| Ajustes → Volume no mínimo e no máximo | diferença clara; sem chiado/distorção forte | |
| `som` em silêncio | nível baixo (≈ 10–35) | |
| `som` batendo palmas perto | nível alto (≈ 60+); palmas no rosto fazem o TubaBot reagir | |
| Toque uma música perto por 10 s | o TubaBot "dança" (expressão Groovy) | |

## 5. Energia e relógio

| Passo | Esperado | Resultado |
|---|---|---|
| **Antes de plugar a bateria**: multímetro no plugue, vermelho no + | tensão positiva 3,6–4,2 V | |
| Plugue a bateria com USB ligado | ícone de raio; animação de carga | |
| Desligue o USB | continua ligado; `status` mostra mV e % | |
| PWR curto | dorme (tela apaga); PWR de novo acorda | |
| Ajuste a hora (portal ou Wi-Fi), desligue tudo 1 min, ligue | hora mantida pelo RTC | |

## 6. Rede, portal e OTA

| Passo | Esperado | Resultado |
|---|---|---|
| Leia o QR da tela com o celular | conecta em `TubaBot-XXXX` e abre `http://192.168.4.1` | |
| Configure nome, Wi-Fi de casa, cidade | aceita; o TubaBot entra na rede de casa e mostra o clima | |
| Portal → espelho da tela | imagem atualiza | |
| Portal → Atualizar firmware com `tubabot-v2.0.0-app-ota.bin` | grava, reinicia, mantém preferências | |
| `wifi-esquecer` | apaga só a rede; apelido e recordes ficam | |
| BOOT por 3 s | reabre a rede de configuração | |

## 7. Montado no corpo

Repita rapidamente os passos 2 (toque nas bordas da moldura), 3 (`imu` em pé) e 4 (volume dentro do corpo). Se a tela ficou de cabeça para baixo: Ajustes → Girar 180°.

Mande para mim a tabela preenchida e a saída de `i2c`, `toque`, `imu` e `status`.
