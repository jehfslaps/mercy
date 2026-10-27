# MERCY (slapworx)

Drum processor. Windows VST3; Mac VST3 + AU (Apple Silicon). Built with [DPF](https://github.com/DISTRHO/DPF) (submodule at `dpf/`).

- **Builds:** every push runs `.github/workflows/build.yml`. The run page has the release zips (`Mercy-Windows`, `Mercy-Mac`) plus the check results.
- **Checks on every build:**
  - Windows vs Mac sound must be **bit-identical** (85 test renders).
  - Mac screenshots are compared with the reference screenshots.
  - `auval` (Logic's check) and `pluginval` run on the plugins.
- **Source:** `src/` (plugin), `test/` (tests, sound check, screenshot harness), `tools/` (screenshots, packaging, compare).
- **Notes:** `BUILD.txt` (technical), `README.txt` (the user readme that ships in the zip).
