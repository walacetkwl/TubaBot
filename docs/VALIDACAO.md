# Validação v3 — o que foi provado, como, e o que depende da placa

Data: 15/10/2026 · núcleo 3.0.0 · placa-alvo ES3C28P (tela 320×240 em paisagem).

## Resultados reais desta entrega

| Verificação | Comando | Resultado |
|---|---|---|
| Testes nativos do núcleo | `make test` | **62 testes, 27.250 verificações, 0 falhas** (também com AddressSanitizer + UBSan: 0 falhas) |
| Mesmo núcleo no simulador | `make crosscheck` | 12 checkpoints com CRC da tela 320×240 **idênticos** entre nativo e WebAssembly (inclui perfil de fábrica, controles do rosto, menu e troca de tema) |
| Simulador e portal no navegador | `make e2e` (Chromium) | **26/26** verificações: paisagem 4:3, gestos, controles do rosto, perfil de fábrica Lite azul/tema claro, portal (tema, cor, informações na tela), portal por HTTP com e sem PIN; sem erros de JavaScript nem recursos externos |
| Ferramenta de fábrica × firmware | teste `factory_profile_from_python_tool` | texto gerado pelo `tools/fabrica.py` lido e regravado pelo núcleo **byte a byte igual** |
| Firmware ES3C28P | `pio run -e tubabot` | compila; flash 1.498.584 B (35,7 % da partição de 4 MB), RAM estática 21,3 %; único aviso vem do Arduino_GFX (terceiro) |
| Imagens prontas | `tools/make_dist.sh`, `tools/fabrica.py --todos` | `firmware/dist/tubabot-v3.0.0-completo.bin` (0x0), `…-app-ota.bin` (portal), 8 perfis em `firmware/fabrica/` (0xFEF000) |
| Galeria | `make screens` | 53 telas reais em `docs/telas.png` (escuro/claro, 4 cores físicas, Lite/Pro) |

## Matriz

| Área | Estado | Observação |
|---|---|---|
| Rosto em tela cheia, controles que aparecem e somem, 34 expressões | testado + compilado | |
| Tema claro/escuro, cor das expressões por tema | testado + compilado | |
| Informações sobre o rosto (hora, clima, bateria só Pro) | testado + compilado | |
| Perfil de fábrica, Lite/Pro, restaurar padrão de fábrica | testado + compilado | gravação da partição real: **bancada** |
| Menu 3×3, Aparência, Cor, Volume, Brilho, Wi-Fi, Sistema, retorno automático ao rosto | testado + compilado | |
| Jogos, Pomodoro, timers, cronômetro, alarmes, lembretes, clima | testado + compilado | clima em rede real: **bancada** |
| Tela ILI9341V em paisagem | compilado | orientação/cores: **bancada** (`tela-teste`) |
| Toque FT6336G | compilado | mapa retrato→paisagem: **bancada** (`toque`) |
| Alto-falante e microfone (ES8311) | compilado | polaridade do amplificador e ganhos: **bancada** |
| Bateria (ADC) | compilado | divisor e curva: **bancada** com multímetro |
| Portal, PIN, backup, OTA | testado + compilado | OTA real: **bancada** |

## Limites honestos

- Nada rodou na placa física ainda. Pinos vieram de três fontes que concordam (LCDWIKI, BSP `ngttai`, Launcher); onde divergem (habilitação do amplificador) há uma opção de compilação.
- A placa não informa se a bateria está carregando nem tem relógio com bateria: o TubaBot não mostra "carregando" e a hora vem da rede.
- Autonomia da bateria e consumo são estimativas.
