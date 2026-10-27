#include "../src/MercyDSP.hpp"
#include <cstdio>
#include <vector>
#include <random>
#include <chrono>
#include <string>
using namespace mercy;
static void writeWav(const std::string& fn, const std::vector<float>& L, const std::vector<float>& R, int sr){
  FILE* f=fopen(fn.c_str(),"wb"); int n=L.size(); int data=n*4;
  auto w32=[&](uint32_t v){fwrite(&v,4,1,f);}; auto w16=[&](uint16_t v){fwrite(&v,2,1,f);};
  fwrite("RIFF",1,4,f); w32(36+data); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(2); w32(sr); w32(sr*4); w16(4); w16(16);
  fwrite("data",1,4,f); w32(data);
  for(int i=0;i<n;i++){ for(float v: {L[i],R[i]}){ int s=(int)std::lround(clampf(v,-1,1)*32767); int16_t q=s; fwrite(&q,2,1,f);} } fclose(f);
}
int main(){
  const int sr=44100; const double bpm=92; const int beats=16; const int N=(int)(sr*60.0/bpm*beats);
  std::vector<float> L(N,0),R(N,0); std::mt19937 rng(1); std::normal_distribution<float> nd(0,1);
  auto addKick=[&](int at,float g){ double ph=0; for(int i=0;i<sr/2&&at+i<N;i++){ double t=i/(double)sr; double f=45+110*std::exp(-t*35); ph+=2*kPi*f/sr; float v=g*std::sin(ph)*std::exp(-t*7)+g*0.3f*std::exp(-t*400)*nd(rng)*0.3f; L[at+i]+=v; R[at+i]+=v;} };
  auto addSnare=[&](int at,float g){ for(int i=0;i<sr/2&&at+i<N;i++){ double t=i/(double)sr; float v=g*(0.5f*std::sin(2*kPi*190*t)*std::exp(-t*25)+0.6f*nd(rng)*std::exp(-t*14)); L[at+i]+=v*0.95f; R[at+i]+=v;} };
  auto addHat=[&](int at,float g){ float hp=0,prev=0; for(int i=0;i<sr/12&&at+i<N;i++){ double t=i/(double)sr; float n=nd(rng); hp=n-prev; prev=n; float v=g*hp*std::exp(-t*60); L[at+i]+=v*0.8f; R[at+i]+=v;} };
  int beat=(int)(sr*60.0/bpm);
  for(int b=0;b<beats;b++){ int at=b*beat; int bar=b%4;
    if(bar==0||bar==2) addKick(at,0.55f); if(bar==2) addKick(at+beat/2+beat/4,0.4f);
    if(bar==1||bar==3) addSnare(at,0.35f);
    for(int e=0;e<2;e++) addHat(at+e*beat/2,0.12f+(e?0.0f:0.04f));
    addSnare(at+beat/4*3,0.06f); }
  // room: simple comb reverb
  { std::vector<float> l2=L,r2=R; int d[4]={1557,1617,1491,1422}; for(int k=0;k<4;k++){ std::vector<float> buf(d[k],0); int idx=0; for(int i=0;i<N;i++){ float o=buf[idx]; buf[idx]=0.5f*(L[i]+R[i])+o*0.72f; idx=(idx+1)%d[k]; l2[i]+=o*0.05f*(k%2?1:0.8f); r2[i]+=o*0.05f*(k%2?0.8f:1);} } L=l2;R=r2; }
  float pk=0; for(int i=0;i<N;i++) pk=std::max(pk,std::max(std::fabs(L[i]),std::fabs(R[i]))); float norm=dbToLin(-6)/pk; for(int i=0;i<N;i++){L[i]*=norm;R[i]*=norm;}
  writeWav("break_dry.wav",L,R,sr);
  auto stats=[&](const std::vector<float>& a,const std::vector<float>& b,int from){ double s=0; float p=0; bool bad=false; for(int i=from;i<(int)a.size();i++){ s+=a[i]*a[i]+b[i]*b[i]; p=std::max(p,std::max(std::fabs(a[i]),std::fabs(b[i]))); if(!std::isfinite(a[i])||!std::isfinite(b[i])) bad=true;} double rms=std::sqrt(s/(2.0*(a.size()-from))); return std::make_tuple(linToDb(rms),linToDb(p),bad); };
  auto [dr,dp,db]=stats(L,R,0); printf("dry: rms %.1f peak %.1f\n",dr,dp);
  struct Case{const char* name;int era;float sq,di,ag;int ag_on;int q;};
  Case cases[]={{"fantasy_default",0,.5f,.5f,.3f,1,1},{"throne_default",1,.5f,.5f,.3f,1,1},{"yeezus_default",2,.5f,.5f,.3f,1,1},
    {"fantasy_zero",0,0,0,0,1,1},{"fantasy_max",0,1,1,1,1,1},{"yeezus_max",2,1,1,1,1,1},{"fantasy_noag",0,.5f,.5f,.3f,0,1},{"fantasy_tr40",0,.5f,.5f,.3f,1,1},{"fantasy_1x",0,.5f,.5f,.3f,1,0},{"throne_max_noag",1,1,1,1,0,0}};
  for(auto& c: cases){
    Engine e; e.setSampleRate(sr);
    e.setParam(pEra,c.era); for(uint32_t i=0;i<kNumAdvParams;i++) e.setParam(kFirstAdvParam+i,kEraDefaults[c.era][i]);
    if(std::string(c.name)=="fantasy_tr40") e.setParam(pTrEnv,40); e.setParam(pSquash,c.sq); e.setParam(pDirt,c.di); e.setParam(pAge,c.ag); e.setParam(pAutoGain,c.ag_on); e.setParam(pQuality,c.q);
    std::vector<float> oL(N),oR(N); float maxGR=0;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int rep=0;rep<1;rep++) for(int i=0;i<N;i+=512){ int n=std::min(512,N-i); e.process(&L[i],&R[i],&oL[i],&oR[i],n); maxGR=std::max(maxGR,e.meterGR);} 
    auto t1=std::chrono::high_resolution_clock::now(); double secs=std::chrono::duration<double>(t1-t0).count(); double audio=N/(double)sr;
    auto [r,p,bad]=stats(oL,oR,sr*3);
    printf("%-18s rms %6.1f  peak %6.1f  GRmax %5.1f  nan %d  cpu %.2f%%\n",c.name,r,p,maxGR,bad,100*secs/audio);
    writeWav(std::string("out_")+c.name+".wav",oL,oR,sr);
  }
  // param sweep stress: random automation every block
  { Engine e; e.setSampleRate(48000); std::mt19937 g(5); std::uniform_real_distribution<float> u(0,1); std::vector<float> oL(N),oR(N); bool bad=false; float pk=0;
    for(int i=0;i<N;i+=64){ for(uint32_t p=0;p<kFirstOutputParam;p++){ if(u(g)<0.05f){ auto& pi=kParamInfo[p]; float v=pi.min+u(g)*(pi.max-pi.min); if(pi.isInt) v=std::round(v); e.setParam(p,v);} }
      if(u(g)<0.01f){ char s[8]; int perm[7]={0,1,2,3,4,5,6}; std::shuffle(perm,perm+7,g); for(int k=0;k<7;k++) s[k]=(char)(48+perm[k]); s[7]=0; e.setOrderString(s);} 
      int n=std::min(64,N-i); e.process(&L[i],&R[i],&oL[i],&oR[i],n); for(int k=i;k<i+n;k++){ if(!std::isfinite(oL[k])) bad=true; pk=std::max(pk,std::fabs(oL[k])); } }
    printf("stress: nan %d peak %.1f dB\n",bad,linToDb(pk)); }
  // silence -> silence & denormal check
  { Engine e; e.setSampleRate(sr); std::vector<float> z(sr*2,0),o1(sr*2),o2(sr*2); auto t0=std::chrono::high_resolution_clock::now(); e.process(z.data(),z.data(),o1.data(),o2.data(),sr*2); auto t1=std::chrono::high_resolution_clock::now(); float m=0; for(float v:o1) m=std::max(m,std::fabs(v)); printf("silence: max %.3g  cpu %.2f%%\n",m,100*std::chrono::duration<double>(t1-t0).count()/2.0);}
}
