// Compare bit-packing: original per-symbol flush vs GUARANTEE_FLUSH batching.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "ref_tables.h"

static inline void w64(uint8_t*p,uint64_t v){memcpy(p,&v,8);}

// original style
static std::vector<uint8_t> orig_pack(const std::vector<uint32_t>& syms,
                                      const std::vector<uint32_t>& sizes){
	std::vector<uint8_t> out(4096);
	uint64_t bb=0; int bbs=0; uint32_t ofs=0;
	auto flush=[&](){ w64(out.data()+ofs,bb); uint32_t sh=bbs&~7; ofs+=sh>>3; bb>>=sh; bbs-=sh; };
	for(size_t i=0;i<syms.size();i++){ bb|=(uint64_t)syms[i]<<bbs; bbs+=sizes[i]; flush(); }
	while(bbs>0){out[ofs++]=(uint8_t)bb;bb>>=8;bbs-=8;}
	out.resize(ofs); return out;
}

// new style: groups of 3 symbols (RGB literals) with GUARANTEE_FLUSH(36)
static std::vector<uint8_t> new_pack(const std::vector<uint32_t>& syms,
                                     const std::vector<uint32_t>& sizes){
	std::vector<uint8_t> out(4096);
	uint64_t bb=0; int bbs=0; uint32_t ofs=0;
	auto gflush=[&](int needed){ if(bbs+needed>56){ w64(out.data()+ofs,bb); uint32_t sh=bbs&~7; ofs+=sh>>3; bb>>=sh; bbs-=sh; } };
	size_t i=0;
	for(;i+3<=syms.size();i+=3){
		gflush((int)(sizes[i]+sizes[i+1]+sizes[i+2]));
		for(int k=0;k<3;k++){bb|=(uint64_t)syms[i+k]<<bbs;bbs+=sizes[i+k];}
	}
	gflush(12);
	for(;i<syms.size();i++){bb|=(uint64_t)syms[i]<<bbs;bbs+=sizes[i];}
	while(bbs>0){out[ofs++]=(uint8_t)bb;bb>>=8;bbs-=8;}
	out.resize(ofs); return out;
}

int main(){
	uint32_t seed=12345;
	int fails=0;
	for(int t=0;t<10000;t++){
		int n=3+(seed%300); seed=seed*1103515245u+12345u;
		std::vector<uint32_t> s(n),z(n);
		for(int i=0;i<n;i++){ s[i]=seed%200; seed=seed*1103515245u+12345u; z[i]=ref_h3_size[s[i]]; }
		auto a=orig_pack(s,z), b=new_pack(s,z);
		if(a.size()!=b.size()||memcmp(a.data(),b.data(),a.size())){
			printf("MISMATCH t=%d n=%d sizes %zu/%zu\n",t,n,a.size(),b.size());
			for(size_t k=0;k<a.size()&&k<b.size();k++) if(a[k]!=b[k]){printf("byte %zu %02x/%02x\n",k,a[k],b[k]);break;}
			if(++fails>3)break;
		}
	}
	printf("done fails=%d\n",fails);
	return fails?1:0;
}
