# Validação v2 — o que foi provado, como, e o que depende das peças

Data: 15/10/2026 · núcleo 2.0.0.

## Legenda

- **SIM** — implementado e testado (testes nativos e/ou simulador)
- **COMP** — firmware compilado com a plataforma real (pioarduino 53.03.13 / Arduino-ESP32 3.1.3) para o ESP32-S3
- **HW** — pendente de bancada (peças ainda não recebidas)

## Resultados reais desta entrega

| Verificação | Comando | Resultado |
|---|---|---|
| Testes nativos do núcleo | `make test` | **55 testes, 27.137 verificações, 0 falhas** (também com AddressSanitizer + UBSan: 0 falhas) |
| Mesmo núcleo no simulador | `make crosscheck` | 9 checkpoints com CRC do framebuffer 466×466 **idênticos** entre nativo e WebAssembly |
| Simulador e portal no navegador | `make e2e` (Chromium headless) | **20/20** verificações (file:// e HTTP, rede própria e rede de casa com PIN), sem erros de JavaScript e sem recurso externo |
| Firmware `tubabot` e `tubabot_vibra` | `pio run -e tubabot -e tubabot_vibra` | compilam; flash 1.624.484 B (38,7 % da partição de 4 MB), RAM estática 20,9 %; único aviso vem do Arduino_GFX (terceiro) |
| Imagens prontas | `tools/make_dist.sh` | `firmware/dist/*-completo.bin` (gravar em 0x0) e `*-app-ota.bin` (portal), com `SHA256SUMS.txt` |
| Portal | `make portal` | 48.127 → 15.538 bytes (gzip), sem CDN |
| Galeria | `make screens` | 31 telas reais em `docs/telas.png` |

## Matriz

| Área | Estado | Observação |
|---|---|---|
| Rosto, 34 expressões, piscar, olhar, carinho, amor | SIM · COMP | |
| Gestos na tela (toque, duplo, longo, deslizar ×4, carinho, recuperação) | SIM · COMP · **HW** | orientação do toque a confirmar |
| Reações a movimento (chacoalhar, pegar, virar, inclinar) | SIM · COMP · **HW** | eixos do IMU a confirmar |
| Reações a som (palmas, barulho, música/dança) | SIM · COMP · **HW** | ganho do ES7210 a confirmar |
| Sons e volume | SIM · COMP · **HW** | ganho do ES8311 e alto-falante comprado |
| Bateria, carregamento, aviso de bateria fraca | SIM · COMP · **HW** | |
| RTC (hora sem Wi-Fi) | SIM · COMP · **HW** | |
| 5 jogos, Pomodoro, 3 timers, cronômetro, lembretes, alarmes | SIM · COMP | |
| Hoje, Clima (Open-Meteo), Respiração, Nível, Sorteio, mostradores | SIM · COMP | clima em rede real: **HW** |
| Portal, PIN, backup (inclusive da v1), OTA | SIM · COMP · **HW** (OTA real) | |
| Vibração (MV50 no GPIO16) | SIM · COMP · **HW** | só `tubabot_vibra` |

## Limites honestos

- Nenhum teste rodou na placa física: a tela, o toque e os codecs foram configurados conforme os exemplos oficiais da Waveshare, mas só a bancada confirma.
- O consumo e a autonomia da bateria são estimativas.
- O simulador reproduz a lógica e o desenho exatamente; a física dos sensores (ruído do IMU, acústica dos microfones) é aproximada.

Roteiro de bancada: [`BANCADA.md`](BANCADA.md).
