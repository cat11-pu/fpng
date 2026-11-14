// In-process A/B benchmark: baseline (pristine) vs optimized, identical input,
// interleaved calls to equalize noise. Reports median of many timed runs.
#include "fpng.h"
#include "/tmp/base/fpng_base.h"
#include <stdio.h>
#include <cstring>
#include <stdint.h>
#include <vector>
#include <time.h>
#include <algorithm>
static double now(){timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
static uint32_t st;
static inline uint32_t xs(){uint32_t x=st;x^=x<<13;x^=x>>17;x^=x<<5;st=x;return x;}
static std::vector<uint8_t> make(uint32_t w,uint32_t h,uint32_t ch,int kind){
	st=0xabcdef01u ^ (w*131) ^ (h*17) ^ (ch*7) ^ kind;
	std::vector<uint8_t> img((size_t)w*h*ch);
	if(kind==0){ // photo-ish: gradient + noise
		for(auto&b:img)b=(uint8_t)xs();
	} else { // flat-heavy: RLE rich
		for(size_t i=0;i<img.size();){uint32_t r=1+(xs()%120);uint8_t v=(uint8_t)xs();
			for(uint32_t k=0;k<r&&i<img.size();k++)img[i++]=v;}
	}
	return img;
}
static double med(std::vector<double>& v){std::sort(v.begin(),v.end());return v[v.size()/2];}
int main(){
	fpng::fpng_init(); fpng_base::fpng_init();
	struct T{uint32_t w,h,ch;const char*name;} tiers[]={
		{3648,2736,3,"10MP"},{3648,2736,4,"10MP"},
		{5472,3648,3,"20MP"},{5472,3648,4,"20MP"},
		{7392,5456,3,"40MP"},{7392,5456,4,"40MP"}};
	printf("%-6s %-3s %-5s %10s %10s %8s %10s\n","tier","ch","kind","base(ms)","opt(ms)","speedup","identical");
	for(auto&t:tiers) for(int kind=0;kind<2;kind++){
		auto img=make(t.w,t.h,t.ch,kind);
		std::vector<uint8_t> a,b;
		// correctness
		fpng_base::fpng_encode_image_to_memory(img.data(),t.w,t.h,t.ch,a,0);
		fpng::fpng_encode_image_to_memory(img.data(),t.w,t.h,t.ch,b,0);
		bool ident=a.size()==b.size()&&!memcmp(a.data(),b.data(),a.size());
		const int RUNS=11;
		std::vector<double> ta,tb;
		for(int r=0;r<RUNS;r++){
			double t0=now(); fpng_base::fpng_encode_image_to_memory(img.data(),t.w,t.h,t.ch,a,0); double t1=now();
			double t2=now(); fpng::fpng_encode_image_to_memory(img.data(),t.w,t.h,t.ch,b,0); double t3=now();
			ta.push_back((t1-t0)*1e3); tb.push_back((t3-t2)*1e3);
		}
		double ma=med(ta),mb=med(tb);
		printf("%-6s %-3u %-5s %10.2f %10.2f %7.2fx %10s\n",t.name,t.ch,kind?"flat":"noise",ma,mb,ma/mb,ident?"yes":"NO");
	}
	return 0;
}
