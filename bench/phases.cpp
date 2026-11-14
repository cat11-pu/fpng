// Phase timing: replicate fpng_encode_image_to_memory pipeline stages.
#include "fpng.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <time.h>

#include <cstring>
#include <emmintrin.h>
#include <smmintrin.h>
static double now_sec() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec/1e9; }
static uint32_t st;
static inline uint32_t xs(){uint32_t x=st; x^=x<<13;x^=x>>17;x^=x<<5; st=x; return x;}

int main()
{
	fpng::fpng_init();
	struct T{uint32_t w,h,ch;} cases[]={{7392,5456,3},{7392,5456,4}};
	for (auto c : cases)
	{
		st=1; uint32_t w=c.w,h=c.h,ch=c.ch,bpl=w*ch;
		std::vector<uint8_t> img((size_t)w*h*ch);
		for (auto& b: img) b=(uint8_t)xs();
		std::vector<uint8_t> filtered((bpl+1)*h+7);
		std::vector<uint8_t> out;

		// filter pass (SSE sub + memcpy), same as apply_filter loop
		double t0=now_sec();
		for (int it=0;it<5;it++)
		{
			uint32_t ofs=0;
			for (uint32_t y=0;y<h;y++)
			{
				const uint8_t* s=img.data()+(size_t)y*bpl;
				uint8_t* d=&filtered[ofs];
				if(!y){*d++=0; memcpy(d,s,bpl);}
				else
				{
					*d++=2;
					const uint8_t* p=img.data()+(size_t)(y-1)*bpl;
					uint32_t n=bpl,o2=0;
					for(;n>=16;n-=16,o2+=16)
					{
						__m128i a=_mm_loadu_si128((const __m128i*)(s+o2));
						__m128i b=_mm_loadu_si128((const __m128i*)(p+o2));
						_mm_storeu_si128((__m128i*)(d+o2),_mm_sub_epi8(a,b));
					}
					for(;n;n--,o2++) d[o2]=s[o2]-p[o2];
				}
				ofs+=1+bpl;
			}
		}
		double t1=now_sec();
		// adler
		uint32_t ad=0;
		double t2=now_sec();
		for(int it=0;it<5;it++) ad=fpng::fpng_adler32(filtered.data(),(size_t)(bpl+1)*h,fpng::FPNG_ADLER32_INIT);
		double t3=now_sec();
		// full encode
		double best=1e9;
		for(int it=0;it<5;it++){out.clear();double a=now_sec();
			fpng::fpng_encode_image_to_memory(img.data(),w,h,ch,out,0);
			double b=now_sec(); if(b-a<best)best=b-a;}
		double t4=now_sec();
		printf("ch=%u  filter=%.2fms  adler=%.2fms  full_encode=%.2fms  (ad=%08x)\n",
			ch,(t1-t0)/5*1e3,(t3-t2)/5*1e3,best*1e3,ad);
	}
}
