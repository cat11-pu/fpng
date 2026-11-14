#include "fpng.h"
#include "/tmp/base/fpng_base.h"
#include <stdio.h>
#include <cstring>
#include <stdint.h>
#include <vector>
#include <cstdlib>
static uint32_t st;
static inline uint32_t xs(){uint32_t x=st;x^=x<<13;x^=x>>17;x^=x<<5;st=x;return x;}
static bool run(uint32_t w,uint32_t h,uint32_t ch,int mode,uint32_t seed){
	st=seed; std::vector<uint8_t> img((size_t)w*h*ch);
	if(mode==0){ // mostly flat runs
		for(size_t i=0;i<img.size();){
			uint32_t run=1+(xs()%200); uint8_t v=(uint8_t)xs();
			for(uint32_t k=0;k<run&&i<img.size();k++) img[i++]=v;
		}
	} else if(mode==1){ // gradient + sparse noise
		for(uint32_t y=0;y<h;y++)for(uint32_t x=0;x<w;x++)for(uint32_t c=0;c<ch;c++){
			uint8_t v=(uint8_t)((x*7+y*13+c*31+(xs()&7))&0xFF);
			img[((size_t)y*w+x)*ch+c]=v;
		}
	} else { // random with repeated rows
		for(uint32_t y=0;y<h;y++)for(uint32_t x=0;x<w;x++)for(uint32_t c=0;c<ch;c++)
			img[((size_t)y*w+x)*ch+c]=(y%3==0)?(uint8_t)xs():(uint8_t)(xs()&3);
	}
	std::vector<uint8_t> a,b;
	if(!fpng_base::fpng_encode_image_to_memory(img.data(),w,h,ch,a,0))return false;
	if(!fpng::fpng_encode_image_to_memory(img.data(),w,h,ch,b,0))return false;
	if(a.size()!=b.size()||memcmp(a.data(),b.data(),a.size())!=0){printf("MISMATCH %ux%u ch%u mode%d\n",w,h,ch,mode);return false;}
	return true;
}
int main(){
	int fails=0,n=0;
	for(uint32_t ch=3;ch<=4;ch++)
	for(int mode=0;mode<3;mode++)
	for(uint32_t dim=1;dim<=300;dim+=37){
		uint32_t w=dim,h=dim+2;
		if(!run(w,h,ch,mode,0x9e3779b9u+dim*7+mode*13+ch)) fails++;
		n++;
	}
	// a few big ones
	for(uint32_t ch=3;ch<=4;ch++)for(int mode=0;mode<3;mode++){
		if(!run(1920,1080,ch,mode,1234+mode))fails++; n++;
	}
	printf("%d cases, %d failures\n",n,fails);
	return fails?1:0;
}
