// Match emission: original two-macro sequence vs packed EMIT_MATCH descriptor.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "ref_tables.h"
static inline void w64(uint8_t*p,uint64_t v){memcpy(p,&v,8);}
static uint32_t bitmasks[17];
int main(){
	for(int i=0;i<17;i++) bitmasks[i]=(1u<<i)-1; bitmasks[0]=0;
	int fails=0;
	for(uint32_t adj=0;adj<256;adj++){
		for(uint32_t init_bbs=0;init_bbs<=8;init_bbs++){
			uint32_t sym=ref_len_sym[adj];
			// original
			uint8_t o[64]={0}, n[64]={0};
			uint64_t ob=0x1122334455667788ULL; int obs=(int)init_bbs; uint32_t oo=0;
			uint64_t nb=ob; int nbs=obs; uint32_t no=0;
			// orig: PUT_BITS_CZ(huff code) then PUT_BITS(extra, extra_bits+1)
			ob |= (uint64_t)ref_h3_code[sym] << obs; obs += ref_h3_size[sym];
			uint32_t ebits=ref_len_extra[adj]+1;
			ob |= (uint64_t)(adj & bitmasks[ref_len_extra[adj]]) << obs; obs += ebits;
			w64(o+oo,ob);
			// new packed: huff then (value<<1) precomputed
			nb |= (uint64_t)ref_h3_code[sym] << nbs; nbs += ref_h3_size[sym];
			uint32_t ev=(adj & bitmasks[ref_len_extra[adj]])<<1;
			nb |= (uint64_t)ev << nbs; nbs += ebits;
			w64(n+no,nb);
			if(obs!=nbs || memcmp(o,n,16)){
				printf("MISMATCH adj=%u init=%u obs=%d nbs=%d\n",adj,init_bbs,obs,nbs);
				for(int k=0;k<8;k++)printf("%02x/%02x ",o[k],n[k]); printf("\n");
				if(++fails>5)return 1;
			}
		}
	}
	printf("match emit fails=%d\n",fails);
	return fails?1:0;
}
