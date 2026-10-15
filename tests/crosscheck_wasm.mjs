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
const enc = new TextEncoder();
const put = s => { const b = enc.encode(s); new Uint8Array(mem.buffer, x.sim_inbuf(), b.length + 1).set([...b, 0]); return b.length; };
const n = x.sim_factory_compose(put('modelo=pro\ncor=vermelho\ntema=escuro\n'));
const fab = cstr(x.sim_outbuf()); if (n < 0) throw new Error('perfil: ' + fab);
x.sim_factory_set(put(fab));
x.sim_init(99, 1, 0, 1 | 8 | 16);
const wstr = (ptr, cap, s) => { const b = enc.encode(s); new Uint8Array(mem.buffer, ptr, b.length + 1).set([...b, 0]); return b.length; };
const K = ['relogio', 'joinha', 'peixe', 'peixe_come', 'cafe', 'meditacao', 'escada', 'ouvidos', 'bocejo'], L = ['pt-BR', 'en', 'es-419'];
function api(path, body, now) {
  wstr(x.sim_methodbuf(), 8, 'POST'); wstr(x.sim_pathbuf(), 128, path); wstr(x.sim_tokenbuf(), 40, cstr(x.sim_token()));
  return x.sim_api(put(JSON.stringify(body)), 1, 1, now);
}
let t = 0;
for (const [target, type, a, b, c] of rows) {
  while (t + 10 <= target) { t += 10; if (x.sim_tick(t)) x.sim_presented(t); }
  if (type === -1) console.log(`${target} ${cstr(x.sim_screen_name())} ${(x.sim_frame_crc() >>> 0).toString(16).padStart(8, '0')}`);
  else if (type === 0 || type === 1) x.sim_touch(type, a, b, target);
  else if (type === 2) x.sim_motion(a, b, c, target);
  else if (type === 3) x.sim_sound(a, target);
  else if (type === 4) api('/api/action', { action: 'legend.play', scene: K[a], discover: true }, target);
  else if (type === 5) api('/api/prefs', { language: L[a] }, target);
  else if (type === 6) api('/api/action', { action: 'legend.open' }, target);
}
