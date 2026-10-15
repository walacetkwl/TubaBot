# TubaBot v2 — atalhos (macOS e Linux). Veja README_MAC.md.
CXX ?= c++
CXXFLAGS = -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -ffp-contract=off -Icore
CORE = $(wildcard core/*.cpp)
BUILD = build

.PHONY: test sim simulador screens crosscheck e2e firmware vibra dist flash monitor portal fonts clean

## Testes nativos do núcleo (Apple clang ou g++)
test: $(BUILD)/tests
	./$(BUILD)/tests
$(BUILD)/tests: tests/test_core.cpp tests/fake_platform.h $(CORE) core/*.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) tests/test_core.cpp $(CORE) -o $@

## Equivalência: mesmo cenário no núcleo nativo e no WebAssembly do simulador
crosscheck: $(BUILD)/xnative
	./$(BUILD)/xnative > $(BUILD)/native.txt
	node tests/crosscheck_wasm.mjs > $(BUILD)/wasm.txt
	diff $(BUILD)/native.txt $(BUILD)/wasm.txt && echo "framebuffers idênticos (nativo x WASM)"
$(BUILD)/xnative: tests/crosscheck_native.cpp tests/scenario.h $(CORE)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) tests/crosscheck_native.cpp $(CORE) -o $@

## Recompila o núcleo para WebAssembly (precisa de: brew install llvm lld)
sim:
	./tools/build_wasm.sh

## Abre o simulador no navegador padrão
simulador:
	open simulator/index.html 2>/dev/null || xdg-open simulator/index.html

## Galeria de telas reais (docs/telas.png; requer python3 + Pillow)
screens: $(BUILD)/screens
	@rm -rf $(BUILD)/telas && mkdir -p $(BUILD)/telas
	./$(BUILD)/screens $(BUILD)/telas
	python3 tools/gallery.py $(BUILD)/telas docs/telas.png
$(BUILD)/screens: tools/screens.cpp $(CORE) core/*.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -O2 tools/screens.cpp $(CORE) -o $@

## Fontes (core/font_data.cpp) a partir da Nunito
fonts:
	python3 tools/gen_fonts.py

## Teste de ponta a ponta do simulador (Playwright)
e2e:
	node tests/e2e_sim.mjs $(BUILD)/e2e

## Portal gzip para a flash
portal:
	python3 tools/embed_portal.py

## Firmware (PlatformIO + pioarduino / Arduino-ESP32 3.1.3)
firmware:
	pio run -e tubabot
vibra:
	pio run -e tubabot_vibra
## Binários prontos em firmware/dist (completo = gravar no endereço 0x0; app-ota = atualizar pelo portal)
dist: firmware vibra
	./tools/make_dist.sh
flash:
	pio run -e $(or $(ENV),tubabot) -t upload $(if $(PORT),--upload-port $(PORT),)
monitor:
	pio device monitor $(if $(PORT),-p $(PORT),)

clean:
	rm -rf $(BUILD) build-check .pio
