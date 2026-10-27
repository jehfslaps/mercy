#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "clap_all.h"
static const void* get_ext(const clap_host_t* h, const char* id){ return NULL; }
static void noop(const clap_host_t* h){}
static clap_host_t host = { CLAP_VERSION_INIT, NULL, "test", "test", "", "1", get_ext, noop, noop, noop };
static char buf[1<<20]; static size_t len = 0, rpos = 0;
static int64_t wr(const clap_ostream_t* s, const void* b, uint64_t n){ memcpy(buf+len,b,n); len+=n; return n; }
static int64_t rd(const clap_istream_t* s, void* b, uint64_t n){ uint64_t k = n < len-rpos ? n : len-rpos; memcpy(b,buf+rpos,k); rpos+=k; return k; }
int main(int argc, char** argv){
  void* lib = dlopen(argv[1], RTLD_NOW); const clap_plugin_entry_t* e = dlsym(lib, "clap_entry"); e->init(argv[1]);
  const clap_plugin_factory_t* f = e->get_factory(CLAP_PLUGIN_FACTORY_ID); const char* id = f->get_plugin_descriptor(f,0)->id;
  const clap_plugin_t* p = f->create_plugin(f, &host, id); p->init(p);
  const clap_plugin_state_t* st = p->get_extension(p, CLAP_EXT_STATE);
  clap_ostream_t os = { NULL, wr }; st->save(p, &os);
  printf("saved %zu bytes; contains key \"order\": %s, key \"ui\": %s\n", len, memmem(buf,len,"order",5)?"yes":"no", memmem(buf,len,"ui",2)?"yes":"no");
  const clap_plugin_t* p2 = f->create_plugin(f, &host, id); p2->init(p2);
  clap_istream_t is = { NULL, rd }; printf("load into fresh instance: %s\n", st->load(p2, &is) ? "ok" : "FAILED");
  p->destroy(p); p2->destroy(p2); e->deinit(); return 0;
}
