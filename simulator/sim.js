// Simulador do TubaBot v3 (ES3C28P, paisagem 320×240): apresenta a imagem do núcleo
// (WASM), toca o áudio sintetizado por ele e simula toque, microfone, bateria (Pro)
// e o perfil de fábrica da unidade. Nenhuma regra de jogo/tempo/interface vive aqui.
"use strict";
(async function () {
  const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
  const enc = new TextEncoder(), dec = new TextDecoder();
  const W = 320, H = 240;
  let X, mem;

  // ── armazenamento persistente (equivalente à NVS e à partição "fabrica") ──
  const memStore = {};
  const ls = {
    get(k) { try { return localStorage.getItem(k); } catch (e) { return memStore[k] ?? null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch (e) { memStore[k] = v; } },
    del(k) { try { localStorage.removeItem(k); } catch (e) { delete memStore[k]; } },
  };
  const key = k => `tubabot.v3.k${k}`;
  const FAB_KEY = "tubabot.v3.fabrica";
  // bit0 alto-falante, bit3 microfone, bit4 conector de bateria (o perfil de fábrica decide Lite/Pro)
  let caps = Number(ls.get("tubabot.v3.caps") ?? (1 | 8 | 16));

  // ── relógio virtual ──
  let vt = 0, realLast = performance.now(), speed = 1;
  const effSpeed = () => (X && X.sim_in_game() ? 1 : speed);
  function vtNow() { const r = performance.now(); vt += (r - realLast) * effSpeed(); realLast = r; return Math.floor(vt); }

  // ── log ──
  const logEl = $("#log");
  function log(msg) {
    const s = Math.floor(vt / 1000), line = `[${String(Math.floor(s / 3600)).padStart(2, "0")}:${String(Math.floor(s / 60) % 60).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}] ${msg}\n`;
    logEl.textContent = (line + logEl.textContent).slice(0, 30000);
  }
  const cstr = p => { const u = new Uint8Array(mem.buffer); let e = p; while (u[e]) e++; return dec.decode(u.subarray(p, e)); };
  const b64ToBytes = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));
  const bytesToB64 = b => { let s = ""; for (const x of b) s += String.fromCharCode(x); return btoa(s); };

  let rebootPending = false, bright = 255, rotate = false;
  const imports = { env: {
    js_storage_load(kind, ptr, cap) { const v = ls.get(key(kind)); if (!v) return 0; const b = b64ToBytes(v); if (b.length > cap) return 0; new Uint8Array(mem.buffer, ptr, b.length).set(b); return b.length; },
    js_storage_save(kind, ptr, len) { ls.set(key(kind), bytesToB64(new Uint8Array(mem.buffer, ptr, len).slice())); return 1; },
    js_storage_erase(kind) { ls.del(key(kind)); },
    js_log(p) { log(cstr(p)); },
    js_reboot() { rebootPending = true; },
    js_display(b, r) { bright = b; rotate = !!r; applyLook(); },
    js_rtc() {},
  } };
  const wasmBytes = b64ToBytes(window.TUBABOT_WASM_B64);
  const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
  X = instance.exports; mem = X.memory;
  $("#buildInfo").textContent = `núcleo WASM · ${Math.round(wasmBytes.length / 1024)} KB`;

  function writeStr(ptr, cap, s) { const b = enc.encode(s); if (b.length >= cap) return -1; const u = new Uint8Array(mem.buffer, ptr, cap); u.set(b); u[b.length] = 0; return b.length; }

  // ── perfil de fábrica ──
  const BODY_RGB = { vermelho: "#c9303a", azul: "#2e6ccb", branco: "#eeeeea", grafite: "#3a3e45" };
  function composeFactory(f) {
    let t = `modelo=${f.modelo}\ncor=${f.cor}\n`;
    if (f.expressao) t += `expressao=${f.expressao}\n`;
    t += `tema=${f.tema}\n`;
    if (f.serie) t += `serie=${f.serie}\n`;
    const len = writeStr(X.sim_inbuf(), X.sim_inbuf_size(), t);
    const n = X.sim_factory_compose(len);
    const out = cstr(X.sim_outbuf());
    if (n < 0) { log("perfil recusado: " + out); return null; }
    return out;
  }
  function parseFab(text) {
    const o = {}; (text || "").split("\n").forEach(l => { const i = l.indexOf("="); if (i > 0) o[l.slice(0, i)] = l.slice(i + 1); });
    return o;
  }
  if (!ls.get(FAB_KEY)) { const t = composeFactory({ modelo: "pro", cor: "vermelho", tema: "escuro" }); if (t) ls.set(FAB_KEY, t); }
  function showFactory() {
    const t = ls.get(FAB_KEY), f = parseFab(t);
    $("#fText").textContent = t || "(sem perfil gravado — a unidade se comporta como Lite genérico)";
    if (t) { $("#fModel").value = f.modelo; $("#fBody").value = f.cor; $("#fTheme").value = f.tema; $("#fExpr").value = f.expressao === f.cor ? "" : f.expressao; $("#fSerial").value = f.serie || ""; }
    const body = t ? f.cor : "grafite";
    document.documentElement.style.setProperty("--body", BODY_RGB[body] || BODY_RGB.grafite);
    $("#badge").textContent = t ? `TubaBot ${f.modelo === "pro" ? "Pro" : "Lite"}` : "sem perfil";
    $("#badge").style.color = body === "branco" ? "rgba(0,0,0,.55)" : "rgba(255,255,255,.75)";
    const pro = t && f.modelo === "pro";
    $("#pwrPro").style.display = pro ? "" : "none"; $("#pwrLite").style.display = pro ? "none" : "";
  }

  function init(flushFirst) {
    if (flushFirst) X.sim_flush(vtNow());
    const t = ls.get(FAB_KEY) || "";
    const len = t ? writeStr(X.sim_inbuf(), X.sim_inbuf_size(), t) : 0;
    X.sim_factory_set(Math.max(0, len));
    X.sim_init((Math.random() * 0xffffffff) >>> 0, 1, vtNow(), caps);
    $("#cSpk").checked = !!(caps & 1); $("#cMic").checked = !!(caps & 8);
    showFactory();
    sendPower();
    full = true;
    // sem RTC: a hora volta pela rede de casa (NTP) quando ela está configurada
    setTimeout(() => {
      const w = myApi("GET", "/api/wifi").body, p = myApi("GET", "/api/prefs").body;
      if (w.configured && !$("#wifiBroken").checked && p.prefs && p.prefs.ntpEnabled) { myApi("POST", "/api/time", { epoch: Math.floor(Date.now() / 1000) }); log("hora recebida pela rede (NTP simulado)"); }
    }, 300);
  }

  // ── API (mesmo contrato do ESP32) ──
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
  addEventListener("message", e => {
    const m = e.data || {};
    if (m.type !== "tb-api") return;
    const r = api(m.method, m.path, m.body || "", m.token, false);
    e.source.postMessage({ type: "tb-api-res", id: m.id, status: r.status, body: r.body }, "*");
  });

  // ── tela (copia só a área alterada) ──
  const cv = $("#scr"), ctx = cv.getContext("2d"), img = ctx.createImageData(W, H);
  let full = true, scrFail = false;
  function applyLook() {
    cv.style.filter = bright === 0 ? "brightness(0)" : `brightness(${(0.3 + 0.7 * bright / 255).toFixed(3)})`;
    cv.style.transform = rotate ? "rotate(180deg)" : "";
  }
  function draw() {
    const px = new Uint16Array(mem.buffer, X.sim_frame(), W * H);
    let x0 = X.sim_dirty(0), y0 = X.sim_dirty(1), x1 = X.sim_dirty(2), y1 = X.sim_dirty(3);
    if (full) { x0 = 0; y0 = 0; x1 = W; y1 = H; full = false; }
    if (x1 <= x0 || y1 <= y0) return;
    const d = img.data;
    for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
      const v = scrFail ? 0 : px[y * W + x], i = (y * W + x) * 4;
      const r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
      d[i] = (r << 3) | (r >> 2); d[i + 1] = (g << 2) | (g >> 4); d[i + 2] = (b << 3) | (b >> 2); d[i + 3] = 255;
    }
    ctx.putImageData(img, 0, 0, x0, y0, x1 - x0, y1 - y0);
  }

  // ── toque: mouse/caneta/dedo; nunca deixa o toque preso ──
  let down = false, stuck = false, lastX = W / 2, lastY = H / 2;
  function pos(e) { const r = cv.getBoundingClientRect(); let x = (e.clientX - r.left) * W / r.width, y = (e.clientY - r.top) * H / r.height; if (rotate) { x = W - 1 - x; y = H - 1 - y; } return [Math.max(0, Math.min(W - 1, Math.round(x))), Math.max(0, Math.min(H - 1, Math.round(y)))]; }
  function send(d, x, y) { lastX = x; lastY = y; X.sim_touch(d ? 1 : 0, x, y, vtNow()); }
  cv.addEventListener("pointerdown", e => { e.preventDefault(); cv.setPointerCapture(e.pointerId); audioInit(); down = true; const [x, y] = pos(e); send(true, x, y); });
  cv.addEventListener("pointermove", e => { if (!down) return; const [x, y] = pos(e); send(true, x, y); });
  const release = e => { if (!down || stuck) return; down = false; const [x, y] = e && e.clientX !== undefined ? pos(e) : [lastX, lastY]; send(false, x, y); };
  ["pointerup", "pointercancel", "lostpointercapture"].forEach(ev => cv.addEventListener(ev, release));
  addEventListener("blur", () => release());
  document.addEventListener("visibilitychange", () => { if (document.hidden) release(); });

  // ── som ambiente (nível) ──
  let clapUntil = 0, loudUntil = 0, music = false, analyser = null, micBuf = null;
  $("#lvl").oninput = e => $("#lvlv").textContent = e.target.value;
  $("#clap").onclick = () => { clapUntil = vtNow() + 90; log("palma"); };
  $("#loud").onclick = () => { loudUntil = vtNow() + 2200; log("barulho alto"); };
  $("#music").onclick = () => { music = !music; $("#music").classList.toggle("on", music); log(music ? "música tocando" : "música parou"); };
  $("#realMic").onchange = async e => {
    if (!e.target.checked) { analyser = null; return; }
    try {
      const st = await navigator.mediaDevices.getUserMedia({ audio: true });
      const mac = new AudioContext(); const src = mac.createMediaStreamSource(st);
      analyser = mac.createAnalyser(); analyser.fftSize = 1024; micBuf = new Float32Array(1024); src.connect(analyser);
      log("microfone do computador: só o nível é usado");
    } catch (err) { e.target.checked = false; log("microfone indisponível: " + err.message); }
  };
  function soundTick(now) {
    let lv = Number($("#lvl").value);
    if (analyser) { analyser.getFloatTimeDomainData(micBuf); let s = 0; for (const v of micBuf) s += v * v; const rms = Math.sqrt(s / micBuf.length); lv = Math.max(0, Math.min(100, Math.round(20 * Math.log10(rms + 1e-9) + 100))); }
    if (music) lv = Math.max(lv, (now % 500) < 80 ? 70 : 52);
    if (now < loudUntil) lv = 92;
    if (now < clapUntil) lv = 82;
    $("#lvlBar").style.width = lv + "%";
    X.sim_sound(lv, now);
  }

  // ── energia (bateria lida pelo ADC; o núcleo ignora no Lite) ──
  function sendPower() {
    const bat = $("#batIn").checked;
    X.sim_power(bat ? 1 : 0, Number($("#bat").value), 0, 0, vtNow());
  }
  $("#bat").oninput = e => { $("#batv").textContent = e.target.value + "%"; sendPower(); };
  $("#batIn").onchange = () => { sendPower(); log($("#batIn").checked ? "bateria conectada" : "bateria desconectada"); };

  // ── áudio: o núcleo sintetiza a 16 kHz; o navegador só toca ──
  let ac = null;
  function audioInit() {
    if (ac || !$("#hear").checked) return;
    try {
      ac = new AudioContext({ sampleRate: 16000 });
      const sp = ac.createScriptProcessor(1024, 0, 1);
      sp.onaudioprocess = ev => {
        const out = ev.outputBuffer.getChannelData(0);
        const p = X.sim_audio(out.length), a = new Int16Array(mem.buffer, p, out.length);
        const on = $("#hear").checked;
        for (let i = 0; i < out.length; i++) out[i] = on ? a[i] / 32768 : 0;
      };
      sp.connect(ac.destination);
    } catch (e) { ac = null; log("áudio indisponível neste navegador"); }
  }
  addEventListener("pointerdown", audioInit, { once: true });
  addEventListener("keydown", audioInit, { once: true });

  // ── clima: o simulador faz o GET que o ESP32 faria ──
  let weatherBusy = false;
  function weatherTick(now) {
    if (weatherBusy) return;
    const p = X.sim_weather_want(now); if (!p) return;
    const url = cstr(p); weatherBusy = true; log("clima: consultando " + url.split("?")[0]);
    fetch(url).then(async r => ({ code: r.status, text: await r.text() })).catch(() => ({ code: 0, text: "" })).then(({ code, text }) => {
      const len = writeStr(X.sim_inbuf(), X.sim_inbuf_size(), text);
      X.sim_weather_result(Math.max(0, len), len < 0 ? 0 : code, vtNow());
      weatherBusy = false;
    });
  }

  // ── laço ──
  let lastSound = 0, lastWeather = 0, lastS = false;
  function loop() {
    const now = vtNow();
    if (stuck) X.sim_touch(1, lastX, lastY, now);
    if (now - lastSound >= 40 && caps & 8) { lastSound = now; soundTick(now); }
    if (now - lastWeather >= 2000) { lastWeather = now; weatherTick(now); }
    if (X.sim_tick(now)) { draw(); X.sim_presented(vtNow()); }
    const s = !!X.sim_sound_active(now);
    if (s !== lastS) { $("#ledS").classList.toggle("on", s); lastS = s; }
    if (rebootPending) { rebootPending = false; log("— reinício solicitado pelo Tuba —"); init(false); }
    requestAnimationFrame(loop);
  }

  // ── painel de estado ──
  setInterval(() => {
    const s = vt / 1000;
    $("#state").innerHTML = `<dt>Tela</dt><dd>${cstr(X.sim_screen_name())}</dd><dt>Expressão</dt><dd>${cstr(X.sim_expr_name())}</dd>` +
      `<dt>Brilho</dt><dd>${X.sim_brightness() ? Math.round(X.sim_brightness() / 2.55) + "%" : "tela apagada"}</dd>` +
      `<dt>Relógio virtual</dt><dd>${Math.floor(s / 3600)}h ${String(Math.floor(s / 60) % 60).padStart(2, "0")}m ${String(Math.floor(s % 60)).padStart(2, "0")}s · ${effSpeed()}×</dd>` +
      `<dt>Gravações na "flash"</dt><dd>${X.sim_storage_writes()}</dd>`;
    $("#speedNow").textContent = X.sim_in_game() ? "em jogo: 1×" : `${speed}×`;
  }, 250);
  setInterval(sendPower, 5000);

  // ── botão físico BOOT ──
  $("#bBoot").onclick = () => X.sim_button(0, 0, vtNow());
  $("#bBootL").onclick = () => { X.sim_button(0, 1, vtNow()); log("BOOT segurado: rede de configuração"); };

  // ── abas e controles ──
  $$(".tabs button").forEach(b => b.onclick = () => { $$(".tabs button").forEach(x => x.classList.toggle("on", x === b)); $$(".pane").forEach(p => p.classList.toggle("on", p.id === "p-" + b.dataset.p)); });
  $("#applyCaps").onclick = () => {
    X.sim_flush(vtNow());
    caps = ($("#cSpk").checked ? 1 : 0) | ($("#cMic").checked ? 8 : 0) | 16;
    ls.set("tubabot.v3.caps", String(caps)); log("— hardware alterado; reiniciando —"); init(false);
  };
  const fabFields = () => ({ modelo: $("#fModel").value, cor: $("#fBody").value, expressao: $("#fExpr").value, tema: $("#fTheme").value, serie: $("#fSerial").value.trim() });
  const wipeUser = () => { for (let k = 1; k <= 3; k++) ls.del(key(k)); };
  $("#fWrite").onclick = () => { const t = composeFactory(fabFields()); if (!t) return; X.sim_flush(vtNow()); ls.set(FAB_KEY, t); wipeUser(); log("— perfil de fábrica gravado; unidade nova —"); init(false); };
  $("#fKeep").onclick = () => { const t = composeFactory(fabFields()); if (!t) return; X.sim_flush(vtNow()); ls.set(FAB_KEY, t); log("— perfil de fábrica gravado (dados mantidos) —"); init(false); };
  $("#fRestore").onclick = () => { const r = myApi("POST", "/api/reset", { scope: "factory", confirm: "FABRICA" }); log(r.status === 200 ? "restaurando padrão de fábrica..." : "falha: " + r.body.error); };
  $("#fClear").onclick = () => { X.sim_flush(vtNow()); ls.del(FAB_KEY); log("— perfil de fábrica apagado (unidade não preparada) —"); init(false); };
  $("#speed").onchange = e => { vtNow(); speed = Number(e.target.value); };
  $$("[data-adv]").forEach(b => b.onclick = () => { vtNow(); vt += Number(b.dataset.adv); log(`+${Number(b.dataset.adv) / 1000} s (relógio virtual)`); });
  $("#sendTime").onclick = () => { let tz = ""; try { tz = Intl.DateTimeFormat().resolvedOptions().timeZone; } catch (e) {}
    const body = { epoch: Math.floor(Date.now() / 1000) }; const zones = myApi("GET", "/api/tz").body.zones || [];
    if (zones.some(z => z.iana === tz)) body.tz = tz; const r = myApi("POST", "/api/time", body); log(r.status === 200 ? "hora do computador enviada" : "falha: " + r.body.error); };
  $("#reboot").onclick = () => { log("— reinício —"); init(true); };
  $("#powerLoss").onclick = () => { log("— perda de energia: gravações pendentes e a hora são perdidas (sem RTC) —"); init(false); };
  $("#wifiBroken").onchange = e => { X.sim_set_wifi_broken(e.target.checked ? 1 : 0); log(e.target.checked ? "Wi-Fi de casa indisponível" : "Wi-Fi de casa disponível"); };
  $("#corrupt").onclick = () => { X.sim_flush(vtNow()); const v = ls.get(key(1)); if (v) { const b = b64ToBytes(v); b[20] ^= 0xff; ls.set(key(1), bytesToB64(b)); } log("— preferências corrompidas; reiniciando —"); init(false); };
  $("#wipe").onclick = () => { wipeUser(); log("— dados do usuário apagados (perfil de fábrica mantido) —"); init(false); };
  $("#scrFail").onchange = e => { scrFail = e.target.checked; full = true; draw(); log(scrFail ? "tela sem resposta: o núcleo segue funcionando" : "tela de volta"); };
  $("#stuck").onchange = e => { stuck = e.target.checked; if (stuck) { down = true; send(true, W / 2, H / 2); } else { stuck = false; down = false; send(false, W / 2, H / 2); } };
  $("#burst").onclick = () => { for (let i = 0; i < 3; i++) myApi("POST", "/api/action", { action: "timer.start", slot: i, seconds: 5 }); log("3 timers de 5 s iniciados"); };
  $("#devUnlock").onclick = () => { const r = myApi("POST", "/api/action", { action: "dev.unlockAll" }, true); log(r.status === 200 ? "perfil de TESTE: expressões desbloqueadas (não gravado)" : "falha: " + r.body.error); };
  $("#devTestOff").onclick = () => { myApi("POST", "/api/action", { action: "dev.testProfileOff" }, true); log("perfil de teste desligado"); };

  // teste automatizado (e2e) acessa o núcleo por aqui
  window.TB = { X, api: myApi, vtNow, init, cstr, composeFactory, FAB_KEY, ls };

  init(false);
  applyLook();
  requestAnimationFrame(loop);
})();
