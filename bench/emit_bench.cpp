// Pure bit-packing throughput: original multi-table sequence vs packed match
// descriptor, over the same stream of adjusted match lengths.
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <cstring>
#include "ref_tables.h"
static double now(){timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
static inline void w64(uint8_t*p,uint64_t v){memcpy(p,&v,8);}
// packed table built like the encoder
struct pm{uint32_t hc,hs,eb,ev;};
int main(){
	pm tbl[256];
	for(uint32_t a=0;a<256;a++){uint32_t s=ref_len_sym[a];
		tbl[a].hc=ref_h3_code[s];tbl[a].hs=ref_h3_size[s];
		tbl[a].eb=ref_len_extra[a]+1;tbl[a].ev=a&ref_bitmasks[ref_len_extra[a]];}
	const int N=20000000;
	std::vector<uint8_t> out(N*3+64);
	std::vector<uint8_t> adj(N);
	uint32_t seed=1; for(auto&a:adj){seed=seed*1103515245u+12345u;a=(seed>>8)%253;} // mostly valid RGB adj 0..252
	std::vector<double> to,tp;
	volatile uint64_t sink=0;
	for(int rep=0;rep<9;rep++){
		// original
		{uint64_t bb=0;int bbs=0;uint32_t ofs=0;double t0=now();
		 for(int i=0;i<N;i++){uint32_t a=adj[i];uint32_t s=ref_len_sym[a];
			bb|=(uint64_t)ref_h3_code[s]<<bbs;bbs+=ref_h3_size[s];
			bb|=(uint64_t)(a&ref_bitmasks[ref_len_extra[a]])<<bbs;bbs+=ref_len_extra[a]+1;
			if(bbs&~7){w64(out.data()+ofs,bb);uint32_t sh=bbs&~7;ofs+=sh>>3;bb>>=sh;bbs-=sh;}}
		 while(bbs>0){out[ofs++]=(uint8_t)bb;bb>>=8;bbs-=8;}
		 double t1=now();to.push_back((t1-t0)*1e3);sink+=ofs;}
		// packed
		{uint64_t bb=0;int bbs=0;uint32_t ofs=0;double t0=now();
		 for(int i=0;i<N;i++){const pm&ms=tbl[adj[i]];
			if(bbs+(int)ms.hs+(int)ms.eb>56){w64(out.data()+ofs,bb);uint32_t sh=bbs&~7;ofs+=sh>>3;bb>>=sh;bbs-=sh;}
			bb|=(uint64_t)ms.hc<<bbs;bbs+=ms.hs;
			bb|=(uint64_t)ms.ev<<bbs;bbs+=ms.eb;}
		 while(bbs>0){out[ofs++]=(uint8_t)bb;bb>>=8;bbs-=8;}
		 double t1=now();tp.push_back((t1-t0)*1e3);sink+=ofs;}
	}
	std::sort(to.begin(),to.end());std::sort(tp.begin(),tp.end());
	printf("orig emit=%.2f ms  packed emit=%.2f ms  ratio=%.2fx (sink=%llu)\n",to[to.size()/2],tp[tp.size()/2],to[tp.size()/2]/tp[tp.size()/2],(unsigned long long)sink);
}
