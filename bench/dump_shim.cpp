// Compiled together with fpng.cpp (same TU via include) to reach static tables.
#include "fpng.cpp"
extern "C" void fpng_test_dump_tables(uint32_t* h3_code,uint8_t* h3_size,
	uint32_t* m3_code,uint8_t* m3_hsize,uint8_t* m3_ebits,uint32_t* m3_eval,
	uint32_t* h4_code,uint8_t* h4_size,
	uint32_t* m4_code,uint8_t* m4_hsize,uint8_t* m4_ebits,uint32_t* m4_eval)
{
	fpng::init_packed_huff_tables();
	for(uint32_t i=0;i<288;i++){
		h3_code[i]=fpng::g_packed_huff_3[i].m_code; h3_size[i]=fpng::g_packed_huff_3[i].m_code_size;
		h4_code[i]=fpng::g_packed_huff_4[i].m_code; h4_size[i]=fpng::g_packed_huff_4[i].m_code_size;
	}
	for(uint32_t a=0;a<256;a++){
		m3_code[a]=fpng::g_packed_match_3[a].m_huff.m_code;
		m3_hsize[a]=fpng::g_packed_match_3[a].m_huff.m_code_size;
		m3_ebits[a]=fpng::g_packed_match_3[a].m_extra_bits;
		m3_eval[a]=fpng::g_packed_match_3[a].m_extra_value;
		m4_code[a]=fpng::g_packed_match_4[a].m_huff.m_code;
		m4_hsize[a]=fpng::g_packed_match_4[a].m_huff.m_code_size;
		m4_ebits[a]=fpng::g_packed_match_4[a].m_extra_bits;
		m4_eval[a]=fpng::g_packed_match_4[a].m_extra_value;
	}
}
