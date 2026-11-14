// Benchmark harness for fpng encoder optimizations.
// Measures fpng_encode_image_to_memory() across 3 image size tiers and
// verifies byte-for-byte identity against a reference PNG captured earlier.
#include "fpng.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <time.h>

static double now_sec()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static uint32_t rng_state = 0x12345678u;
static inline uint32_t xorshift32()
{
	uint32_t x = rng_state;
	x ^= x << 13; x ^= x >> 17; x ^= x << 5;
	rng_state = x;
	return x;
}

// Deterministic, photo-ish synthetic image: smooth gradients + noise +
// occasional flat regions, so both filtering and RLE paths get exercised.
static std::vector<uint8_t> make_image(uint32_t w, uint32_t h, uint32_t chans)
{
	rng_state = 0x12345678u;
	std::vector<uint8_t> img((size_t)w * h * chans);
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
		{
			uint8_t* p = &img[((size_t)y * w + x) * chans];
			uint32_t flat = ((x / 64) + (y / 64)) & 3;
			if (flat == 0)
			{
				p[0] = (uint8_t)((x * 3 + (xorshift32() & 15)) & 0xFF);
				p[1] = (uint8_t)((y * 5 + (xorshift32() & 15)) & 0xFF);
				p[2] = (uint8_t)(((x + y) * 2 + (xorshift32() & 15)) & 0xFF);
			}
			else
			{
				p[0] = p[1] = p[2] = (uint8_t)(flat * 40);
				xorshift32(); xorshift32();
			}
			if (chans == 4)
				p[3] = (uint8_t)((x ^ y) & 0xFF);
		}
	}
	return img;
}

struct Tier { uint32_t w, h; const char* name; };

int main(int argc, char** argv)
{
	bool write_ref = argc > 1 && strcmp(argv[1], "--write-ref") == 0;
	bool check_ref = argc > 1 && strcmp(argv[1], "--check-ref") == 0;
	const char* ref_dir = argc > 2 ? argv[2] : "bench/ref";

	const Tier tiers[] = {
		{ 3648, 2736, "10MP" },  // ~9.98 MP
		{ 5472, 3648, "20MP" },  // ~19.96 MP
		{ 7392, 5456, "40MP" },  // ~40.33 MP
	};

	fpng::fpng_init();
	printf("SSE4.1: %s\n", fpng::fpng_cpu_supports_sse41() ? "yes" : "no");
	printf("%-6s %-5s %10s %12s %10s %10s %s\n",
		"tier", "ch", "pixels", "encode(ms)", "size(KB)", "mps", "check");

	for (const Tier& t : tiers)
	{
		for (uint32_t chans : {3u, 4u})
		{
			std::vector<uint8_t> img = make_image(t.w, t.h, chans);
			std::vector<uint8_t> out;

			// warmup + timed runs, report best of 15
			double best = 1e9;
			for (int iter = 0; iter < 9; iter++)
			{
				out.clear();
				double t0 = now_sec();
				bool ok = fpng::fpng_encode_image_to_memory(img.data(), t.w, t.h, chans, out, 0);
				double t1 = now_sec();
				if (!ok) { fprintf(stderr, "encode failed\n"); return 1; }
				double ms = (t1 - t0) * 1000.0;
				if (ms < best) best = ms;
			}

			double mp = (double)t.w * t.h / 1e6;
			double mps = mp / (best / 1000.0);

			std::string path = std::string(ref_dir) + "/" + t.name +
				"_c" + (chans == 3 ? "3" : "4") + ".png";
			const char* check = "-";
			if (write_ref)
			{
				FILE* f = fopen(path.c_str(), "wb");
				fwrite(out.data(), 1, out.size(), f);
				fclose(f);
				check = "ref-written";
			}
			else if (check_ref)
			{
				FILE* f = fopen(path.c_str(), "rb");
				if (!f) { fprintf(stderr, "missing ref %s\n", path.c_str()); return 1; }
				fseek(f, 0, SEEK_END);
				long n = ftell(f);
				fseek(f, 0, SEEK_SET);
				std::vector<uint8_t> ref(n);
				if (fread(ref.data(), 1, n, f) != (size_t)n) { fprintf(stderr, "read fail\n"); return 1; }
				fclose(f);
				check = (ref.size() == out.size() && memcmp(ref.data(), out.data(), n) == 0)
					? "IDENTICAL" : "MISMATCH!";
				if (check[0] == 'M') return 2;
			}

			printf("%-6s %-5u %10zu %12.2f %10.1f %10.1f %s\n",
				t.name, chans, (size_t)t.w * t.h, best, out.size() / 1024.0, mps, check);
		}
	}
	return 0;
}
