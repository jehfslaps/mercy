# slapworx: how Mercy 1.0 was released (playbook for the next plugin)

Written 2026-11-14 from the Mercy release. Source for everything below: the public repo jehfslaps/mercy (branch main). Copy files from it instead of rewriting them.

## 0. Working with Jeff (the owner)
- Nothing gets built until he says "Fandango". "Use your best judgment" or a direct instruction also counts as a go.
- He's easily confused: give big headings and one short step at a time, and walk him through screens.
- Never type passwords, API keys, tokens, Apple credentials or private signing keys into anything, even with his permission. He adds GitHub secrets and types passwords himself.
- **Naming (firm):** in all text (installers, file names, manual, readme, Gumroad, website, emails) write the plugin name normally: "Mercy", capital first letter. A stylized lowercase "mercy." is only for graphics and visuals (logo, cover, ad, the plugin's own artwork).
- **Pricing words:** ask Jeff for every plugin. Mercy is "$0 intro price for a limited time, then $29.99" and must never be called "free". Dewitt is plainly free.
- Vendor name: "slapworx" (lowercase). Website: slapworx.net. Public version numbers start at 1.0, and internal test numbers never appear publicly. Prices in USD. No copy protection, no AAX.

## 1. The finished product (what Mercy shipped)
- Gumroad: $0 intro price (limited time, then $29.99), Discover OFF. Two downloads:
  - Mercy_v1.0_Windows.zip: Install Mercy.exe (NSIS), "Windows warning - how to install.png", Mercy_User_Manual.pdf, README.txt, licenses/, "manual install/" (plain VST3 + CLAP), source/
  - Mercy_v1.0_Mac.zip: Install Mercy.pkg (signed + notarized + stapled), the manual, README.txt, licenses/, "manual install/" (signed AU/VST3/CLAP bundles), source/
- Formats: Windows 10/11 64-bit VST3 + CLAP. Mac: universal (Apple Silicon + Intel), macOS 10.15+, AU + VST3 + CLAP.
- Website /mercy/ page links to the Gumroad page. Downloads are never hosted on the site itself.

## 2. GitHub + builds (no Mac needed)
- GitHub Actions builds everything: .github/workflows/build.yml in jehfslaps/mercy.
  - windows-build: mingw on Ubuntu (plus nsis from apt), then makes the installer.
  - mac: macos-15 runner (arm64). Builds arm64 and x86_64 separately (CFLAGS/CXXFLAGS/LDFLAGS = "-arch <arch> -mmacosx-version-min=10.15", separate BUILD_DIR_SUFFIX), joins them with tools/mac_universal.sh (lipo), checks both arches + minos 10.15, then signs, notarizes and packages. distribution.xml: hostArchitectures="arm64,x86_64", os-version min 10.15.
  - Intel is tested on the same runner under Rosetta: `arch -x86_64` for the sound-check render (must match Windows bit-for-bit), auval and pluginval.
  - windows-check: real Windows render + pluginval. compare: Windows vs Mac bit-identical renders, plus a screenshot look-compare (that one is often red; harmless).
- **Billing:** Jeff's GitHub billing is broken, so private repos get no Actions minutes until about Dec 1. Mercy's repo was made PUBLIC to get free minutes. Do the same (or a public mirror repo, like jehfslaps/dewitt-airliner).
- **Downloads stay private:** a "hide-downloads" job runs first and keeps the "latest-build" release as a DRAFT (only the owner sees it), so the public repo doesn't hand out the installers. The workspace downloads assets with: gh api -H "Accept: application/octet-stream" repos/jehfslaps/<repo>/releases/assets/<id>
- Local builds in the cloud workspace: Linux `make`; Windows via mingw (see BUILD.txt / package.sh in the mercy repo).

## 3. Mac signing + notarizing
- One Apple Developer account (individual, "JEFFREY LOGAN ORSAT", Team ID YCD5K76V6N, valid through Nov 2027). The same Developer ID certificates work for EVERY slapworx plugin. Only the GitHub secrets are per repo (or put them once on a free "slapworx" GitHub organization).
- Mercy's .p12 files and password were deleted after setup and can't be read back from GitHub. **The next plugin needs fresh certificates:**
  1. Claude makes one RSA key and two CSRs (Developer ID Application + Developer ID Installer, G2 Sub-CA).
  2. Jeff uploads them at developer.apple.com > Certificates > + and downloads the two .cer files.
  3. Claude builds the .p12 files (+ password + base64 text) and sends them.
  4. **This time Jeff saves the .p12 and password in a password manager**, then adds the repo secrets himself.
- Mercy's secrets: MAC_APP_P12_BASE64, MAC_INSTALLER_P12_BASE64, MAC_P12_PASSWORD, APPLE_ID, APPLE_APP_PASSWORD (app-specific password from appleid.apple.com), APPLE_TEAM_ID.
- In CI: import into a temporary keychain; `codesign --options runtime --timestamp` each bundle; `productbuild --sign "Developer ID Installer: ..."` for the pkg; `xcrun notarytool submit --wait --timeout 45m`; then `xcrun stapler staple` on the pkg AND on the bundles in "manual install". Missing secrets fall back to ad-hoc signing so builds never break.
- With signing, macOS opens the installer with no warning. Don't put "right-click > Open" or xattr steps in the manual.

## 4. Installers
- **Mac (installer/mac/ + tools/package.sh):**
  - One component pkg per format (pkgbuild, BundleIsRelocatable = false so it can't install into a stray copy), combined with productbuild + distribution.xml (with choices per format). Installs to /Library/Audio/Plug-Ins/{Components,VST3,CLAP}.
  - postinstall: removes a hand-installed copy in ~/Library/Audio/Plug-Ins and runs `killall AudioComponentRegistrar` so Logic sees the new AU.
  - welcome.html / conclusion.html MUST start with `<meta charset="utf-8">` (and use &rsaquo; etc.), or characters like "›" show up garbled ("â€º").
- **Windows (installer/mercy.nsi):**
  - NSIS 3, `Target amd64-unicode` (64-bit, so it also runs under wine64 for testing), MUI2, RequestExecutionLevel admin.
  - VST3 to $COMMONFILES64\VST3 (replace the whole bundle folder), CLAP to $COMMONFILES64\CLAP, manual/readme/licenses to $PROGRAMFILES64\slapworx\<Name>, Start-menu shortcuts, an uninstaller + registry Uninstall key.
  - VIProductVersion must be numeric (1.0.0.0): package.sh computes NUMVER.
  - Images: mercy.ico, side.bmp (164x314 welcome panel), header.bmp (150x57).
- **The Windows warning picture:** docs/smartscreen/ (guide.html + render.js) renders "Windows warning - how to install.png" (More info > Run anyway). Put it in the Windows zip and mention it in the manual and README. (Free option for later: submit the .exe to Microsoft's file-submission portal so the warning shows less.)

## 5. Manual
- docs/manual/manual.html + render.js (Playwright) renders the PDF (16 pages for Mercy). Same fonts/colors as the plugin. Install section written for the installers. Copy the structure.

## 6. Bugs we hit (check every new plugin for these)
1. **Retina Mac: window 4x too big, clicks miss.** DPF's automatic scaling scales twice. Use `UI(w, h, false)`, size in real pixels × getScaleFactor() with setGeometryConstraints(..., false), keep the user's size in uiScaleFactorChanged, and draw relative to getWidth()/getHeight(). Full write-up in project doc dpf/RETINA-RULE.md. Test with DPF_SCALE_FACTOR=2 on Linux (test/embedtest.c: the window must be 2x, not 4x, and a click on the advanced tab must open it). A rented cloud Mac usually runs at 1x and won't show it.
2. **Presets show "*" / bakes named "...edit" on Mac (FL Studio VST3).** The host echoes our own parameter values back with float32 rounding, and the UI treated that as an edit. Fix: remember each value the UI sends (sentValue[p]); when the host echoes something within 1e-4 of the range of it (0.25 for int/bool params), keep the exact value and return. Real changes clear it. Test: test/echotest.cpp (loads every preset, sends VST3-rounded echoes).
3. **Windows installer `VIProductVersion`** rejects text like "2.1-test4". Use numeric only.
4. **Bitwig (Mac, CLAP): bypass turns on but never turns back off.** Customer report, 2026-11-17. A parameter with DPF's `kParameterDesignationBypass` becomes `CLAP_PARAM_IS_BYPASS`, and Bitwig ties it to its device on/off button: turning on the plugin's own bypass switched the device off, Bitwig stopped calling process(), and DPF only hands UI clicks to the host inside process(), so the "off" click never arrived. Fix: make the plugin's bypass a plain boolean parameter (no designation), keeping the same name, symbol (`dpf_bypass`) and range so saved projects still load. Test: test/hosttest.cpp (params, bypass and ui-flush checks). The full stability setup is section 6b.

## 6b. Stability: rules + automatic host tests (every plugin, every release)
Nobody can test every DAW by hand. Instead, every build runs tests that act like picky DAWs, and a build that fails one makes no zips. Copy all of this into each new plugin's repo.

**Rules (things that caused or commonly cause host bugs)**
1. The plugin's bypass is a plain on/off parameter. Never `kParameterDesignationBypass` (Bitwig ties it to its device power button; see bug 4).
2. UI clicks must reach the host even when it isn't calling process(): patches/dpf/dpf-clap-host-fixes.patch makes DPF's CLAP wrapper call `request_flush()` and deliver queued UI changes in `params.flush()`.
3. Latency is set in the constructor and in activate()/sampleRateChanged() only, never while playing. The same DPF patch reports it inside activate(), as CLAP requires.
4. When the UI sends a value, remember it and accept the host's rounded echo (bug 2, test/echotest.cpp).
5. Retina: `UI(w, h, false)` and sizes × getScaleFactor() (bug 1, dpf/RETINA-RULE.md).
6. Keep DPF fixes as patch files in patches/dpf and patches/pugl (src/Makefile applies them). Never commit inside the dpf submodule.

**Automatic tests in CI (.github/workflows/build.yml)**
- **test/hosttest.cpp** (Linux CLAP, under xvfb), 41 checks: parameter sanity and no host-bypass flag; bypass on/off three times with processing stopped in between; a click in the window while the host is NOT processing still arrives (request_flush + flush); events for another namespace are ignored; the same sound at buffer sizes 1/7/64/512/4096; 44.1 to 192 kHz; silence, +12 dBFS noise and DC; random settings saved and reloaded into a new instance give the same values and the same sound; a project saved by an earlier version (test/fixtures/mercy-1.0.state + .values) loads intact; three copies at once sound like one; 12 activate/deactivate cycles with resets, latency reported only inside activate; the window opens and closes 10 times and is exactly 2x at 2x scale. Proven: the build before the Bitwig fix fails 6 of these.
- **clap-validator** (free-audio, pinned commit, cached), on the Linux CLAP and on the universal Mac CLAP. `param-set-wrong-namespace` is skipped because it also compares read-only meter values (a false alarm); hosttest covers it. Before the DPF patch Mercy failed 23 of 44; now 0.
- **test/echotest.cpp**: FL Studio-style rounded echoes on every factory preset.
- Already there: pluginval (VST3 Windows + Mac, AU Mac, Intel half too), auval (Apple Silicon + Intel), 233 sound scenarios bit-identical across Windows, Mac and Mac Intel.
- New plugin: copy hosttest.cpp (change the bypass click position and the fixture name), make a fixture with `hosttest <plugin.clap> --make-fixture test/fixtures`, and keep old fixtures forever so old projects keep loading.
- Every new bug report becomes a check in hosttest before it's fixed.

**Real DAWs before a big release:** FL Studio on Windows (Jeff). On a rented Mac for a week: Logic, Ableton Live and Bitwig (all have free trials). If possible, 2 to 4 beta testers on other DAWs with a 10-step checklist.

## 7. Testing on a real Mac (no Mac at home)
- Rentamac.io: a dedicated Mac mini M4, full admin rights, Texas location. Remote screen via the free DeskIn app. About $59/week (the daily plan was sold out). **Cancel the subscription right after buying**: the Mac stays until the week ends, then it's wiped. Never Shut Down the Mac, never click Wipe. (Scaleway was cheaper but sold out; MacinCloud pay-as-you-go has no admin, so installers won't run.)
- What was tested: no Gatekeeper warning, installer, FL Studio finds it (VST3, CLAP, AU), window size, clicks, every control/module/preset, bake + capture + export, save/reopen project.
- Cross-platform sound check: same FL Studio template on both ("Minimal > Basic with limiter": Basic 808 Kick/Snare), bake the same presets, compare files (md5 + null test). Mercy was bit-identical on Mac and Windows.
- DeskIn's "File Transfer" mode copies files between the Mac and the PC.

## 8. Gumroad + website
- Gumroad page: name written normally ("Mercy"), price wording as Jeff decides (see section 0), suggested amount left blank, summary line, description (what it is, features, what you get, system requirements, install note incl. the Windows warning, "Made by slapworx: plug-ins + samples. that slap. slapworx.net"), Discover off. Upload both zips, publish, then test a $0 download logged out and open both zips.
- Images: cover 2560x1440 and thumbnail 1200x1200, made as HTML in the plugin's look and rendered with Playwright (docs/gumroad/ in the mercy repo). Use a FRESH screenshot of the current version (an old one said "v2.0").
- If the plugin references a real artist, add a "not affiliated with ..." line to the Gumroad page, readme, website and ads.
- Then give the website chat a handoff: name, tagline, version, formats/requirements, features, colors/fonts, images, the Gumroad link, the disclaimer, and "downloads only through Gumroad".
