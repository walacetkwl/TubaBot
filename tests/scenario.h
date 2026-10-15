// Cenário compartilhado pelo teste de equivalência nativo x WebAssembly (tela 320×240).
// Formato: {tempo_ms, tipo, a, b, c}
//   tipo -1: checkpoint (imprime CRC da imagem) · 1/0: toque (x=a, y=b) pressionado/solto
//   2: movimento (a, b, c em mg) · 3: nível sonoro (a)
//   4: reação lendária (descoberta) de índice a · 5: idioma a (0 pt, 1 en, 2 es) · 6: abre a coleção Lendárias
// A unidade é um TubaBot Pro vermelho, tema escuro (perfil de fábrica nos dois lados).
#pragma once
static const long long SCENARIO[][5] = {
  {3000,-1,0,0,0},
  {3600,1,160,120,0}, {3700,0,160,120,0}, {4200,-1,0,0,0},          // primeiro uso: página QR
  {4300,1,296,176,0}, {4380,0,296,176,0}, {4900,-1,0,0,0},          // próxima página
  {5000,1,236,200,0}, {5080,0,236,200,0}, {5800,-1,0,0,0},          // usar offline
  {6000,1,200,120,0}, {6080,0,200,120,0}, {7000,-1,0,0,0},          // boop no rosto: controles aparecem
  {7100,1,295,120,0}, {7180,0,295,120,0}, {7800,-1,0,0,0},          // botão Menu
  {7900,1,54,70,0}, {7980,0,54,70,0}, {8600,-1,0,0,0},              // Jogos
  {8700,1,60,140,0}, {8730,1,120,140,0}, {8760,1,190,140,0}, {8790,1,260,140,0}, {8800,0,260,140,0}, {9300,-1,0,0,0},  // voltar
  {9400,1,60,140,0}, {9430,1,120,140,0}, {9460,1,190,140,0}, {9490,1,260,140,0}, {9500,0,260,140,0}, {10000,-1,0,0,0}, // rosto
  {10100,1,160,30,0}, {10130,1,160,90,0}, {10160,1,160,150,0}, {10190,1,160,210,0}, {10200,0,160,210,0}, {10800,-1,0,0,0}, // painel rápido
  {10900,1,100,80,0}, {10980,0,100,80,0}, {11600,-1,0,0,0},         // tema claro
  {11700,3,30,0,0}, {11740,3,80,0,0}, {11780,3,32,0,0}, {12400,-1,0,0,0},
  {12500,4,4,0,0}, {13900,-1,0,0,0}, {15500,-1,0,0,0},                 // cafezinho (lendária)
  {16000,4,6,0,0}, {20200,-1,0,0,0}, {26000,-1,0,0,0},                 // escada rolante + medalha
  {26100,5,2,0,0}, {26200,6,0,0,0}, {27000,-1,0,0,0},                  // espanhol + coleção
};
