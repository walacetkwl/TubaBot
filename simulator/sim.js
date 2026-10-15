// Simulador do TubaBot v2: apresenta a imagem do núcleo (WASM), toca o áudio
// sintetizado por ele e simula toque, movimento, som ambiente, bateria e RTC.
// Nenhuma regra de jogo/tempo/interface vive aqui — só E/S simulada.
"use strict";
(async function () {
  const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
  const enc = new TextEncoder(), dec = new TextDecoder();
  let X, mem;

  // ── armazenamento persistente (equivalente à NVS) ──
  const memStore = {};
  const ls = {
    get(k) { try { return localStorage.getItem(k); } catch (e) { return memStore[k] ?? null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch (e) { memStore[k] = v; } },
    del(k) { try { localStorage.removeItem(k); } catch (e) { delete memStore[k]; } },
  };
  const key = k => `tubabot.v2.k${k}`;
  let caps = Number(ls.get("tubabot.v2.caps") ?? (1 | 4 | 8 | 16 | 32));  // alto-falante, IMU, mic, bateria, RTC

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

  // ── RTC com "bateria": a hora sobrevive aos reinícios do simulador ──
  function rtcNow() { const v = ls.get("tubabot.v2.rtc"); if (!v) return 0; const [ep, at] = v.split(",").map(Number); return ep + (Date.now() - at) / 1000; }

  let rebootPending = false, bright = 255, rotate = false;
  const imports = { env: {
    js_storage_load(kind, ptr, cap) { const v = ls.get(key(kind)); if (!v) return 0; const b = b64ToBytes(v); if (b.length > cap) return 0; new Uint8Array(mem.buffer, ptr, b.length).set(b); return b.length; },
    js_storage_save(kind, ptr, len) { ls.set(key(kind), bytesToB64(new Uint8Array(mem.buffer, ptr, len).slice())); return 1; },
    js_storage_erase(kind) { ls.del(key(kind)); },
    js_log(p) { log(cstr(p)); },
    js_reboot() { rebootPending = true; },
    js_display(b, r) { bright = b; rotate = !!r; applyLook(); },
    js_rtc(ep) { ls.set("tubabot.v2.rtc", `${Math.floor(ep)},${Date.now()}`); },
  } };
  const wasmBytes = b64ToBytes(window.TUBABOT_WASM_B64);
  const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
  X = instance.exports; mem = X.memory;
  $("#buildInfo").textContent = `núcleo WASM · ${Math.round(wasmBytes.length / 1024)} KB`;

  function init(flushFirst) {
    if (flushFirst) X.sim_flush(vtNow());
    X.sim_init((Math.random() * 0xffffffff) >>> 0, 1, vtNow(), caps);
    const ep = rtcNow(); if (ep > 1577836800 && (caps & 32)) X.sim_rtc_time(ep, vtNow());
    $("#cSpk").checked = !!(caps & 1); $("#cMot").checked = !!(caps & 2); $("#cBat").checked = !!(caps & 16);
    sendPower();
    full = true;
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
  addEventListener("message", e => {
    const m = e.data || {};
    if (m.type !== "tb-api") return;
    const r = api(m.method, m.path, m.body || "", m.token, false);
    e.source.postMessage({ type: "tb-api-res", id: m.id, status: r.status, body: r.body }, "*");
  });

  // ── tela (copia só a área alterada) ──
  const cv = $("#scr"), ctx = cv.getContext("2d"), img = ctx.createImageData(466, 466);
  let full = true, scrFail = false;
  function applyLook() {
    cv.style.filter = bright === 0 ? "brightness(0)" : `brightness(${(0.25 + 0.75 * bright / 255).toFixed(3)})`;
    cv.style.transform = rotate ? "rotate(180deg)" : "";
  }
  function draw() {
    const px = new Uint16Array(mem.buffer, X.sim_frame(), 466 * 466);
    let x0 = X.sim_dirty(0), y0 = X.sim_dirty(1), x1 = X.sim_dirty(2), y1 = X.sim_dirty(3);
    if (full) { x0 = 0; y0 = 0; x1 = 466; y1 = 466; full = false; }
    if (x1 <= x0 || y1 <= y0) return;
    const d = img.data;
    for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
      const v = scrFail ? 0 : px[y * 466 + x], i = (y * 466 + x) * 4;
      const r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
      d[i] = (r << 3) | (r >> 2); d[i + 1] = (g << 2) | (g >> 4); d[i + 2] = (b << 3) | (b >> 2); d[i + 3] = 255;
    }
    ctx.putImageData(img, 0, 0, x0, y0, x1 - x0, y1 - y0);
  }

  // ── toque: mouse/caneta/dedo; nunca deixa o toque preso ──
  let down = false, stuck = false, lastX = 233, lastY = 233;
  function pos(e) { const r = cv.getBoundingClientRect(); let x = (e.clientX - r.left) * 466 / r.width, y = (e.clientY - r.top) * 466 / r.height; if (rotate) { x = 465 - x; y = 465 - y; } return [Math.max(0, Math.min(465, Math.round(x))), Math.max(0, Math.min(465, Math.round(y)))]; }
  function send(d, x, y) { lastX = x; lastY = y; X.sim_touch(d ? 1 : 0, x, y, vtNow()); }
  cv.addEventListener("pointerdown", e => { e.preventDefault(); cv.setPointerCapture(e.pointerId); audioInit(); down = true; const [x, y] = pos(e); send(true, x, y); });
  cv.addEventListener("pointermove", e => { if (!down) return; const [x, y] = pos(e); send(true, x, y); });
  const release = e => { if (!down || stuck) return; down = false; const [x, y] = e && e.clientX !== undefined ? pos(e) : [lastX, lastY]; send(false, x, y); };
  ["pointerup", "pointercancel", "lostpointercapture"].forEach(ev => cv.addEventListener(ev, release));
  addEventListener("blur", () => release());
  document.addEventListener("visibilitychange", () => { if (document.hidden) release(); });

  // ── movimento: vetor gravidade + eventos ──
  let tiltX = 0, tiltZ = 0, pose = "up", shakeUntil = 0, pickUntil = 0;
  const pad = $("#pad"), dot = $("#padDot");
  function setPad(e) { const r = pad.getBoundingClientRect(); tiltX = Math.max(-1, Math.min(1, ((e.clientX - r.left) / r.width) * 2 - 1)); tiltZ = Math.max(-1, Math.min(1, ((e.clientY - r.top) / r.height) * 2 - 1)); dot.style.left = (50 + tiltX * 50) + "%"; dot.style.top = (50 + tiltZ * 50) + "%"; }
  pad.addEventListener("pointerdown", e => { pad.setPointerCapture(e.pointerId); setPad(e); pad.dataset.drag = 1; });
  pad.addEventListener("pointermove", e => { if (pad.dataset.drag) setPad(e); });
  pad.addEventListener("pointerup", () => { delete pad.dataset.drag; });
  pad.addEventListener("dblclick", () => { tiltX = tiltZ = 0; dot.style.left = dot.style.top = "50%"; });
  function gravity() {
    if (pose === "down") return [0, 0, 1000];
    if (pose === "back") return [0, 0, -1000];
    if (pose === "upside") return [0, -1000, 0];
    const gx = tiltX * 600, gz = tiltZ * 600, gy = Math.sqrt(Math.max(0, 1e6 - gx * gx - gz * gz));
    return [gx, gy, gz];
  }
  function motionTick(now) {
    let [x, y, z] = gravity();
    if (now < shakeUntil) { const k = Math.floor(now / 80) % 2 ? 1 : -1; x += 1800 * k; z -= 900 * k; $("#dev").classList.add("shake"); }
    else $("#dev").classList.toggle("shake", !!X.sim_motor());
    if (now < pickUntil) { y += 450; x += 120; }
    X.sim_motion(Math.round(x), Math.round(y), Math.round(z), now);
    $("#body").style.transform = pose === "down" ? "scale(.92) rotateX(70deg)" : pose === "back" ? "scale(.92) rotateX(-55deg)" : pose === "upside" ? "rotate(180deg)" : `rotate(${tiltX * 25}deg)`;
  }
  $("#shake").onclick = () => { shakeUntil = vtNow() + 900; log("sacudido"); };
  $("#pick").onclick = () => { pickUntil = vtNow() + 300; log("pego no colo"); };
  const poseBtn = (id, p) => $(id).onclick = () => { pose = pose === p ? "up" : p; $$("#faceDown,#onBack,#upside").forEach(b => b.classList.remove("on")); if (pose !== "up") $(id).classList.add("on"); log("posição: " + pose); };
  poseBtn("#faceDown", "down"); poseBtn("#onBack", "back"); poseBtn("#upside", "upside");

  // ── som ambiente (nível) ──
  let clapUntil = 0, loudUntil = 0, music = false, micLevel = -1, analyser = null, micBuf = null;
  $("#lvl").oninput = e => $("#lvlv").textContent = e.target.value;
  $("#clap").onclick = () => { clapUntil = vtNow() + 90; log("palma"); };
  $("#loud").onclick = () => { loudUntil = vtNow() + 2200; log("barulho alto"); };
  $("#music").onclick = () => { music = !music; $("#music").classList.toggle("on", music); log(music ? "música tocando" : "música parou"); };
  $("#realMic").onchange = async e => {
    if (!e.target.checked) { analyser = null; micLevel = -1; return; }
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

  // ── energia ──
  function sendPower() {
    const bat = !!(caps & 16);
    X.sim_power(bat ? 1 : 0, Number($("#bat").value), $("#chg").checked ? 1 : 0, $("#chg").checked || !bat ? 1 : 0, vtNow());
  }
  $("#bat").oninput = e => { $("#batv").textContent = e.target.value + "%"; sendPower(); };
  $("#chg").onchange = () => { sendPower(); log($("#chg").checked ? "carregando" : "desconectado do USB"); };

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
  let lastMotion = 0, lastSound = 0, lastWeather = 0, lastV = false, lastS = false;
  function loop() {
    const now = vtNow();
    if (stuck) X.sim_touch(1, lastX, lastY, now);
    if (now - lastMotion >= 20 && caps & 4) { lastMotion = now; motionTick(now); }
    if (now - lastSound >= 40 && caps & 8) { lastSound = now; soundTick(now); }
    if (now - lastWeather >= 2000) { lastWeather = now; weatherTick(now); }
    if (X.sim_tick(now)) { draw(); X.sim_presented(vtNow()); }
    const v = !!X.sim_motor(), s = !!X.sim_sound_active(now);
    if (v !== lastV) { $("#ledV").classList.toggle("on", v); lastV = v; }
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

  // ── botões físicos ──
  $("#bBoot").onclick = () => X.sim_button(0, 0, vtNow());
  $("#bBootL").onclick = () => { X.sim_button(0, 1, vtNow()); log("BOOT segurado: rede de configuração"); };
  $("#bPwr").onclick = () => X.sim_button(1, 0, vtNow());

  // ── abas e controles ──
  $$(".tabs button").forEach(b => b.onclick = () => { $$(".tabs button").forEach(x => x.classList.toggle("on", x === b)); $$(".pane").forEach(p => p.classList.toggle("on", p.id === "p-" + b.dataset.p)); });
  $("#applyCaps").onclick = () => {
    X.sim_flush(vtNow());
    caps = ($("#cSpk").checked ? 1 : 0) | ($("#cMot").checked ? 2 : 0) | 4 | 8 | ($("#cBat").checked ? 16 : 0) | 32;
    ls.set("tubabot.v2.caps", String(caps)); log("— hardware alterado; reiniciando —"); init(false);
  };
  $("#speed").onchange = e => { vtNow(); speed = Number(e.target.value); };
  $$("[data-adv]").forEach(b => b.onclick = () => { vtNow(); vt += Number(b.dataset.adv); log(`+${Number(b.dataset.adv) / 1000} s (relógio virtual)`); });
  $("#sendTime").onclick = () => { let tz = ""; try { tz = Intl.DateTimeFormat().resolvedOptions().timeZone; } catch (e) {}
    const body = { epoch: Math.floor(Date.now() / 1000) }; const zones = myApi("GET", "/api/tz").body.zones || [];
    if (zones.some(z => z.iana === tz)) body.tz = tz; const r = myApi("POST", "/api/time", body); log(r.status === 200 ? "hora do computador enviada (gravada no RTC)" : "falha: " + r.body.error); };
  $("#reboot").onclick = () => { log("— reinício —"); init(true); };
  $("#powerLoss").onclick = () => { log("— perda de energia: gravações pendentes perdidas; o RTC mantém a hora —"); init(false); };
  $("#wifiBroken").onchange = e => { X.sim_set_wifi_broken(e.target.checked ? 1 : 0); log(e.target.checked ? "Wi-Fi de casa indisponível" : "Wi-Fi de casa disponível"); };
  $("#corrupt").onclick = () => { X.sim_flush(vtNow()); const v = ls.get(key(1)); if (v) { const b = b64ToBytes(v); b[20] ^= 0xff; ls.set(key(1), bytesToB64(b)); } log("— preferências corrompidas; reiniciando —"); init(false); };
  $("#wipe").onclick = () => { for (let k = 1; k <= 3; k++) ls.del(key(k)); ls.del("tubabot.v2.rtc"); log("— dados do simulador apagados —"); init(false); };
  $("#scrFail").onchange = e => { scrFail = e.target.checked; full = true; draw(); log(scrFail ? "tela sem resposta: o núcleo segue funcionando" : "tela de volta"); };
  $("#stuck").onchange = e => { stuck = e.target.checked; if (stuck) { down = true; send(true, 233, 233); } else { stuck = false; down = false; send(false, 233, 233); } };
  $("#burst").onclick = () => { for (let i = 0; i < 3; i++) myApi("POST", "/api/action", { action: "timer.start", slot: i, seconds: 5 }); log("3 timers de 5 s iniciados"); };
  $("#devUnlock").onclick = () => { const r = myApi("POST", "/api/action", { action: "dev.unlockAll" }, true); log(r.status === 200 ? "perfil de TESTE: expressões desbloqueadas (não gravado)" : "falha: " + r.body.error); };
  $("#devTestOff").onclick = () => { myApi("POST", "/api/action", { action: "dev.testProfileOff" }, true); log("perfil de teste desligado"); };

  // teste automatizado (e2e) acessa o núcleo por aqui
  window.TB = { X, api: myApi, vtNow, init, cstr };

  init(false);
  applyLook();
  requestAnimationFrame(loop);
})();
