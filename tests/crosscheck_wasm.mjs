// Executa o MESMO cenário no núcleo compilado para WebAssembly (o do simulador).
import fs from 'node:fs';
const src = fs.readFileSync(new URL('./scenario.h', import.meta.url), 'utf8');
const rows = [...src.matchAll(/\{(\d+),(-?\d+)\}/g)].map(m => [Number(m[1]), Number(m[2])]);
const bytes = fs.readFileSync(new URL('../simulator/tubabot_core.wasm', import.meta.url));
let mem;
const cstr = p => { const u = new Uint8Array(mem.buffer); let e = p; while (u[e]) e++; return new TextDecoder().decode(u.subarray(p, e)); };
const { instance } = await WebAssembly.instantiate(bytes, { env: {
  js_storage_load: () => 0, js_storage_save: () => 1, js_storage_erase: () => {}, js_log: () => {}, js_reboot: () => {}, js_display: () => {} } });
const x = instance.exports; mem = x.memory;
x.sim_init(1, 99, 1, 0);
let t = 0;
for (const [target, lvl] of rows) {
  while (t + 10 <= target) { t += 10; if (x.sim_tick(t)) x.sim_presented(t); }
  if (lvl >= 0) x.sim_touch(lvl, target);
  else console.log(`${target} ${cstr(x.sim_screen_name())} ${(x.sim_frame_crc() >>> 0).toString(16).padStart(8, '0')}`);
}
