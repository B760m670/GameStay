// Рисует SVG-чертёж в PNG (Chromium через Playwright). Запуск: node art/concept/render_sheet.js <svg> <png>
const fs = require('fs');
const { chromium } = require('playwright');
(async () => {
  const [src, out] = process.argv.slice(2);
  const svg = fs.readFileSync(src, 'utf8');
  const w = +svg.match(/width="(\d+)"/)[1], h = +svg.match(/height="(\d+)"/)[1];
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: w, height: h } });
  await p.setContent(`<html><head><link href="https://fonts.googleapis.com/css2?family=Rubik:wght@400;700&display=swap" rel="stylesheet"></head><body style="margin:0">${svg}</body></html>`);
  await p.waitForTimeout(1500);
  await p.screenshot({ path: out });
  await b.close();
})();
