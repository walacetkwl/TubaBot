# Preparação de fábrica — um firmware, um perfil por unidade

Cada TubaBot sai da bancada com um **perfil de fábrica**: um texto curto, com CRC, gravado na partição `fabrica` (4 KB em `0xFEF000`). Ele define o que é **da unidade** e os **padrões** das preferências. O código-fonte e o firmware são os mesmos para todas as unidades.

```
TUBABOT-FABRICA 1
modelo=pro                 lite | pro
cor=vermelho               cor física: vermelho | azul | branco | grafite (preto = grafite)
expressao=vermelho         cor padrão das expressões (padrão: a da cor física)
tema=escuro                escuro | claro
volume=60                  0..100
brilho=80                  10..100 (%)
personalidade=tranquilo    tranquilo | brincalhao
mostrar_hora=0             0 | 1  (idem mostrar_clima, mostrar_bateria — esta só vale no Pro)
formato_24h=1
fuso=America/Sao_Paulo
idioma=pt-BR               pt-BR | en | es-419 (idioma inicial; o usuário troca em Sistema → Idioma)
telemetria=1               0 | 1  (estatísticas anônimas para o painel; o usuário pode desligar)
serie=TB3-PVM-2610-00042   opcional, até 23 caracteres (--serie auto gera e registra)
crc=…                      CRC-32 (zlib) de tudo antes desta linha
```

Chaves desconhecidas são ignoradas, então perfis futuros podem acrescentar padrões sem quebrar unidades antigas. Perfis da v3.0 (sem `idioma`/`telemetria`) continuam válidos: português e telemetria ligada.

### Número de série e registro

Para unidades de venda use `--serie auto`: a ferramenta gera a próxima série (`TB3-<P|L><cor>-<AAMM>-<NNNNN>`) e acrescenta a unidade em `firmware/fabrica/registro.csv` (série, modelo, cor, idioma, data). Esse registro alimenta o futuro painel ("não ativados" = registrados que nunca deram sinal). Veja `docs/futuras-atualizacoes.html`.

```bash
python3 tools/fabrica.py --modelo pro --cor azul --idioma es-419 --serie auto --gravar --porta /dev/cu.usbmodem1101
```

## Onde o perfil fica e o que o apaga

| Ação | Firmware | Preferências do usuário | Perfil de fábrica |
|---|---|---|---|
| Atualizar pelo portal (OTA) | troca | mantém | **mantém** |
| Regravar `tubabot-v3.1.0-completo.bin` em 0x0 | troca | mantém* | **mantém** (fica depois do fim da imagem) |
| "Restaurar padrão de fábrica" (Sistema ou portal) | — | apaga (volta ao primeiro uso com os padrões da unidade) | **mantém** |
| "Erase flash" (apagar a flash inteira) | apaga | apaga | **apaga** — grave o perfil de novo |

\* a imagem completa inclui a tabela de partições; as preferências ficam na NVS, que a imagem não cobre.

## Três jeitos de gravar

### 1. Ferramenta `tools/fabrica.py` (recomendado)

```bash
# um TubaBot Pro vermelho, tema escuro (as expressões seguem a cor do corpo)
python3 tools/fabrica.py --modelo pro --cor vermelho

# Lite branco, tema claro, com número de série
python3 tools/fabrica.py --modelo lite --cor branco --tema claro --serie TB-2026-000124

# os 8 perfis padrão (Lite/Pro × 4 cores) — já estão prontos em firmware/fabrica/
python3 tools/fabrica.py --todos

# gerar e gravar direto pela USB (pip install esptool)
python3 tools/fabrica.py --modelo pro --cor azul --gravar --porta /dev/cu.usbmodem1101
```

A ferramenta valida tudo antes de gerar; o arquivo `.txt` ao lado do `.bin` mostra o conteúdo legível. O formato é o mesmo que o firmware lê (há um teste automatizado que confere byte a byte).

### 2. Pelo navegador, junto com o firmware (sem instalar nada)

Em <https://espressif.github.io/esptool-js/> (Chrome/Edge): **Connect**, depois adicione **dois arquivos**:

| Endereço | Arquivo |
|---|---|
| `0x0` | `firmware/dist/tubabot-v3.1.0-completo.bin` |
| `0xFEF000` | `firmware/fabrica/fabrica-pro-vermelho.bin` (o perfil da unidade) |

**Program**. Pronto: a unidade liga já vermelha, Pro, tema escuro.

### 3. Pelo console serial (115200), sem PC especial

Com o firmware já gravado, num monitor serial (o da esptool-js serve):

```
fabrica-gravar modelo=pro cor=vermelho tema=escuro serie=TB-2026-000125
```

O firmware monta o texto, valida com o mesmo código do núcleo, grava a partição e reinicia. `fabrica` mostra o perfil atual.

## Roteiro de bancada por unidade

1. Gravar firmware + perfil (jeito 2) — ou firmware e depois `fabrica-gravar` (jeito 3).
2. Ligar: a abertura aparece com o tema e a cor da unidade; o serial mostra `TubaBot Pro 3.0.0 | Vermelho | ...`.
3. Menu → Mais apps → Sobre → página **Fábrica**: conferir modelo, cor, expressão, tema e série.
4. Pro: conferir a bateria em Sobre → Sensores (mV e %). Lite: "bateria: não se aplica".
5. Deixar a unidade no primeiro uso (QR Code) para o cliente.

## Testando no simulador

Aba **Fábrica** do simulador: escolha modelo, cor física, cor das expressões e tema → "Gravar perfil e reiniciar como unidade nova". O corpo desenhado muda de cor, a aba Energia some no Lite e o texto gravado aparece embaixo (idêntico ao da ferramenta).
