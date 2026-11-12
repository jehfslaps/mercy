const { chromium } = require('playwright');
(async () => { const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 1600, height: 1000 }, deviceScaleFactor: 1 });
  await p.goto('file://' + process.cwd() + '/guide.html'); await p.evaluate(() => document.fonts.ready);
  await p.screenshot({ path: 'Windows warning - how to install.png' }); await b.close(); })();
