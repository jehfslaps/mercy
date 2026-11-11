// embed Mercy.clap's GUI in an X11 parent window (like a DAW), click a point, screenshot
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include "clap_all.h"
#include "clap/ext/timer-support.h"
static clap_id tid=0; static uint32_t tms=0; static int havetimer=0;
static bool reg_timer(const clap_host_t* h, uint32_t ms, clap_id* id){ *id=++tid; tms=ms; havetimer=1; return true; }
static bool unreg_timer(const clap_host_t* h, clap_id id){ havetimer=0; return true; }
static clap_host_timer_support_t ts={reg_timer,unreg_timer};
static bool req_resize(const clap_host_t* h, uint32_t w, uint32_t hh){ printf("host: plugin asks resize %ux%u\n", w, hh); return true; }
static void noop_g(const clap_host_t* h){}
static void req_show(const clap_host_t* h){}
static bool req_show_b(const clap_host_t* h){return true;}
static void closed(const clap_host_t* h, bool b){}
static clap_host_gui_t hg={noop_g, req_resize, req_show_b, req_show_b, closed};
static const void* get_ext(const clap_host_t* h, const char* id){ if(!strcmp(id,CLAP_EXT_TIMER_SUPPORT)) return &ts; if(!strcmp(id,CLAP_EXT_GUI)) return &hg; return NULL; }
static void noop(const clap_host_t* h){}
static clap_host_t host = { CLAP_VERSION_INIT, NULL, "test", "test", "", "1", get_ext, noop, noop, noop };
int main(int argc, char** argv){
  void* lib = dlopen(argv[1], RTLD_NOW); if(!lib){ printf("dlopen fail %s\n", dlerror()); return 1; }
  const clap_plugin_entry_t* e = dlsym(lib, "clap_entry"); e->init(argv[1]);
  const clap_plugin_factory_t* f = e->get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_descriptor_t* d = f->get_plugin_descriptor(f, 0);
  const clap_plugin_t* p = f->create_plugin(f, &host, d->id); p->init(p);
  const clap_plugin_gui_t* gui = p->get_extension(p, CLAP_EXT_GUI);
  const clap_plugin_timer_support_t* pt = p->get_extension(p, CLAP_EXT_TIMER_SUPPORT);
  Display* dpy = XOpenDisplay(NULL);
  Window par = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 2400, 1500, 0, 0, 0x202020);
  XMapWindow(dpy, par); XSync(dpy, False);
  if(!gui->create(p, CLAP_WINDOW_API_X11, false)){ printf("create failed\n"); return 1; }
  double sc = argc>2 ? atof(argv[2]) : 1.0;
  if (sc != 1.0) printf("set_scale(%.2f) -> %d\n", sc, gui->set_scale(p, sc));
  uint32_t w=0,h=0; gui->get_size(p,&w,&h); printf("get_size after create: %ux%u\n", w,h);
  clap_window_t cw = { CLAP_WINDOW_API_X11 }; cw.x11 = par;
  gui->set_parent(p, &cw); gui->show(p);
  for(int i=0;i<60;i++){ if(havetimer) pt->on_timer(p,tid); while(XPending(dpy)){XEvent ev; XNextEvent(dpy,&ev);} usleep(16000); }
  gui->get_size(p,&w,&h); printf("get_size after show: %ux%u\n", w,h);
  Window r, pa, *ch; unsigned nch; XQueryTree(dpy, par, &r, &pa, &ch, &nch);
  for(unsigned i=0;i<nch;i++){ XWindowAttributes a; XGetWindowAttributes(dpy, ch[i], &a); printf("child window: %dx%d at %d,%d\n", a.width, a.height, a.x, a.y); }
  // click the "advanced" tab: design (480,60) -> window coords at the expected scale
  int cx = (int)(480*sc), cy=(int)(60*sc);
  XTestFakeMotionEvent(dpy, -1, cx, cy, 0); XTestFakeButtonEvent(dpy,1,True,0); XTestFakeButtonEvent(dpy,1,False,30); XSync(dpy,False);
  for(int i=0;i<40;i++){ if(havetimer) pt->on_timer(p,tid); while(XPending(dpy)){XEvent ev; XNextEvent(dpy,&ev);} usleep(16000); }
  XSync(dpy,False); system("import -window root embed.png"); printf("screenshot embed.png\n");
  gui->hide(p); gui->destroy(p); p->destroy(p); return 0;
}
