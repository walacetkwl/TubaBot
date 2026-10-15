// Servidor HTTP local que imita o ESP32: serve o portal em "/" e responde /api/* com o
// MESMO núcleo (WebAssembly). Testa o caminho fetch + X-Tuba-Token do portal, e permite
// abrir o portal num celular na mesma rede do Mac.
//   node tools/portal_server.mjs [porta] [--sem-som] [--home]
//   --home simula acesso pela rede de casa (exige o PIN mostrado no "visor", impresso no terminal)
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const port = Number(process.argv.find(a => /^\d+$/.test(a)) || 8080);
const noSpeaker = process.argv.includes('--sem-som'), home = process.argv.includes('--home');
const bytes = fs.readFileSync(path.join(root, 'simulator/tubabot_core.wasm'));
let mem; const store = {};
const dec = new TextDecoder(), enc = new TextEncoder();
const cstr = p => { const u = new Uint8Array(mem.buffer); let e = p; while (u[e]) e++; return dec.decode(u.subarray(p, e)); };
const { instance } = await WebAssembly.instantiate(bytes, { env: {
  js_storage_load(k, p, cap) { const b = store[k]; if (!b || b.length > cap) return 0; new Uint8Array(mem.buffer, p, b.length).set(b); return b.length; },
  js_storage_save(k, p, n) { store[k] = new Uint8Array(mem.buffer, p, n).slice(); return 1; },
  js_storage_erase(k) { delete store[k]; },
  js_log(p) { console.log('[tuba]', cstr(p)); },
  js_reboot() {}, js_display() {}, js_rtc() {},
} });
const X = instance.exports; mem = X.memory;
const t0 = Date.now(); const now = () => Date.now() - t0;
X.sim_init(1234, 1, now(), noSpeaker ? 62 : 63);
setInterval(() => X.sim_tick(now()), 20);
function put(ptr, cap, s) { const b = enc.encode(s); if (b.length >= cap) return -1; const u = new Uint8Array(mem.buffer, ptr, cap); u.set(b); u[b.length] = 0; return b.length; }

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/' || url.pathname === '/index.html') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    return res.end(fs.readFileSync(path.join(root, 'web-ui/portal.html')));
  }
  if (!url.pathname.startsWith('/api/')) { res.writeHead(404); return res.end(); }
  const chunks = []; let size = 0;
  req.on('data', c => { chunks.push(c); size += c.length; if (size > 8192) req.destroy(); });
  req.on('end', () => {
    const body = Buffer.concat(chunks).toString('utf8');
    put(X.sim_methodbuf(), 8, req.method); put(X.sim_pathbuf(), 128, url.pathname); put(X.sim_tokenbuf(), 40, req.headers['x-tuba-token'] || '');
    const n = put(X.sim_inbuf(), X.sim_inbuf_size(), body);
    const code = n < 0 ? 413 : X.sim_api(n, home ? 0 : 1, 0, now());
    res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
    res.end(n < 0 ? '{"ok":false,"error":"corpo grande demais"}' : cstr(X.sim_outbuf()));
  });
}).listen(port, () => console.log(`Portal do TubaBot (núcleo WASM) em http://localhost:${port}/` + (home ? `  | rede de casa: PIN ${cstr(X.sim_pin())}` : '')));
