const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch(); const p = await b.newPage();
  await p.goto('file://' + process.cwd() + '/manual.html', { waitUntil: 'networkidle' });
  await p.evaluate(() => document.fonts.ready);
  await p.pdf({ path: 'MERCY_User_Manual.pdf', format: 'A4', printBackground: true, preferCSSPageSize: true });
  await b.close();
})();
