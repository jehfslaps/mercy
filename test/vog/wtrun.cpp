// run a raw stereo f32 file through MERCY's Weight at the given knobs: wtrun in.f32 out.f32 center freq amp flat
#include "../src/MercyDSP.hpp"
#include <cstdio>
#include <vector>
#include <cstdlib>
using namespace mercy;
int main(int c, char** v){ FILE* f=fopen(v[1],"rb"); std::vector<float> x; float b[4096]; size_t n;
 while((n=fread(b,4,4096,f))>0) x.insert(x.end(),b,b+n); fclose(f);
 Weight w; w.reset(); w.configure(44100.0, atof(v[3]), atof(v[4]), atof(v[5]), atof(v[6]));
 for(size_t i=0;i+1<x.size();i+=2) w.process(x[i],x[i+1]);
 f=fopen(v[2],"wb"); fwrite(x.data(),4,x.size(),f); fclose(f); }
