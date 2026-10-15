// Executa o MESMO cenário no núcleo compilado para WebAssembly (o do simulador).
import fs from 'node:fs';
const src = fs.readFileSync(new URL('./scenario.h', import.meta.url), 'utf8');
const rows = [...src.matchAll(/\{(\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}/g)].map(m => m.slice(1, 6).map(Number));
const bytes = fs.readFileSync(new URL('../simulator/tubabot_core.wasm', import.meta.url));
let mem;
const cstr = p => { const u = new Uint8Array(mem.buffer); let e = p; while (u[e]) e++; return new TextDecoder().decode(u.subarray(p, e)); };
const { instance } = await WebAssembly.instantiate(bytes, { env: {
  js_storage_load: () => 0, js_storage_save: () => 1, js_storage_erase: () => {}, js_log: () => {}, js_reboot: () => {},
  js_display: () => {}, js_rtc: () => {} } });
const x = instance.exports; mem = x.memory;
x.sim_init(99, 1, 0, 63);
let t = 0;
for (const [target, type, a, b, c] of rows) {
  while (t + 10 <= target) { t += 10; if (x.sim_tick(t)) x.sim_presented(t); }
  if (type === -1) console.log(`${target} ${cstr(x.sim_screen_name())} ${(x.sim_frame_crc() >>> 0).toString(16).padStart(8, '0')}`);
  else if (type === 0 || type === 1) x.sim_touch(type, a, b, target);
  else if (type === 2) x.sim_motion(a, b, c, target);
  else if (type === 3) x.sim_sound(a, target);
}
