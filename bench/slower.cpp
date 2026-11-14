#include "fpng.h"
#include "/tmp/base/fpng_base.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <cstring>
#include <cstdlib>
static uint32_t st;
static inline uint32_t xs(){uint32_t x=st;x^=x<<13;x^=x>>17;x^=x<<5;st=x;return x;}
int main(){
	int fails=0,n=0;
	for(uint32_t ch=3;ch<=4;ch++)for(uint32_t dim=1;dim<=200;dim+=29){
		uint32_t w=dim,h=dim+3; st=777+dim*3+ch;
		std::vector<uint8_t> img((size_t)w*h*ch);
		for(auto&b:img)b=(uint8_t)xs();
		std::vector<uint8_t> a,b;
		fpng_base::fpng_encode_image_to_memory(img.data(),w,h,ch,a,fpng_base::FPNG_ENCODE_SLOWER);
		fpng::fpng_encode_image_to_memory(img.data(),w,h,ch,b,fpng::FPNG_ENCODE_SLOWER);
		n++;
		if(a.size()!=b.size()||memcmp(a.data(),b.data(),a.size())){printf("SLOWER MISMATCH %ux%u ch%u\n",w,h,ch);fails++;}
	}
	printf("slower %d cases %d fails\n",n,fails);
	return fails?1:0;
}
