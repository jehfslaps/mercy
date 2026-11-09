// MERCY motion recorder: plays a timeline of input events at a fixed 30 fps and writes every frame (raw RGB)
#include "Application.hpp"
#include "StandaloneWindow.hpp"
#include "NanoVG.hpp"
#include "MercyView.hpp"
#include <GL/gl.h>
#include "nanovg/nanovg.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <map>
START_NAMESPACE_DGL
struct Host : mercy::ViewHost {
  void hostBeginEdit(uint32_t) override {} void hostEndEdit(uint32_t) override {}
  void hostSetParam(uint32_t, float) override {} void hostSetOrder(const char*) override {}
  void hostRepaint() override {} void hostResize(unsigned, unsigned) override {}
};
struct Ev { int f; std::string cmd; };
class Rec : public NanoStandaloneWindow, public IdleCallback {
public:
  Application& appRef; Host hst; mercy::Session sess; mercy::MercyView view; FILE* out; int frame=0, total; float sc; unsigned t=1000;
  std::vector<Ev> evs; size_t ei=0;
  Rec(Application& app, int w, int h, float s, bool dark, FILE* o, int tot) : NanoStandaloneWindow(app), appRef(app), sess(), view(&hst, (sess.dark = dark, &sess)), out(o), total(tot), sc(s) { setSize(w,h); setResizable(false); }
  void idleCallback() override { repaint(); }
  void run(const std::string& cmd){
    char sym[64]; float a,b,c3;
    if(sscanf(cmd.c_str()," c %f %f",&a,&b)==2){ view.mouse(1,true,a*sc,b*sc,0,t); view.mouse(1,false,a*sc,b*sc,0,t+60); t+=1000; }
    else if(sscanf(cmd.c_str()," C %f %f",&a,&b)==2){ for(int k=0;k<2;k++){ view.mouse(1,true,a*sc,b*sc,0,t); view.mouse(1,false,a*sc,b*sc,0,t+40); t+=120; } t+=1000; }
    else if(sscanf(cmd.c_str()," m %f %f",&a,&b)==2) view.motion(a*sc,b*sc,0);
    else if(sscanf(cmd.c_str()," D %f %f",&a,&b)==2){ view.mouse(1,true,a*sc,b*sc,0,t); t+=50; }
    else if(sscanf(cmd.c_str()," U %f %f",&a,&b)==2){ view.mouse(1,false,a*sc,b*sc,0,t); t+=1000; }
    else if(sscanf(cmd.c_str()," s %f %f %f",&a,&b,&c3)==3){ view.scroll(a*sc,b*sc,c3>0?1.f:-1.f,0); }
    else if(sscanf(cmd.c_str()," k %f",&a)==1){ view.key((unsigned)a,true); view.key((unsigned)a,false); }
    else if(cmd.find("N ")!=std::string::npos){ std::string nm=cmd.substr(cmd.find('N')+2); for(int i=0;i<mercy::kNumFactory;i++) if(nm==mercy::kFactory[i].name) view.loadPreset(i); }
    else if(sscanf(cmd.c_str()," p %63s %f",sym,&a)==2){ for(uint32_t p=0;p<mercy::kNumParams;++p) if(!strcmp(mercy::kParamInfo[p].symbol,sym)) view.paramChanged(p,a); }
  }
  void onNanoDisplay() override {
    while(ei<evs.size() && evs[ei].f<=frame){ run(evs[ei].cmd); ++ei; }
    view.idle();
    view.draw(*this, getWidth(), getHeight());
    nvgEndFrame(getContext()); glFinish();
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp); const int W=vp[2], H=vp[3];
    std::vector<unsigned char> px((size_t)W*H*3); glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,W,H,GL_RGB,GL_UNSIGNED_BYTE,px.data());
    for(int y=H-1;y>=0;y--) fwrite(&px[(size_t)y*W*3],1,(size_t)W*3,out);
    nvgBeginFrame(getContext(), getWidth(), getHeight(), 1.0f);
    if(++frame>=total){ fflush(out); appRef.quit(); } else repaint();
  }
};
END_NAMESPACE_DGL
using namespace DGL_NAMESPACE;
// usage: motrec out.rgb scale dark frames timeline.txt   (timeline lines: "<frame> <cmd>")
int main(int argc,char** argv){
  float sc=atof(argv[2]);
  char cmdl[512]; snprintf(cmdl,sizeof cmdl,"ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r 30 -i - -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p %s",(int)(960*sc),(int)(600*sc),argv[1]);
  FILE* o=popen(cmdl,"w"); bool dark=atoi(argv[3])!=0; int tot=atoi(argv[4]);
  Application app; Rec win(app,(int)(960*sc),(int)(600*sc),sc,dark,o,tot);
  win.view.mo.fixedDt = 1.0/30.0;
  FILE* tl=fopen(argv[5],"r"); char line[256];
  while(fgets(line,sizeof line,tl)){ int f; char rest[200]; if(sscanf(line,"%d %199[^\n]",&f,rest)==2) win.evs.push_back({f,rest}); }
  fclose(tl);
  const int N=mercy::Scope::N; unsigned r=7;
  const float hits[][3]={{.01f,.92f,.06f},{.14f,.25f,.02f},{.26f,.82f,.07f},{.39f,.28f,.02f},{.5f,.95f,.05f},{.62f,.4f,.03f},{.76f,.85f,.08f},{.89f,.3f,.02f}};
  for(int i=0;i<N;i++){ float tt=(float)i/N, v=.01f; for(auto&h:hits){ if(tt>=h[0]) v+=h[1]*std::exp(-(tt-h[0])/h[2]); }
    r=r*1664525u+1013904223u; float j=1.f+((r>>8)&1023)/1023.f*.16f-.08f; win.sess.scope.out[i].store(std::min(v,1.f)*j*.40f); win.sess.scope.in[i].store(std::min(v,1.f)*j*.55f); }
  win.sess.scope.pos.store(N);
  app.addIdleCallback(&win); win.show(); app.exec(); pclose(o); return 0;
}
