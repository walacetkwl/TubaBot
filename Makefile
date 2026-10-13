# TubaBot — atalhos (macOS e Linux). Veja README_MAC.md.
CXX ?= c++
CXXFLAGS = -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -Icore
CORE = $(wildcard core/*.cpp)
BUILD = build

.PHONY: test sim simulador screens crosscheck e2e check-fw firmware lite flash monitor portal clean

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

## Galeria de telas reais (docs/telas.png; requer python3 + Pillow para o PNG)
screens: $(BUILD)/screens
	./$(BUILD)/screens docs/telas.pbm
	python3 -c "from PIL import Image, ImageOps; im=ImageOps.invert(Image.open('docs/telas.pbm').convert('L')); im.resize((im.width*2,im.height*2),Image.NEAREST).save('docs/telas.png')" && rm docs/telas.pbm
$(BUILD)/screens: tools/screens.cpp $(CORE)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) tools/screens.cpp $(CORE) -o $@

## Teste de ponta a ponta do simulador (Playwright)
e2e:
	node tests/e2e_sim.mjs $(BUILD)/e2e

## Portal gzip para a flash
portal:
	python3 tools/embed_portal.py

## Firmware (PlatformIO)
firmware:
	pio run -e tubabot
lite:
	pio run -e tubabot_lite
flash:
	pio run -e $(or $(ENV),tubabot) -t upload $(if $(PORT),--upload-port $(PORT),)
monitor:
	pio device monitor $(if $(PORT),-p $(PORT),)

clean:
	rm -rf $(BUILD) build-check .pio
