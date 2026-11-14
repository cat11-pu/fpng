// Micro-benchmark: RLE run-length scanning alone vs full encode, on flat data.
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <time.h>
#include <algorithm>
static double now(){timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
int main(){
	const uint32_t w=7392,h=5456,ch=3,bpl=1+w*3,N=bpl*h;
	std::vector<uint8_t> d(N);
	// flat-ish: runs of constant pixels
	uint32_t seed=5;
	for(size_t i=0;i<d.size();){seed=seed*1103515245u+12345u; uint32_t r=3*((seed%80)+1); if(i+r>d.size())r=d.size()-i;
		uint8_t v=(uint8_t)(seed>>9); for(uint32_t k=0;k<r;k++)d[i+k]=v; i+=r;}
	std::vector<double> tv;
	volatile uint32_t sink=0;
	for(int rep=0;rep<11;rep++){
		double t0=now();
		uint32_t src=0,matches=0,lits=0;
		for(uint32_t y=0;y<h;y++){
			uint32_t end=src+bpl; src++; // filter byte
			uint32_t prev=*(uint32_t*)(d.data()+src)&0xFFFFFF; src+=3;
			while(src<end){
				uint32_t cur=*(uint32_t*)(d.data()+src)&0xFFFFFF;
				if(cur==prev){
					uint32_t ml=3,mx=255<(end-src)?255:(end-src);
					while(ml<mx){ if((*(uint32_t*)(d.data()+src+ml)&0xFFFFFF)!=cur)break; ml+=3; }
					src+=ml; matches++;
				} else { prev=cur; src+=3; lits++; }
			}
		}
		double t1=now(); tv.push_back((t1-t0)*1e3); sink+=matches+lits;
	}
	std::sort(tv.begin(),tv.end());
	printf("RLE scan-only median=%.2f ms (sink=%u)\n",tv[tv.size()/2],sink);
	return 0;
}
