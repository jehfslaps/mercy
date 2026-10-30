#include "Application.hpp"
#include "StandaloneWindow.hpp"
#include "NanoVG.hpp"
#include "../src/MercyView.hpp"
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include "../dpf/dgl/src/nanovg/nanovg.h"
#include <functional>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>
START_NAMESPACE_DGL
struct Host : mercy::ViewHost {
  void hostBeginEdit(uint32_t) override {} void hostEndEdit(uint32_t) override {}
  void hostSetParam(uint32_t p, float v) override { if(log) fprintf(stderr,"set %s = %g\n", mercy::kParamInfo[p].symbol, v); }
  bool log=false; void hostSetOrder(const char*) override {}
  void hostRepaint() override {} void hostResize(unsigned, unsigned) override {}
};
class ShotWin : public NanoStandaloneWindow, public IdleCallback {
public:
  Application& appRef; Host hst; mercy::Session sess; mercy::MercyView view; const char* out; int frames=0; int W,H;
  ShotWin(Application& app, int w, int h, const char* o, bool dark) : NanoStandaloneWindow(app), appRef(app), sess(), view(&hst, (sess.dark = dark, &sess)), out(o), W(w), H(h) { setSize(w,h); setResizable(false);} 
  bool grabbed=false;
  void idleCallback() override { repaint(); }
  void onNanoDisplay() override {  view.draw(*this, getWidth(), getHeight()); if(++frames>=3 && !grabbed){ nvgEndFrame(getContext()); glFinish(); grab(); grabbed=true; nvgBeginFrame(getContext(), getWidth(), getHeight(), 1.0f); appRef.quit();} else if(!grabbed) repaint(); }
  // read the whole framebuffer (on a Retina Mac it is 2x the window size)
  void grab(){  GLint vp[4]={0,0,W,H}; glGetIntegerv(GL_VIEWPORT,vp); const int W=vp[2]>0?vp[2]:this->W, H=vp[3]>0?vp[3]:this->H;
    std::vector<unsigned char> px(W*H*3); glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,W,H,GL_RGB,GL_UNSIGNED_BYTE,px.data());
    FILE* f=fopen(out,"wb"); fprintf(f,"P6 %d %d 255\n",W,H); for(int y=H-1;y>=0;y--) fwrite(&px[y*W*3],1,W*3,f); fclose(f); }
};
END_NAMESPACE_DGL
using namespace DGL_NAMESPACE;
// a believable drum bar in the scope: kick, hats, snare, ghost, kick, snare
static void fillScope(mercy::Session& s){
  const float hits[][3]={{.01f,.92f,.06f},{.14f,.25f,.02f},{.26f,.82f,.07f},{.39f,.28f,.02f},{.5f,.95f,.05f},{.62f,.4f,.03f},{.76f,.85f,.08f},{.89f,.3f,.02f}};
  const int N=mercy::Scope::N; unsigned r=7;
  for(int i=0;i<N;i++){ float t=(float)i/N, v=.01f; for(auto&h:hits){ if(t>=h[0]) v+=h[1]*std::exp(-(t-h[0])/h[2]); }
    r=r*1664525u+1013904223u; float j=1.f+((r>>8)&1023)/1023.f*.16f-.08f; s.scope.out[i].store(std::min(v,1.f)*j*.40f); s.scope.in[i].store(std::min(v,1.f)*j*.55f); }
  s.scope.pos.store(N);
}
int main(int argc,char** argv){
  int mode = argc>1 ? atoi(argv[1]) : 0; const char* out = argc>2? argv[2] : "shot.ppm"; float sc = argc>3? atof(argv[3]):1.f; bool dark = argc>4 ? atoi(argv[4])!=0 : true;
  Application app; ShotWin win(app,(int)(960),(int)(600),out,dark);
  fillScope(win.sess);
  auto click=[&](float x,float y,unsigned t){ win.view.mouse(1,true,x,y,0,t); win.view.mouse(1,false,x,y,0,t+5); };
  win.view.paramChanged(mercy::pMeterGR, 4.2f); win.view.paramChanged(mercy::pMeterOut, -8.0f); win.view.paramChanged(mercy::pMeterIn, -12.4f);
  for(int i=0;i<5;i++) win.view.idle();
  switch(mode){
  case 1: click(482,66,1000); break;                                  // advanced
  case 2: win.view.motion(182,250,0); break;                    // hover squash
  case 3: click(350,66,1000); win.view.paramChanged(mercy::pMeterGR, 11.0f); break; // yeezus
  case 11: click(240,66,1000); break;                                 // throne
  case 4: click(150,557,1000); win.view.motion(200,(96+80+22*4),0); break; // preset popup
  case 5: click(482,66,1000); click(560,114,2000); win.view.motion(600,(100+40+50+22*2),0); break; // module popup
  case 6: click(482,66,1000); win.view.paramChanged(mercy::pTrEnv, 40.f); win.view.paramChanged(mercy::pClipCeiling, -3.f); break;
  case 8: click(482,66,1000); click(780,560,2000); break;             // bake panel
  case 9: { click(482,66,1000); click(780,560,2000); click(325,128,3000);
      const char* d="/tmp/claude-0/-home-claude/104cb674-3710-5301-a490-2354e17c6d6b/scratchpad/wav/";
      win.view.filesDropped({std::string(d)+"kick loop 92.wav",std::string(d)+"snare break.wav",std::string(d)+"hats 130.wav"});
      click(318,386,5000);
      for(int i=0;i<300;i++){ win.view.idle(); usleep(20000); }
      win.view.motion(700,(204+20),0); } break;
  case 19: { // folder drop -> bulk -> bake all -> export to desktop (needs SHOT_KIT = a folder of loops; HOME decides the desktop)
      click(482,66,1000); click(780,560,2000);
      const char* kit = getenv("SHOT_KIT");
      if (kit) win.view.filesDropped({std::string(kit)});
      click(318,386,5000);
      for(int i=0;i<300;i++){ win.view.idle(); usleep(20000); }
      click(560,446,7000);
      win.view.motion(600,446,0); } break;
  case 10: click(482,66,1000); click(780,560,2000); win.view.setDropHover(true); break;
  case 13: click(260,557,1000); for (const char* c="dark fantasy kick"; *c; ++c) win.view.character((unsigned)*c); break; // save -> type a name
  case 14: click(150,557,1000); for(int i=0;i<10;i++) win.view.scroll(300,300,-1,0); win.view.motion(200,462,0); break;       // browser, hover a user preset
  case 15: click(150,557,1000); for(int i=0;i<10;i++) win.view.scroll(300,300,-1,0); win.view.mouse(2,true,450,462,0,2000); win.view.motion(470,462+34,0); break; // right-click menu
  case 16: click(150,557,1000); for(int i=0;i<10;i++) win.view.scroll(300,300,-1,0); click(348,462,2000); win.view.motion(318,462,0); break; // x -> delete? yes/no
  case 18: click(150,557,1000); for(int i=0;i<10;i++) win.view.scroll(300,300,-1,0); win.view.mouse(2,true,450,462,0,2000); click(470,462+34,3000); break; // ctx -> rename
  case 17: click(482,66,1000); click(780,560,2000); break; // bake panel (single)
  case 12: win.view.paramChanged(mercy::pTrEnv, 40.f); win.view.paramChanged(mercy::pHpf, 80.f); win.view.paramChanged(mercy::pAutoGain, 0.f); click(400,557,1000); break;
  }
  app.addIdleCallback(&win);
  win.show();
  app.exec();
  return 0;
}
