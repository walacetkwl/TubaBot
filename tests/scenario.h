// Cenário compartilhado pelo teste de equivalência nativo x WebAssembly.
// Formato: {tempo_ms, tipo, a, b, c}
//   tipo -1: checkpoint (imprime CRC da imagem) · 1/0: toque (x=a, y=b) pressionado/solto
//   2: movimento (a, b, c em mg) · 3: nível sonoro (a)
#pragma once
static const long long SCENARIO[][5] = {
  {3000,-1,0,0,0},
  {3600,1,233,233,0}, {3700,0,233,233,0}, {4200,-1,0,0,0},          // primeiro uso: página QR
  {4300,1,422,233,0}, {4380,0,422,233,0}, {4900,-1,0,0,0},          // próxima página
  {5000,1,233,440,0}, {5080,0,233,440,0}, {5800,-1,0,0,0},          // usar offline
  {6000,1,300,200,0}, {6080,0,300,200,0}, {7000,-1,0,0,0},          // boop no rosto
  {7200,1,233,330,0}, {7250,1,233,280,0}, {7300,1,233,200,0}, {7350,1,233,130,0}, {7360,0,233,130,0}, {8200,-1,0,0,0},  // deslizar para cima: apps
  {8400,1,233,233,0}, {8480,0,233,233,0}, {9200,-1,0,0,0},          // abre o item central
  {9400,2,0,1000,0}, {9500,2,1800,1000,0}, {9580,2,-1800,1000,0}, {9660,2,1800,1000,0}, {9740,2,-1800,1000,0}, {9820,2,0,1000,0}, {10400,-1,0,0,0},  // sacudir
  {10500,3,30,0,0}, {10540,3,80,0,0}, {10580,3,32,0,0}, {11000,-1,0,0,0},
};
