// Simulador do TubaBot: apresenta o framebuffer do núcleo (WASM) e encaminha entradas.
// Nenhuma regra de jogo/tempo vive aqui — só E/S simulada.
"use strict";
(async function () {
  const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
  const enc = new TextEncoder(), dec = new TextDecoder();
  let X, mem, model = 1, wipeOnly = false;

  // ── armazenamento persistente (equivalente à NVS) ──
  const memStore = {};
  const key = k => `tubabot.sim.m${model}.k${k}`;
  const ls = {
    get(k) { try { return localStorage.getItem(k); } catch (e) { return memStore[k] ?? null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch (e) { memStore[k] = v; } },
    del(k) { try { localStorage.removeItem(k); } catch (e) { delete memStore[k]; } },
  };
  try { model = Number(ls.get("tubabot.sim.model") ?? 1); } catch (e) {}

  // ── relógio virtual ──
  let vt = 0, realLast = performance.now(), speed = 1;
  const effSpeed = () => (X && X.sim_in_game() ? 1 : speed);
  function vtNow() { const r = performance.now(); vt += (r - realLast) * effSpeed(); realLast = r; return Math.floor(vt); }

  // ── log ──
  const logEl = $("#log");
  function log(msg) {
    const s = Math.floor(vt / 1000), line = `[${String(Math.floor(s / 3600)).padStart(2, "0")}:${String(Math.floor(s / 60) % 60).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}] ${msg}\n`;
    logEl.textContent = (line + logEl.textContent).slice(0, 20000);
  }
  const cstr = p => { const u = new Uint8Array(mem.buffer); let e = p; while (u[e]) e++; return dec.decode(u.subarray(p, e)); };
  const b64ToBytes = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));
  const bytesToB64 = b => { let s = ""; for (const x of b) s += String.fromCharCode(x); return btoa(s); };

  let rebootPending = false, contrastAlpha = 1, rotate = false;
  const imports = { env: {
    js_storage_load(kind, ptr, cap) { const v = ls.get(key(kind)); if (!v) return 0; const b = b64ToBytes(v); if (b.length > cap) return 0; new Uint8Array(mem.buffer, ptr, b.length).set(b); return b.length; },
    js_storage_save(kind, ptr, len) { ls.set(key(kind), bytesToB64(new Uint8Array(mem.buffer, ptr, len).slice())); return 1; },
    js_storage_erase(kind) { ls.del(key(kind)); },
    js_log(p) { log(cstr(p)); },
    js_reboot() { rebootPending = true; },
    js_display(c, r) { contrastAlpha = 0.35 + 0.65 * (c / 255); rotate = !!r; },
  } };
  const { instance } = await WebAssembly.instantiate(b64ToBytes(window.TUBABOT_WASM_B64), imports);
  X = instance.exports; mem = X.memory;
  $("#buildInfo").textContent = `núcleo WASM · ${Math.round(b64ToBytes(window.TUBABOT_WASM_B64).length / 1024)} KB`;

  function init(flushFirst) {
    if (flushFirst) X.sim_flush(vtNow());
    X.sim_init(model, (Math.random() * 0xffffffff) >>> 0, 1, vtNow());
    $("#model").value = String(model);
    $("#modelTag").textContent = model ? "Modelo: TubaBot" : "Modelo: TubaBot Lite (sem buzzer/motor)";
    syncPrefs();
  }

  // ── API (mesmo contrato do ESP32) ──
  function writeStr(ptr, cap, s) { const b = enc.encode(s); if (b.length >= cap) return -1; const u = new Uint8Array(mem.buffer, ptr, cap); u.set(b); u[b.length] = 0; return b.length; }
  function api(method, path, body, token, devTools) {
    writeStr(X.sim_methodbuf(), 8, method);
    writeStr(X.sim_pathbuf(), 128, path.split("?")[0]);
    writeStr(X.sim_tokenbuf(), 40, token || "");
    const len = writeStr(X.sim_inbuf(), X.sim_inbuf_size(), body || "");
    if (len < 0) return { status: 413, body: { ok: false, error: "corpo grande demais" } };
    const status = X.sim_api(len, 1, devTools ? 1 : 0, vtNow());
    let out = {}; try { out = JSON.parse(cstr(X.sim_outbuf())); } catch (e) { out = { ok: false, error: "resposta inválida" }; }
    return { status, body: out };
  }
  const tok = () => cstr(X.sim_token());
  const myApi = (m, p, b, dev) => api(m, p, b === undefined ? "" : JSON.stringify(b), tok(), dev);

  // portal (iframe) -> núcleo
  addEventListener("message", e => {
    const m = e.data || {};
    if (m.type !== "tb-api") return;
    const r = api(m.method, m.path, m.body || "", m.token, false);
    e.source.postMessage({ type: "tb-api-res", id: m.id, status: r.status, body: r.body }, "*");
  });

  // ── tela ──
  const cv = $("#oled"), ctx = cv.getContext("2d"), img = ctx.createImageData(128, 64);
  let oledFail = false;
  function draw() {
    const fb = new Uint8Array(mem.buffer, X.sim_frame(), 1024);
    const on = Math.round(255 * contrastAlpha);
    for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) {
      const lit = !oledFail && ((fb[(y >> 3) * 128 + x] >> (y & 7)) & 1);
      const dx = rotate ? 127 - x : x, dy = rotate ? 63 - y : y, i = (dy * 128 + dx) * 4;
      img.data[i] = img.data[i + 1] = img.data[i + 2] = lit ? on : 0; img.data[i + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }

  // ── entrada: mouse, toque e espaço; nunca deixa o sensor virtual preso ──
  const head = $("#head");
  let level = false, stuck = false;
  function setLevel(v) { if (stuck) v = true; if (v === level) return; level = v; X.sim_touch(v ? 1 : 0, vtNow()); head.classList.toggle("on", v); audioInit(); }
  head.addEventListener("pointerdown", e => { e.preventDefault(); head.setPointerCapture(e.pointerId); setLevel(true); });
  ["pointerup", "pointercancel", "lostpointercapture"].forEach(ev => head.addEventListener(ev, () => setLevel(false)));
  addEventListener("keydown", e => { if (e.code === "Space" && !e.target.closest("input,select,textarea,iframe")) { e.preventDefault(); if (!e.repeat) setLevel(true); } });
  addEventListener("keyup", e => { if (e.code === "Space") setLevel(false); });
  addEventListener("blur", () => setLevel(false));
  document.addEventListener("visibilitychange", () => { if (document.hidden) setLevel(false); });

  // gestos injetados (ferramenta de desenvolvedor) em tempo virtual
  const queue = [];
  function inject(seq) { let t = vtNow(); for (const [dt, v] of seq) { t += dt; queue.push([t, v]); } }
  const G = { tap: [[0, 1], [80, 0]], double: [[0, 1], [70, 0], [80, 1], [70, 0]], medium: [[0, 1], [1200, 0]], long: [[0, 1], [3500, 0]], recovery: [[0, 1], [8500, 0]] };
  $$("[data-g]").forEach(b => b.onclick = () => inject(G[b.dataset.g]));

  // ── áudio do buzzer ativo: tom fixo liga/desliga ──
  let ac, osc, gain;
  function audioInit() { if (ac || !$("#hearBuzz").checked) return; try { ac = new AudioContext(); osc = ac.createOscillator(); gain = ac.createGain(); osc.type = "square"; osc.frequency.value = 2700; gain.gain.value = 0; osc.connect(gain).connect(ac.destination); osc.start(); } catch (e) { ac = null; } }
  $("#hearBuzz").onchange = () => { audioInit(); if (!$("#hearBuzz").checked && gain) gain.gain.value = 0; };

  // ── laço ──
  let lastB = false, lastV = false;
  function loop() {
    const now = vtNow();
    while (queue.length && queue[0][0] <= now) { const [t, v] = queue.shift(); X.sim_touch(v, t); head.classList.toggle("on", !!v); }
    if (X.sim_tick(now)) { draw(); X.sim_presented(vtNow()); }
    const b = !!X.sim_buzzer(), v = !!X.sim_motor();
    if (b !== lastB) { $("#ledB").classList.toggle("on", b); $("#dev").classList.toggle("buzz", b); if (gain && $("#hearBuzz").checked) gain.gain.setTargetAtTime(b ? 0.035 : 0, ac.currentTime, 0.003); lastB = b; }
    if (v !== lastV) { $("#ledV").classList.toggle("on", v); $("#dev").classList.toggle("shake", v); lastV = v; }
    const hold = X.sim_pressed() ? X.sim_hold_ms(now) : 0;
    $("#holdbar").style.width = Math.min(100, hold / 80) + "%";
    if (rebootPending) { rebootPending = false; log("— reinício solicitado pelo Tuba —"); init(false); }
    requestAnimationFrame(loop);
  }

  // ── painel de estado ──
  setInterval(() => {
    const s = vt / 1000;
    $("#state").innerHTML = `<dt>Tela</dt><dd>${cstr(X.sim_screen_name())}</dd><dt>Expressão</dt><dd>${cstr(X.sim_expr_name())}</dd>` +
      `<dt>Relógio virtual</dt><dd>${Math.floor(s / 3600)}h ${String(Math.floor(s / 60) % 60).padStart(2, "0")}m ${String(Math.floor(s % 60)).padStart(2, "0")}s · ${effSpeed()}×</dd>` +
      `<dt>Gravações na "flash"</dt><dd>${X.sim_storage_writes()}</dd>`;
    $("#speedNow").textContent = X.sim_in_game() ? "em jogo: 1×" : `${speed}×`;
  }, 250);

  // ── preferências espelhadas ──
  function syncPrefs() {
    const r = myApi("GET", "/api/prefs"); if (r.status !== 200) return; const p = r.body.prefs;
    $("#pSound").checked = p.sound; $("#pSilent").checked = p.silent; $("#pVib").checked = p.vibration;
    ["#pSound", "#pSilent", "#pVib"].forEach(id => $(id).disabled = model === 0);
  }
  ["#pSound", "#pSilent", "#pVib"].forEach(id => $(id).onchange = () => {
    const r = myApi("POST", "/api/prefs", { sound: $("#pSound").checked, silent: $("#pSilent").checked, vibration: $("#pVib").checked });
    log(r.status === 200 ? "preferências de som/vibração aplicadas (saída em andamento cortada se proibida)" : "falha: " + r.body.error);
  });

  // ── controles ──
  $$(".tabs button").forEach(b => b.onclick = () => { $$(".tabs button").forEach(x => x.classList.toggle("on", x === b)); $$(".pane").forEach(p => p.classList.toggle("on", p.id === "p-" + b.dataset.p)); });
  $("#applyModel").onclick = () => { X.sim_flush(vtNow()); model = Number($("#model").value); ls.set("tubabot.sim.model", String(model)); log(`— modelo: ${model ? "TubaBot" : "TubaBot Lite"} —`); init(false); $("#portal").contentWindow.location.reload(); };
  $("#speed").onchange = e => { vtNow(); speed = Number(e.target.value); };
  $$("[data-adv]").forEach(b => b.onclick = () => { vtNow(); vt += Number(b.dataset.adv); log(`+${Number(b.dataset.adv) / 1000} s (relógio virtual)`); });
  $("#sendTime").onclick = () => { let tz = ""; try { tz = Intl.DateTimeFormat().resolvedOptions().timeZone; } catch (e) {}
    const body = { epoch: Math.floor(Date.now() / 1000) }; const zones = myApi("GET", "/api/tz").body.zones || [];
    if (zones.some(z => z.iana === tz)) body.tz = tz; const r = myApi("POST", "/api/time", body); log(r.status === 200 ? "hora do Mac enviada" : "falha: " + r.body.error); };
  $("#reboot").onclick = () => { log("— reinício —"); init(true); };
  $("#powerLoss").onclick = () => { log("— perda de energia: gravações pendentes perdidas; hora civil perdida —"); init(false); };
  $("#wifiBroken").onchange = e => { X.sim_set_wifi_broken(e.target.checked ? 1 : 0); log(e.target.checked ? "Wi-Fi de casa indisponível" : "Wi-Fi de casa disponível"); };
  $("#corrupt").onclick = () => { X.sim_flush(vtNow()); const v = ls.get(key(1)); if (v) { const b = b64ToBytes(v); b[20] ^= 0xff; ls.set(key(1), bytesToB64(b)); } log("— preferências corrompidas; reiniciando —"); init(false); };
  $("#wipe").onclick = () => { for (let k = 1; k <= 3; k++) ls.del(key(k)); log("— dados do simulador apagados —"); init(false); };
  $("#oledFail").onchange = e => { oledFail = e.target.checked; draw(); log(oledFail ? "OLED simulado sem resposta: o núcleo segue funcionando" : "OLED simulado de volta"); };
  $("#stuck").onchange = e => { stuck = e.target.checked; if (stuck) { level = false; setLevel(true); } else { stuck = false; setLevel(false); } };
  $("#burst").onclick = () => { for (let i = 0; i < 3; i++) myApi("POST", "/api/action", { action: "timer.start", slot: i, seconds: 5 }); log("3 timers de 5 s iniciados"); };
  $("#devUnlock").onclick = () => { const r = myApi("POST", "/api/action", { action: "dev.unlockAll" }, true); log(r.status === 200 ? "perfil de TESTE: expressões desbloqueadas (não gravado)" : "falha: " + r.body.error); };
  $("#devTestOff").onclick = () => { myApi("POST", "/api/action", { action: "dev.testProfileOff" }, true); log("perfil de teste desligado"); };

  init(false);
  draw();
  requestAnimationFrame(loop);
})();
