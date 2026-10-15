# Componentes de terceiros

O TubaBot é software proprietário da TubaLabs (veja `LICENSE`). Ele inclui ou usa
os componentes abaixo, cada um sob a própria licença — que precisa ser respeitada
em qualquer distribuição (inclusive do firmware gravado no aparelho).

| Componente | Onde | Licença | Obrigação principal |
|---|---|---|---|
| Nunito (fonte) | `tools/fonts/Nunito.ttf` → `core/font_data.cpp` | SIL Open Font License 1.1 (`tools/fonts/OFL.txt`) | manter o aviso de copyright e a licença junto da fonte |
| QR Code generator (Nayuki) | `core/third_party/` | MIT | manter o aviso de copyright |
| Driver ES8311 (esp-adf/esp-bsp) | `firmware/src/codec/` | Apache-2.0 | manter cabeçalhos de licença e avisos de modificação |
| Arduino-ESP32 / ESP-IDF | plataforma (`platformio.ini`) | LGPL-2.1 (núcleo Arduino) / Apache-2.0 (ESP-IDF) | permitir religação com a biblioteca LGPL; avisos |
| Arduino_GFX | `lib_deps` | BSD | manter aviso |
| Open-Meteo (serviço de clima) | consulta em tempo de execução | dados CC BY 4.0 | atribuição "Open-Meteo" (mostrada na tela do clima) |

Nenhum código, imagem ou som de outros produtos (ex.: Neo Deskimon) foi copiado:
rosto, expressões, reações lendárias, sons e telas são criação da TubaLabs.
