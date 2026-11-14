#include "fpng.h"
#include "/tmp/base/fpng_base.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <cstring>
static uint32_t st;
static inline uint32_t xs(){uint32_t x=st;x^=x<<13;x^=x>>17;x^=x<<5;st=x;return x;}
int main(){
	uint32_t w=38,h=40,ch=3,mode=0; st=0x9e3779b9u+w*7+mode*13+ch;
	std::vector<uint8_t> img((size_t)w*h*ch);
	for(size_t i=0;i<img.size();){uint32_t run=1+(xs()%200);uint8_t v=(uint8_t)xs();
		for(uint32_t k=0;k<run&&i<img.size();k++)img[i++]=v;}
	std::vector<uint8_t> a,b;
	fpng_base::fpng_encode_image_to_memory(img.data(),w,h,ch,a,0);
	fpng::fpng_encode_image_to_memory(img.data(),w,h,ch,b,0);
	printf("sizes %zu %zu\n",a.size(),b.size());
	size_t n=a.size()<b.size()?a.size():b.size();
	for(size_t i=0;i<n;i++) if(a[i]!=b[i]){
		printf("first diff %zu: base=%02x new=%02x\n",i,a[i],b[i]);
		size_t lo=i>20?i-20:0;
		printf("base: "); for(size_t k=lo;k<i+12;k++)printf("%02x ",a[k]); printf("\nnew : "); for(size_t k=lo;k<i+12;k++)printf("%02x ",b[k]); printf("\n");
		break;
	}
	return 0;
}
