// Teste de ponta a ponta do simulador no navegador (Playwright + Chromium).
// Uso: node tests/e2e_sim.mjs [pasta-de-capturas]
import { createRequire } from 'node:module';
import path from 'node:path';
import fs from 'node:fs';
const require = createRequire(import.meta.url);
let pw;
try { pw = require('playwright'); } catch (e) { pw = require(process.env.PLAYWRIGHT_PATH || '/opt/node-tools/node_modules/playwright'); }
const out = process.argv[2] || 'tests/out';
fs.mkdirSync(out, { recursive: true });
const exe = process.env.CHROME || (fs.existsSync('/opt/google/chrome/chrome') ? '/opt/google/chrome/chrome' : undefined);
const browser = await pw.chromium.launch({ executablePath: exe, args: ['--allow-file-access-from-files'] });
const page = await browser.newPage({ viewport: { width: 1360, height: 900 } });
const errors = [];
page.on('pageerror', e => errors.push(String(e)));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
const url = 'file://' + path.resolve('simulator/index.html');
await page.goto(url);
await page.evaluate(() => localStorage.clear());
await page.reload();
await page.waitForTimeout(3500);
let fails = 0;
const ok = (c, msg) => { console.log((c ? 'ok   ' : 'FAIL ') + msg); if (!c) fails++; };
const state = async () => page.$eval('#state', e => e.textContent);
ok((await state()).includes('first_use'), 'primeiro uso aparece no simulador');
await page.screenshot({ path: `${out}/01_primeiro_uso.png` });
// Portal: assistente
const fr = page.frameLocator('#portal');
await page.click('.tabs button[data-p=portal]');
await fr.locator('#wNick').waitFor({ timeout: 5000 });
await fr.locator('#wNick').fill('Júlia');
await fr.locator('[data-step="0"] [data-next]').click();
await page.waitForTimeout(400);
await fr.locator('[data-step="1"] [data-next]').click();
await page.waitForTimeout(400);
await fr.locator('#wDone').click();
await page.waitForTimeout(800);
ok((await state()).includes('face'), 'assistente do portal conclui o primeiro uso (rosto)');
await page.screenshot({ path: `${out}/02_portal.png` });
// Gestos na cabeça: segurar 1,2 s = carinho
await page.click('.tabs button[data-p=log]');
const head = page.locator('#head');
const box = await head.boundingBox();
await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
await page.mouse.down(); await page.waitForTimeout(1300); await page.mouse.up();
await page.waitForTimeout(500);
ok((await page.$eval('#log', e => e.textContent)).includes('carinho'), 'segurar a cabeça 1,2 s gera carinho');
// espaço: duplo toque -> menu
await page.keyboard.down('Space'); await page.waitForTimeout(70); await page.keyboard.up('Space');
await page.waitForTimeout(80);
await page.keyboard.down('Space'); await page.waitForTimeout(70); await page.keyboard.up('Space');
await page.waitForTimeout(600);
ok((await state()).includes('menu'), 'duplo toque pela barra de espaço abre o menu');
// autorepeat do teclado não gera vários toques: segura espaço 1,2 s -> seleciona (Rosto)
await page.keyboard.down('Space');
for (let i = 0; i < 20; i++) { await page.waitForTimeout(60); await page.keyboard.down('Space'); }  // autorrepetição (repeat=true)
await page.keyboard.up('Space');
await page.waitForTimeout(500);
ok((await state()).includes('face'), 'segurar espaço (com autorrepetição) é um único gesto');
// botão solto fora / perda de foco não prende o sensor
await page.mouse.move(box.x + 10, box.y + 10); await page.mouse.down();
await page.mouse.move(5, 5); await page.mouse.up();
await page.waitForTimeout(200);
ok(!(await page.$eval('#head', e => e.classList.contains('on'))), 'soltar fora da cabeça libera o sensor virtual');
await page.screenshot({ path: `${out}/03_rosto.png` });
// Portal (já configurado) mostra abas
await page.click('.tabs button[data-p=portal]');
await page.waitForTimeout(1500);
const frame = page.frames().find(f => f.url().includes('portal.html'));
await frame.evaluate(() => location.reload());
await page.waitForTimeout(1500);
ok(await fr.locator('#appView').isVisible(), 'portal abre as abas após o primeiro uso');
ok((await fr.locator('#hName').textContent()) === 'Júlia', 'apelido com acento chega ao portal');
await fr.locator('button[data-tabbtn="Coleção"]').click();
await page.waitForTimeout(500);
await page.screenshot({ path: `${out}/04_colecao.png` });
await fr.locator('button[data-tabbtn="Foco e tempo"]').click();
await page.waitForTimeout(300);
await page.screenshot({ path: `${out}/05_foco.png` });
// Nenhum recurso externo carregado
const ext = await page.evaluate(() => performance.getEntriesByType('resource').map(r => r.name).filter(n => /^https?:/.test(n)));
const extPortal = await frame.evaluate(() => performance.getEntriesByType('resource').map(r => r.name).filter(n => /^https?:/.test(n)));
ok(ext.length === 0 && extPortal.length === 0, 'nenhum recurso externo (simulador e portal)');
ok(errors.length === 0, 'sem erros de JavaScript' + (errors.length ? ': ' + errors.join(' | ') : ''));
// ── Fase 2: portal servido por HTTP (caminho fetch + X-Tuba-Token, como no ESP32) ──
const { spawn } = await import('node:child_process');
for (const mode of ['ap', 'home']) {
  const srv = spawn(process.execPath, ['tools/portal_server.mjs', mode === 'ap' ? '18731' : '18732', ...(mode === 'home' ? ['--home'] : [])]);
  const pin = await new Promise(res => { let buf = ''; srv.stdout.on('data', d => { buf += d; const m = buf.match(/PIN (\d{6})/); if (buf.includes('http://')) res(m ? m[1] : null); }); });
  const p2 = await browser.newPage({ viewport: { width: 420, height: 860 } });
  p2.on('pageerror', e => errors.push(String(e)));
  if (mode === 'home') p2.once('dialog', () => {});
  await p2.goto(`http://localhost:${mode === 'ap' ? 18731 : 18732}/`);
  if (mode === 'home') {
    await p2.locator('#dlg').waitFor({ state: 'visible', timeout: 5000 });
    await p2.locator('#dlgI').fill('000000'); await p2.locator('#dlgOk').click();
    await p2.waitForTimeout(500);
    ok(await p2.locator('#appView').isHidden() && await p2.locator('#wizView').isHidden(), 'rede de casa: PIN errado não dá acesso');
    await p2.reload();
    await p2.locator('#dlg').waitFor({ state: 'visible', timeout: 5000 });
    await p2.locator('#dlgI').fill(pin); await p2.locator('#dlgOk').click();
  }
  await p2.locator('#wNick').waitFor({ timeout: 5000 });
  await p2.locator('#wNick').fill('Nome comprido demais aqui');
  await p2.locator('[data-step="0"] [data-next]').click();
  await p2.waitForTimeout(300);
  ok((await p2.locator('#we_nickname').textContent()).includes('12'), `${mode}: apelido longo é recusado pelo núcleo`);
  await p2.locator('#wNick').fill('Tubarão');
  await p2.locator('[data-step="0"] [data-next]').click(); await p2.waitForTimeout(300);
  await p2.locator('[data-step="1"] [data-next]').click(); await p2.waitForTimeout(300);
  await p2.locator('#wDone').click(); await p2.waitForTimeout(500);
  ok(await p2.locator('#appView').isVisible(), `${mode}: assistente concluído via HTTP`);
  ok((await p2.locator('#hName').textContent()) === 'Tubarão', `${mode}: apelido salvo e relido`);
  if (mode === 'ap') await p2.screenshot({ path: `${out}/06_portal_celular.png`, fullPage: false });
  await p2.close(); srv.kill();
}
await browser.close();
console.log(fails ? `\n${fails} falha(s)` : '\nE2E ok');
process.exit(fails ? 1 : 0);
