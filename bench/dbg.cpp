#include "fpng.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <cstring>
static uint32_t st=0x12345678u;
static inline uint32_t xs(){uint32_t x=st;x^=x<<13;x^=x>>17;x^=x<<5;st=x;return x;}
int main()
{
	fpng::fpng_init();
	uint32_t w=64,h=4,ch=3;
	std::vector<uint8_t> img(w*h*ch);
	for(auto& b:img)b=(uint8_t)xs();
	std::vector<uint8_t> out;
	bool ok=fpng::fpng_encode_image_to_memory(img.data(),w,h,ch,out,0);
	printf("ok=%d size=%zu\n",ok,out.size());
	FILE* f=fopen("bench/ref/10MP_c3.png","rb");
	fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
	std::vector<uint8_t> ref(n);
	fread(ref.data(),1,n,f); fclose(f);
	// compare structure: print first 64 bytes of each
	for(size_t i=0;i<64&&i<out.size();i++){
		if(i%16==0)printf("\n%4zu: ",i);
		printf("%02x ",out[i]);
	}
	printf("\nref:\n");
	for(size_t i=0;i<64;i++){
		if(i%16==0)printf("\n%4zu: ",i);
		printf("%02x ",ref[i]);
	}
	printf("\n");
	return 0;
}
