#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "clap_all.h"
#include "clap/ext/timer-support.h"
#include <unistd.h>
static const void* get_ext(const clap_host_t* h, const char* id){ return NULL; }
static void noop(const clap_host_t* h){}
static clap_host_t host = { CLAP_VERSION_INIT, NULL, "test", "test", "", "1", get_ext, noop, noop, noop };
static uint32_t in_size(const clap_input_events_t* l){ return 0; }
static const clap_event_header_t* in_get(const clap_input_events_t* l, uint32_t i){ return NULL; }
static clap_input_events_t in_ev = { NULL, in_size, in_get };
static bool out_push(const clap_output_events_t* l, const clap_event_header_t* e){ return true; }
static clap_output_events_t out_ev = { NULL, out_push };
int main(int argc, char** argv){
  void* lib = dlopen(argv[1], RTLD_NOW); if(!lib){ printf("dlopen fail %s\n", dlerror()); return 1; }
  const clap_plugin_entry_t* e = dlsym(lib, "clap_entry"); e->init(argv[1]);
  const clap_plugin_factory_t* f = e->get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_descriptor_t* d = f->get_plugin_descriptor(f, 0);
  printf("plugin: %s id=%s vendor=%s ver=%s\n", d->name, d->id, d->vendor, d->version);
  for(int i=0; d->features[i]; i++) printf(" feature: %s\n", d->features[i]);
  const clap_plugin_t* p = f->create_plugin(f, &host, d->id);
  if(!p->init(p)){ printf("init failed\n"); return 1; }
  const clap_plugin_params_t* pr = p->get_extension(p, CLAP_EXT_PARAMS);
  uint32_t n = pr->count(p); printf("params: %u\n", n);
  int shown=0;
  for(uint32_t i=0;i<n;i++){ clap_param_info_t inf; pr->get_info(p,i,&inf); char txt[64]="";
    pr->value_to_text(p,inf.id,inf.default_value,txt,64);
    if(i<4 || i==8 || (i>=43 && i<=51) || i>=n-3) printf("  [%u] %-22s min %.2f max %.2f def %.2f -> '%s'%s%s\n", i, inf.name, inf.min_value, inf.max_value, inf.default_value, txt,
      (inf.flags & CLAP_PARAM_IS_BYPASS)?" BYPASS":"", (inf.flags & CLAP_PARAM_IS_READONLY)?" READONLY":""); }
  const clap_plugin_audio_ports_t* ap = p->get_extension(p, CLAP_EXT_AUDIO_PORTS);
  clap_audio_port_info_t pi; ap->get(p,0,true,&pi); printf("audio in: %u ch\n", pi.channel_count);
  printf("gui ext: %s, state ext: %s, latency ext: %s\n", p->get_extension(p, CLAP_EXT_GUI)?"yes":"no", p->get_extension(p, CLAP_EXT_STATE)?"yes":"no", p->get_extension(p, CLAP_EXT_LATENCY)?"yes":"no");
  if(!p->activate(p, 48000, 32, 512)){ printf("activate failed\n"); return 1; }
  { const clap_plugin_latency_t* lt = p->get_extension(p, CLAP_EXT_LATENCY); if(lt) printf("reported latency samples @48k: %u\n", lt->get(p)); }
  p->start_processing(p);
  enum { N=512 }; float L[N],R[N],oL[N],oR[N]; float* ins[2]={L,R}; float* outs[2]={oL,oR};
  clap_audio_buffer_t ib={ins,NULL,2,0,0}, ob={outs,NULL,2,0,0};
  double peakIn=0, peakOut=0; int bad=0; unsigned seed=1;
  for(int blk=0; blk<400; blk++){
    for(int i=0;i<N;i++){ int t=blk*N+i; double env=exp(-(t%24000)/2400.0); seed=seed*1103515245+12345; double nz=((seed>>16)&0x7fff)/16384.0-1.0;
      L[i]=(float)(0.5*env*sin(2*M_PI*55*t/48000.0)+0.1*env*nz); R[i]=L[i]*0.9f; if(fabs(L[i])>peakIn) peakIn=fabs(L[i]); }
    clap_process_t pc; memset(&pc,0,sizeof pc); pc.steady_time=blk*N; pc.frames_count=N; pc.audio_inputs=&ib; pc.audio_outputs=&ob; pc.audio_inputs_count=1; pc.audio_outputs_count=1; pc.in_events=&in_ev; pc.out_events=&out_ev;
    p->process(p,&pc);
    for(int i=0;i<N;i++){ if(!isfinite(oL[i])||!isfinite(oR[i])) bad++; if(fabs(oL[i])>peakOut) peakOut=fabs(oL[i]); }
  }
  double gr; pr->get_value(p, n-1, &gr); double mo; pr->get_value(p, n-2, &mo);
  printf("processed 400 blocks: peak in %.1f dB, peak out %.1f dB, bad samples %d, GR meter %.1f dB, out meter %.1f dB\n", 20*log10(peakIn), 20*log10(peakOut), bad, gr, mo);
  // state save/load round trip
  const clap_plugin_state_t* st = p->get_extension(p, CLAP_EXT_STATE);
  const clap_plugin_gui_t* gui = p->get_extension(p, CLAP_EXT_GUI);
  if(getenv("DISPLAY") && gui){
    bool ok = gui->is_api_supported(p, CLAP_WINDOW_API_X11, true);
    printf("gui x11 floating supported: %d\n", ok);
    if(ok && gui->create(p, CLAP_WINDOW_API_X11, true)){
      uint32_t w=0,h=0; gui->get_size(p,&w,&h); printf("gui size %ux%u, resizable %d\n", w,h, gui->can_resize(p));
      gui->show(p);
      for(int i=0;i<30;i++){ const clap_plugin_timer_support_t* t = p->get_extension(p, CLAP_EXT_TIMER_SUPPORT); (void)t; usleep(20000);} 
      gui->hide(p); gui->destroy(p); printf("gui open/close OK\n");
    }
  }
  p->stop_processing(p); p->deactivate(p); p->destroy(p); e->deinit();
  printf("clean shutdown OK\n");
  return 0;
}
