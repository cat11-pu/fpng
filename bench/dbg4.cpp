#include "fpng.h"
#include <stdio.h>
#include <stdint.h>
#include <vector>
// original tables extracted from pristine source via a generated include
#include "ref_tables.h"
// access current packed tables: include the current cpp's internal header is not
// possible, so re-declare by compiling fpng.cpp with an exported test shim.
extern "C" void fpng_test_dump_tables(uint32_t* h3_code,uint8_t* h3_size,
	uint32_t* m3_code,uint8_t* m3_hsize,uint8_t* m3_ebits,uint32_t* m3_eval,
	uint32_t* h4_code,uint8_t* h4_size,
	uint32_t* m4_code,uint8_t* m4_hsize,uint8_t* m4_ebits,uint32_t* m4_eval);
int main(){
	static uint32_t h3c[288]; static uint8_t h3s[288];
	static uint32_t m3c[256]; static uint8_t m3hs[256],m3eb[256]; static uint32_t m3ev[256];
	static uint32_t h4c[288]; static uint8_t h4s[288];
	static uint32_t m4c[256]; static uint8_t m4hs[256],m4eb[256]; static uint32_t m4ev[256];
	fpng_test_dump_tables(h3c,h3s,m3c,m3hs,m3eb,m3ev,h4c,h4s,m4c,m4hs,m4eb,m4ev);
	int fails=0;
	for(uint32_t i=0;i<288;i++){
		if(h3c[i]!=ref_h3_code[i]||h3s[i]!=ref_h3_size[i]){printf("h3[%u] %u/%u vs %u/%u\n",i,h3c[i],h3s[i],ref_h3_code[i],ref_h3_size[i]);fails++;}
		if(h4c[i]!=ref_h4_code[i]||h4s[i]!=ref_h4_size[i]){printf("h4[%u]\n",i);fails++;}
	}
	for(uint32_t adj=0;adj<256;adj++){
		uint32_t sym=ref_len_sym[adj];
		if(m3c[adj]!=ref_h3_code[sym]||m3hs[adj]!=ref_h3_size[sym]){printf("m3huff adj=%u\n",adj);fails++;}
		uint32_t eb=ref_len_extra[adj]+1;
		uint32_t ev=(adj&ref_bitmasks[ref_len_extra[adj]])<<1;
		if(m3eb[adj]!=eb||m3ev[adj]!=ev){printf("m3extra adj=%u got %u/%u exp %u/%u\n",adj,m3eb[adj],m3ev[adj],eb,ev);fails++;}
		if(m4c[adj]!=ref_h4_code[sym]||m4hs[adj]!=ref_h4_size[sym]){printf("m4huff adj=%u\n",adj);fails++;}
		if(m4eb[adj]!=eb||m4ev[adj]!=ev){printf("m4extra adj=%u\n",adj);fails++;}
	}
	printf("fails=%d\n",fails);
	return fails?1:0;
}
