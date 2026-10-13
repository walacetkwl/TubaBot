# Roteiro de bancada (quando as peças chegarem)

Faça na ordem. Não ligue um módulo cuja tensão/polaridade ainda esteja "a confirmar" antes de medir. Anote os resultados em `docs/VALIDACAO.md` e ajuste `firmware/include/board_profile.h`.

## 0. Antes de tudo (multímetro)
- Continuidade dos jumpers e das soldas; nenhum curto entre 3V3, 5V e GND da Super Mini.
- Com a placa no USB: 5V ≈ 5 V e 3V3 ≈ 3,3 V.

## 1. Placa sozinha
1. `pio run -e tubabot_lite -t upload` (Lite primeiro: não aciona buzzer/motor). Se a porta não aparecer, modo BOOT/RESET (README_MAC.md).
2. `pio device monitor -b 115200`: deve aparecer "TubaBot Lite … OLED AUSENTE (seguindo sem visor)" e a ajuda do console.
3. `status`: anote **heap livre**; `pio run` já mostrou o **tamanho da imagem** e o uso da partição (1,875 MB).
4. Desconecte e reconecte o USB 5 vezes: a porta deve voltar sempre.

## 2. OLED
1. Ligue VCC no **3V3**, GND, SDA → GPIO4, SCL → GPIO6.
2. `i2c`: deve listar 0x3C ou 0x3D e "OLED: ok". Anote o tempo do último envio (µs).
3. Reinicie: abertura (barbatana → TubaBot → olhos). `oled-teste`: quadrado no canto superior esquerdo; se estiver invertido, use "Girar 180°" no portal. Se a imagem estiver deslocada 2 colunas, recompile com `-DTB_OLED_SH1106=1`.
4. Puxe o fio SDA com o aparelho ligado e recoloque: o firmware deve continuar rodando e o visor voltar em até ~5 s.

## 3. Toque (TTP223)
1. Meça a saída do TTP223 alimentado em 3,3 V: tocado/solto. Confirme jumpers A (polaridade) e B (momentâneo).
2. Ligue OUT → GPIO7. `toque` no console: "TOCADO"/"livre" e bordas aumentando.
3. Teste através do plástico da cabeça na espessura final.
4. Gestos: toque (troca expressão), duplo (menu), 1 s (carinho), 3 s (amor), 8 s + soltar + toque (configuração).
5. Segure 25 s: deve aparecer "Sensor preso?" e nada mais.

## 4. Jogos e tempo (ainda no Lite)
1. Jogue os quatro jogos. Em especial: Pula, Tuba! deve ser fácil de começar e responder na hora; Hora do Bote só aceita o peixe inteiro no quadrado.
2. Reflexo: compare tempos com o simulador; a diferença indica latência do TTP223 + visor (anote).
3. Timer de 1 min com o visor apagado (descanso de 30 s): o aviso deve acordar o visor.
4. Pomodoro curto (1/1 min) pelo portal.

## 5. Buzzer ativo (modelo completo) — medir antes
1. Sem ligar ao ESP32: alimente o módulo e encoste o pino IN em GND e depois em 3,3 V. Descubra o nível que liga (ALTO ou BAIXO) e a corrente.
2. Se ligar em BAIXO: compile com `-DTB_BUZZER_ACTIVE_HIGH=0`. Se for PNP e só desligar com 5 V no IN, alimente o módulo em 3,3 V.
3. Grave `-e tubabot`. Ao ligar não pode haver bipe antes da abertura. Teste silencioso no portal (corta na hora).

## 6. Vibração (MV50) — medir antes
1. Identifique no módulo: transistor? diodo? Meça a corrente do motor em 3,3 V e 5 V.
2. Sem driver: **não ligue ao GPIO**; monte transistor + diodo (HARDWARE.md §3.4).
3. Ligue IN → GPIO10. Carinho deve "ronronar" em pulsos; alertas vibram no máximo ~6 s.
4. Com o motor vibrando, verifique se o TTP223 dispara toques falsos.

## 7. Rede e portal
1. Menu → Configurar: conecte o celular em `TubaBot-XXXX` com a senha do visor; abra `http://192.168.4.1` (iPhone e Android). Verifique se o aviso de portal cativo aparece.
2. Faça o assistente, "Usar offline", troque apelido com acento, envie a hora do celular.
3. Configure a rede de casa; desligue o roteador: o Tuba segue normal e tenta de novo depois.
4. Esqueça a rede de casa: apelido/recordes continuam.

## 8. Integração e estabilidade
1. OTA pelo portal com o `.bin` do outro ambiente; depois teste OTA com Pomodoro rodando (deve recusar).
2. **OTA interrompida** (desligue o USB no meio): deve voltar à versão anterior; se não voltar, grave pelo USB (anote o resultado).
3. 24 h ligado com timers e lembretes: anote heap mínima (`status`) no começo e no fim.
4. Perda de energia com Pomodoro rodando: ao religar, "Sessão interrompida" e hora a acertar.
