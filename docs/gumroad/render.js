const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  for (const [f, w, h] of [['cover', 1280, 720], ['thumb', 600, 600]]) {
    const p = await b.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: 2 });
    await p.goto('file://' + __dirname + '/' + f + '.html'); await p.waitForTimeout(400);
    await p.screenshot({ path: __dirname + '/MERCY_gumroad_' + f + '.png' });
  }
  await b.close();
})();
