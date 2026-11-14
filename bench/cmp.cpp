#include "fpng.h"
#include "/tmp/base/fpng_base.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <cstdlib>
static uint32_t st=0x12345678u;
static inline uint32_t xs(){uint32_t x=st;x^=x<<13;x^=x>>17;x^=x<<5;st=x;return x;}
static std::vector<uint8_t> make(uint32_t w,uint32_t h,uint32_t ch){
	st=0x12345678u; std::vector<uint8_t> img((size_t)w*h*ch);
	for(auto& b:img)b=(uint8_t)xs(); return img;
}
int main(int argc,char**argv){
	fpng::fpng_init(); fpng_base::fpng_init();
	uint32_t W=argc>1?(uint32_t)atoi(argv[1]):3648,H=argc>2?(uint32_t)atoi(argv[2]):2736,ch=argc>3?(uint32_t)atoi(argv[3]):3;
	auto img=make(W,H,ch);
	std::vector<uint8_t> a,b;
	bool ok1=fpng_base::fpng_encode_image_to_memory(img.data(),W,H,ch,a,0);
	bool ok2=fpng::fpng_encode_image_to_memory(img.data(),W,H,ch,b,0);
	if(!ok1||!ok2){printf("encode fail %d %d\n",ok1,ok2);return 3;}
	printf("base=%zu new=%zu\n",a.size(),b.size());
	size_t n=a.size()<b.size()?a.size():b.size(), first=(size_t)-1;
	for(size_t i=0;i<n;i++) if(a[i]!=b[i]){first=i;break;}
	if(first==(size_t)-1 && a.size()==b.size()) printf("IDENTICAL\n");
	else{
		printf("first diff at %zu (0x%zx)\n",first,first);
		for(size_t i=first?first-8:0;i<first+16&&i<b.size()&&i<a.size();i++) printf("%zu:%02x/%02x ",i,a[i],b[i]);
		printf("\n");
	}
}
