// Encode the real example.png with baseline and optimized, compare bytes.
#include "fpng.h"
#include "/tmp/base/fpng_base.h"
#include <stdio.h>
#include <vector>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#include "stb_image.h"
int main(int argc,char**argv){
	int w,h,n;
	uint8_t* px=stbi_load(argc>1?argv[1]:"example.png",&w,&h,&n,0);
	if(!px){printf("load fail\n");return 1;}
	int ch=n==4?4:3;
	std::vector<uint8_t> a,b;
	fpng_base::fpng_init(); fpng::fpng_init();
	fpng_base::fpng_encode_image_to_memory(px,w,h,ch,a,0);
	fpng::fpng_encode_image_to_memory(px,w,h,ch,b,0);
	bool ok=a.size()==b.size()&&!memcmp(a.data(),b.data(),a.size());
	printf("example.png %dx%d ch%d base=%zu new=%zu %s\n",w,h,ch,a.size(),b.size(),ok?"IDENTICAL":"MISMATCH");
	stbi_image_free(px);
	return ok?0:2;
}
