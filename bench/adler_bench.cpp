#include "fpng.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <time.h>
#include <algorithm>
static double now(){timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
int main(){
	fpng::fpng_init();
	const size_t N=40330752*3+7392; // ~40MP RGB filtered size
	std::vector<uint8_t> d(N);
	for(size_t i=0;i<N;i++)d[i]=(uint8_t)(i*131+7);
	std::vector<double> ta,ts;
	volatile uint32_t sink=0;
	for(int r=0;r<12;r++){
		double t0=now(); uint32_t a=fpng::fpng_adler32(d.data(),N,fpng::FPNG_ADLER32_INIT); double t1=now();
		sink+=a; ta.push_back((t1-t0)*1e3);
		// emulate streaming row-by-row with ~bpl=22177+1 rows
		double u0=now();
		uint32_t s1=1,s2=0,bl=5552; const uint8_t* p=d.data(); size_t rem=N;
		while(rem){
			uint32_t n=(uint32_t)(rem<bl?rem:bl); uint32_t i=0;
			for(;i+7<n;i+=8,p+=8){s1+=p[0],s2+=s1;s1+=p[1],s2+=s1;s1+=p[2],s2+=s1;s1+=p[3],s2+=s1;
				s1+=p[4],s2+=s1;s1+=p[5],s2+=s1;s1+=p[6],s2+=s1;s1+=p[7],s2+=s1;}
			for(;i<n;++i)s1+=*p++,s2+=s1;
			rem-=n; bl-=n; if(!bl){s1%=65521;s2%=65521;bl=5552;}
		}
		uint32_t b=(s2%65521)<<16|(s1%65521); double u1=now();
		sink+=b; ts.push_back((u1-u0)*1e3);
		if(a!=b)printf("MISMATCH %08x %08x\n",a,b);
	}
	std::sort(ta.begin(),ta.end()); std::sort(ts.begin(),ts.end());
	printf("SSE full adler median=%.2f ms  streaming median=%.2f ms  (sink=%u)\n",ta[ta.size()/2],ts[ts.size()/2],sink);
	return 0;
}
