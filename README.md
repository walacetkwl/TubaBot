# TubaBot — software (firmware, simulador e portal)

Companheiro de mesa da **TubaLabs**, em duas versões: **TubaBot Lite** (visor + toque) e **TubaBot** (visor + toque + buzzer ativo + vibração).

Um único núcleo C++ (`core/`) contém toda a lógica — gestos, rosto, personalidade, os quatro jogos, Pomodoro, timers, cronômetro, lembretes, alertas, persistência, desenho do visor 128×64 e a API local. Esse mesmo núcleo é compilado:

| Destino | Como | Para quê |
|---|---|---|
| ESP32-C3 Super Mini | `firmware/` + PlatformIO | o aparelho real |
| WebAssembly | `simulator/` (navegador do Mac) | testar a lógica real antes das peças |
| Nativo (Mac/Linux) | `tests/` | testes automatizados |

O teste `make crosscheck` roda a mesma sequência de toques no núcleo nativo e no WebAssembly e confere que os framebuffers são idênticos bit a bit.

```
core/        lógica compartilhada (C++17 sem STL, sem heap, sem exceções)
firmware/    adaptadores ESP32: OLED, toque, saídas, NVS, Wi-Fi, HTTP, OTA, serial
simulator/   simulador no navegador (abre com duplo clique em index.html)
web-ui/      portal de configuração (o mesmo arquivo no ESP32 e no simulador)
tests/       testes nativos, equivalência nativo×WASM e teste de ponta a ponta
tools/       scripts de build, verificação e geração de telas
docs/        hardware, funcionalidades, API, validação, bancada, inventário
```

**Comece por:** [`README_MAC.md`](README_MAC.md) (como rodar tudo no Mac) e [`docs/VALIDACAO.md`](docs/VALIDACAO.md) (o que foi testado e o que depende das peças).

Galeria de telas geradas pelo próprio núcleo: [`docs/telas.png`](docs/telas.png).
