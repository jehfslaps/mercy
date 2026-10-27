#include "Application.hpp"
#include "StandaloneWindow.hpp"
#include "NanoVG.hpp"
#include "../src/MercyView.hpp"
#include "../dpf/dgl/src/nanovg/nanovg.h"
#include <GL/gl.h>
#include <cstdio>
#include <unistd.h>
START_NAMESPACE_DGL
struct Host : mercy::ViewHost {
  std::string uiState;
  void hostBeginEdit(uint32_t) override {} void hostEndEdit(uint32_t) override {}
  void hostSetParam(uint32_t, float) override {} void hostSetOrder(const char*) override {}
  void hostRepaint() override {} void hostResize(unsigned, unsigned) override {}
  void hostSetUiState(const char* s) override { uiState = s; }
};
class ShotWin : public NanoStandaloneWindow, public IdleCallback {
public:
  Application& appRef; Host hst; mercy::MercyView view; const char* out; int frames = 0; bool grabbed = false;
  ShotWin(Application& app, const char* o, mercy::Session* s) : NanoStandaloneWindow(app), appRef(app), view(&hst, s), out(o) { setSize(960, 600); setResizable(false); }
  void idleCallback() override { repaint(); }
  void onNanoDisplay() override {
    view.draw(*this, getWidth(), getHeight());
    if (++frames >= 3 && !grabbed) {
      nvgEndFrame(getContext()); glFinish();
      std::vector<unsigned char> px(960 * 600 * 3); glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadPixels(0, 0, 960, 600, GL_RGB, GL_UNSIGNED_BYTE, px.data());
      FILE* f = fopen(out, "wb"); fprintf(f, "P6 960 600 255\n"); for (int y = 599; y >= 0; y--) fwrite(&px[y * 960 * 3], 1, 960 * 3, f); fclose(f);
      grabbed = true; nvgBeginFrame(getContext(), getWidth(), getHeight(), 1.0f); appRef.quit();
    } else if (!grabbed) repaint();
  }
};
END_NAMESPACE_DGL
using namespace DGL_NAMESPACE;
int main(int argc, char** argv) {
  const int mode = atoi(argv[1]);
  Application app;
  static mercy::Session sess;            // lives in the plugin in the real thing
  std::string saved;
  {
    Host h; mercy::MercyView v(&h, &sess);   // first time the window is open
    auto click = [&](float x, float y, unsigned t) { v.mouse(1, true, x, y, 0, t); v.mouse(1, false, x, y, 0, t + 5); };
    click(150, 557, 1000); click(128 + 244 + 40, 96 + 80 + 22 * 8, 1100);   // preset browser -> pick a preset (right column, row 8)
    click(900, 18, 1500); click(482, 66, 2000);                   // advanced
    click(780, 560, 3000);                  // bake
    click(325, 128, 4000);                  // bulk
    v.filesDropped({"/tmp/claude-0/-home-claude/104cb674-3710-5301-a490-2354e17c6d6b/scratchpad/wav/kick loop 92.wav", "/tmp/claude-0/-home-claude/104cb674-3710-5301-a490-2354e17c6d6b/scratchpad/wav/snare break.wav", "/tmp/claude-0/-home-claude/104cb674-3710-5301-a490-2354e17c6d6b/scratchpad/wav/hats 130.wav"});
    click(318, 386, 5000);                  // bake all
    for (int i = 0; i < 200; ++i) { v.idle(); usleep(20000); }
    click(800, 300, 6000);                  // a click elsewhere inside the plugin (used to close the panel)
    click(30, 300, 6100);                   // a click on the side bar, outside the panel
    v.idle();
    saved = h.uiState;
    printf("saved ui state: %s\n", saved.substr(0, 120).c_str());
  }                                          // window closed: view destroyed
  if (mode == 0) { ShotWin w(app, "reopen.ppm", &sess); app.addIdleCallback(&w); w.show(); app.exec(); }
  if (mode == 1) {                           // project reload: brand-new plugin instance, state comes from the project
    static mercy::Session fresh; fresh.savedUi = saved;
    ShotWin w(app, "reload.ppm", &fresh); app.addIdleCallback(&w); w.show(); app.exec();
  }
}
