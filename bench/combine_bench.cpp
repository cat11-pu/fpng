// Compare: 3 dependent shifts/or per RGB pixel vs one combined 64-bit build.
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <cstring>
#include "ref_tables.h"
static double now(){timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
static inline void w64(uint8_t*p,uint64_t v){memcpy(p,&v,8);}
int main(){
	const int PIX=20000000;
	std::vector<uint8_t> out((size_t)PIX*6+64), pix(PIX*3);
	uint32_t s=7; for(auto&b:pix){s=s*1103515245u+12345u;b=(uint8_t)s;}
	std::vector<double> ta,tb; volatile uint64_t sink=0;
	for(int rep=0;rep<9;rep++){
		// A: three separate emits
		{uint64_t bb=0;int bbs=0;uint32_t ofs=0;double t0=now();
		 for(int i=0;i<PIX;i++){
			uint32_t l=pix[3*i]|(pix[3*i+1]<<8)|(pix[3*i+2]<<16);
			if(bbs+36>56){w64(out.data()+ofs,bb);uint32_t sh=bbs&~7;ofs+=sh>>3;bb>>=sh;bbs-=sh;}
			bb|=(uint64_t)ref_h3_code[l&255]<<bbs;bbs+=ref_h3_size[l&255];
			bb|=(uint64_t)ref_h3_code[(l>>8)&255]<<bbs;bbs+=ref_h3_size[(l>>8)&255];
			bb|=(uint64_t)ref_h3_code[l>>16]<<bbs;bbs+=ref_h3_size[l>>16];
		 }
		 while(bbs>0){out[ofs++]=(uint8_t)bb;bb>>=8;bbs-=8;}
		 double t1=now();ta.push_back((t1-t0)*1e3);sink+=ofs;}
		// B: combine three codes into one value first (independent shifts), one or
		{uint64_t bb=0;int bbs=0;uint32_t ofs=0;double t0=now();
		 for(int i=0;i<PIX;i++){
			uint32_t l=pix[3*i]|(pix[3*i+1]<<8)|(pix[3*i+2]<<16);
			uint32_t s0=ref_h3_size[l&255],s1=ref_h3_size[(l>>8)&255];
			uint64_t combined=(uint64_t)ref_h3_code[l&255]
				|((uint64_t)ref_h3_code[(l>>8)&255]<<s0)
				|((uint64_t)ref_h3_code[l>>16]<<(s0+s1));
			uint32_t total=s0+s1+ref_h3_size[l>>16];
			if(bbs+(int)total>56){w64(out.data()+ofs,bb);uint32_t sh=bbs&~7;ofs+=sh>>3;bb>>=sh;bbs-=sh;}
			bb|=combined<<bbs;bbs+=(int)total;
		 }
		 while(bbs>0){out[ofs++]=(uint8_t)bb;bb>>=8;bbs-=8;}
		 double t1=now();tb.push_back((t1-t0)*1e3);sink+=ofs;}
	}
	std::sort(ta.begin(),ta.end());std::sort(tb.begin(),tb.end());
	printf("separate=%.2f ms  combined=%.2f ms  ratio=%.2fx (sink=%llu)\n",ta[ta.size()/2],tb[tb.size()/2],ta[ta.size()/2]/tb[tb.size()/2],(unsigned long long)sink);
}
