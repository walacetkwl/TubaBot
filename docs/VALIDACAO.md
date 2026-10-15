# Validação v3.1 — o que foi provado, como, e o que depende da placa

Data: 15/10/2026 · núcleo 3.1.0 · placa-alvo ES3C28P (tela 320×240 em paisagem).

## Resultados reais desta entrega

| Verificação | Comando | Resultado |
|---|---|---|
| Testes nativos do núcleo | `make test` | **69 testes, 136.482 verificações, 0 falhas** (também com AddressSanitizer + UBSan: 0 falhas) |
| Mesmo núcleo no simulador | `make crosscheck` | 18 checkpoints com CRC da tela 320×240 **idênticos** entre nativo e WebAssembly (inclui perfil de fábrica, controles do rosto, menu, troca de tema, reações lendárias com medalha e a coleção em espanhol) |
| Simulador e portal no navegador | `make e2e` (Chromium) | **30/30** verificações (inclui aba Reações, troca de idioma, aviso de atualização publicada e adiamento): paisagem 4:3, gestos, controles do rosto, perfil de fábrica Lite azul/tema claro, portal (tema, cor, informações na tela), portal por HTTP com e sem PIN; sem erros de JavaScript nem recursos externos |
| Ferramenta de fábrica × firmware | teste `factory_profile_from_python_tool` | texto gerado pelo `tools/fabrica.py` lido e regravado pelo núcleo **byte a byte igual** |
| Firmware ES3C28P | `pio run -e tubabot` | compila; flash 1.553.976 B (37,0 % da partição de 4 MB), RAM estática 24,1 %; único aviso vem do Arduino_GFX (terceiro) |
| Imagens prontas | `tools/make_dist.sh`, `tools/fabrica.py --todos` | `firmware/dist/tubabot-v3.1.0-completo.bin` (0x0), `…-app-ota.bin` (portal), 8 perfis em `firmware/fabrica/` (0xFEF000) |
| Traduções | `make i18n-audit` + teste `i18n_language_switch_and_table` | todas as telas percorridas em inglês e espanhol; restam só textos compostos já traduzidos por partes, nomes próprios e números |
| Galeria | `make screens` | 61 telas reais em `docs/telas.png` (escuro/claro, 4 cores físicas, Lite/Pro) |

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
| Reações lendárias (regras de raridade, medalha, coleção, replay) | testado (10 dias simulados) + compilado | |
| Idiomas pt-BR / en / es-419, padrão de fábrica | testado + compilado | |
| Atualização pela internet (aviso, adiar, nunca ignorar, agendar, instalar) | testado com servidor simulado + compilado | download real com SHA-256: **bancada** + servidor |
| Telemetria | testado (conteúdo, opt-out) + compilado | servidor/painel: **futuro** |

## Limites honestos

- Nada rodou na placa física ainda. Pinos vieram de três fontes que concordam (LCDWIKI, BSP `ngttai`, Launcher); onde divergem (habilitação do amplificador) há uma opção de compilação.
- A placa não informa se a bateria está carregando nem tem relógio com bateria: o TubaBot não mostra "carregando" e a hora vem da rede.
- Autonomia da bateria e consumo são estimativas.
