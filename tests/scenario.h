// Cenário compartilhado pelo teste de equivalência nativo x WebAssembly.
// Formato: {tempo_ms, nível do sensor (-1 = só checkpoint)}
#pragma once
static const long long SCENARIO[][2] = {
  {3000,-1}, {3100,1}, {4600,0}, {5200,-1},            // primeiro uso -> usar offline
  {6000,1}, {6080,0}, {6800,-1},                         // toque: próxima expressão
  {7000,1}, {7070,0}, {7150,1}, {7220,0}, {7800,-1},     // duplo: menu
  {8000,1}, {8080,0}, {8600,1}, {8680,0}, {9200,-1},     // próximo item x2
  {9400,1}, {10600,0}, {11000,-1},                       // selecionar (Pomodoro)
  {11200,1}, {11280,0}, {12000,-1}, {72000,-1},          // inicia; 1 min depois
  {72200,1}, {73400,0}, {74000,-1},                      // volta ao menu
  {80000,1}, {81300,0}, {82000,-1},                      // seleciona (Pomodoro)
};
