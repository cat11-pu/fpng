#include "fpng.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <cstring>
#include <cstdlib>
// mirror of fpng_adler32_scalar block logic
static uint32_t scalar_adler(const uint8_t* ptr,size_t buf_len,uint32_t adler){
	uint32_t s1=adler&0xffff,s2=adler>>16; uint32_t block_len=(uint32_t)(buf_len%5552);
	while(buf_len){
		uint32_t i=0;
		for(;i+7<block_len;i+=8,ptr+=8){
			s1+=ptr[0],s2+=s1;s1+=ptr[1],s2+=s1;s1+=ptr[2],s2+=s1;s1+=ptr[3],s2+=s1;
			s1+=ptr[4],s2+=s1;s1+=ptr[5],s2+=s1;s1+=ptr[6],s2+=s1;s1+=ptr[7],s2+=s1;
		}
		for(;i<block_len;++i)s1+=*ptr++,s2+=s1;
		s1%=65521U;s2%=65521U;buf_len-=block_len;block_len=5552;
	}
	return (s2<<16)+s1;
}
// incremental: feed in arbitrary chunk sizes, reducing every 5552 bytes
static uint32_t inc_adler(const std::vector<uint8_t>& data,size_t chunk,uint32_t adler){
	uint32_t s1=adler&0xffff,s2=adler>>16;
	size_t pos=0, total=data.size();
	uint32_t block_left=5552;
	while(pos<total){
		size_t n=total-pos; if(n>chunk)n=chunk; if(n>block_left)n=block_left;
		const uint8_t* p=data.data()+pos;
		size_t i=0;
		for(;i+7<n;i+=8,p+=8){
			s1+=p[0],s2+=s1;s1+=p[1],s2+=s1;s1+=p[2],s2+=s1;s1+=p[3],s2+=s1;
			s1+=p[4],s2+=s1;s1+=p[5],s2+=s1;s1+=p[6],s2+=s1;s1+=p[7],s2+=s1;
		}
		for(;i<n;++i)s1+=*p++,s2+=s1;
		pos+=n; block_left-=(uint32_t)n;
		if(!block_left){s1%=65521U;s2%=65521U;block_left=5552;}
	}
	s1%=65521U;s2%=65521U;
	return (s2<<16)+s1;
}
int main(){
	fpng::fpng_init();
	uint32_t seed=99; int fails=0;
	for(int t=0;t<300;t++){
		size_t n=(seed%20000)+1; seed=seed*1103515245u+12345u;
		std::vector<uint8_t> d(n);
		for(auto&b:d)b=(uint8_t)(seed=seed*1103515245u+12345u);
		uint32_t ref=fpng::fpng_adler32(d.data(),n,fpng::FPNG_ADLER32_INIT);
		uint32_t sc=scalar_adler(d.data(),n,1);
		size_t chunks[]={1,3,7,114,115,5551,5552,5553,12000};
		for(size_t c:chunks){
			uint32_t iv=inc_adler(d,c,1);
			if(iv!=ref||sc!=ref){printf("fail n=%zu chunk=%zu ref=%08x inc=%08x sc=%08x\n",n,c,ref,iv,sc);fails++;break;}
		}
	}
	printf("adler fails=%d\n",fails);
	return fails?1:0;
}
