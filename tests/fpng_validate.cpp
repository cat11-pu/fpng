// Deterministic fpng round-trip / determinism validation harness.
//
// Modes:
//   fpng_validate random  --count 2000 --seed 20261114 --manifest <file>
//   fpng_validate extreme --manifest <file>
//   fpng_validate invalid --manifest <file>
//
// Every valid case is encoded twice (fresh output buffers), the two PNG byte
// streams are compared, then the PNG is decoded with both fpng and lodepng and
// compared byte-for-byte against the source pixels.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include <algorithm>
#include <string>
#include <vector>

#include "fpng.h"
#include "lodepng.h"

namespace {

struct Rng {
  uint64_t state;

  explicit Rng(uint64_t seed) : state(seed) {}

  uint64_t next() {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

  uint32_t u32() { return static_cast<uint32_t>(next() >> 32); }

  uint32_t range(uint32_t lo, uint32_t hi_exclusive) {
    if (hi_exclusive <= lo) return lo;
    const uint32_t span = hi_exclusive - lo;
    return lo + static_cast<uint32_t>(
                    (static_cast<uint64_t>(u32()) * span) >> 32);
  }

  uint8_t byte() { return static_cast<uint8_t>(next() >> 56); }
};

uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

uint64_t fnv1a64(const uint8_t* p, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  return h;
}

double now_secs() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<double>(ts.tv_sec) +
         static_cast<double>(ts.tv_nsec) / 1e9;
}

enum Pattern {
  PAT_RANDOM = 0,
  PAT_CONST,
  PAT_HORIZ_BANDS,
  PAT_VERT_BANDS,
  PAT_GRADIENT,
  PAT_CHECKER,
  PAT_RUNS,
  PAT_NOISE_BLOBS,
  PAT_BORDER,
  PAT_COUNT
};

const char* const kPatternNames[PAT_COUNT] = {
    "random",      "const",      "horiz_bands", "vert_bands", "gradient",
    "checker",     "runs",       "noise_blobs", "border",
};

struct CaseSpec {
  std::string tag;
  uint32_t width;
  uint32_t height;
  uint32_t channels;
  Pattern pattern;
  uint32_t flags;
};

struct Stats {
  uint64_t count;
  uint64_t hard_errors;
  uint64_t fpng_decode_failures;
  uint64_t lodepng_decode_failures;
  uint64_t pixel_mismatch_cases;
  uint64_t mismatched_pixels;
  uint64_t nondeterministic;
  uint64_t info_mismatch;
  uint64_t encode_calls;
  uint64_t decode_calls;
  uint64_t total_pixels;
  uint64_t total_raw_bytes;
  uint64_t total_png_bytes;
  uint32_t min_width;
  uint32_t max_width;
  uint32_t min_height;
  uint32_t max_height;
  uint64_t rgb_images;
  uint64_t rgba_images;
  uint64_t flags_count[3];
  uint64_t pattern_count[PAT_COUNT];
  double encode_secs;
  double fpng_decode_secs;
  double lodepng_decode_secs;
  double elapsed_secs;

  Stats()
      : count(0),
        hard_errors(0),
        fpng_decode_failures(0),
        lodepng_decode_failures(0),
        pixel_mismatch_cases(0),
        mismatched_pixels(0),
        nondeterministic(0),
        info_mismatch(0),
        encode_calls(0),
        decode_calls(0),
        total_pixels(0),
        total_raw_bytes(0),
        total_png_bytes(0),
        min_width(0xFFFFFFFFu),
        max_width(0),
        min_height(0xFFFFFFFFu),
        max_height(0),
        rgb_images(0),
        rgba_images(0),
        encode_secs(0),
        fpng_decode_secs(0),
        lodepng_decode_secs(0),
        elapsed_secs(0) {
    memset(flags_count, 0, sizeof(flags_count));
    memset(pattern_count, 0, sizeof(pattern_count));
  }
};

inline void store_pixel(uint8_t* dst, const uint8_t color[4], uint32_t ch) {
  for (uint32_t c = 0; c < ch; ++c) dst[c] = color[c];
}

void generate_pixels(const CaseSpec& spec, uint64_t pixel_seed,
                     std::vector<uint8_t>& pixels) {
  const uint32_t w = spec.width;
  const uint32_t h = spec.height;
  const uint32_t ch = spec.channels;
  const uint64_t total = static_cast<uint64_t>(w) * h;

  pixels.resize(static_cast<size_t>(total * ch));
  Rng r(pixel_seed);

  if (spec.pattern == PAT_RANDOM) {
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = r.byte();
    return;
  }

  if (spec.pattern == PAT_CONST) {
    uint8_t color[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
    for (uint64_t i = 0; i < total; ++i)
      store_pixel(&pixels[static_cast<size_t>(i * ch)], color, ch);
    return;
  }

  if (spec.pattern == PAT_HORIZ_BANDS) {
    const uint32_t band_h = r.range(1, 33);
    uint8_t color[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
    for (uint32_t y = 0; y < h; ++y) {
      if ((y % band_h) == 0) {
        color[0] = r.byte();
        color[1] = r.byte();
        color[2] = r.byte();
        color[3] = r.byte();
      }
      uint8_t* row = &pixels[static_cast<size_t>(y) * w * ch];
      for (uint32_t x = 0; x < w; ++x)
        store_pixel(row + static_cast<size_t>(x) * ch, color, ch);
    }
    return;
  }

  if (spec.pattern == PAT_VERT_BANDS) {
    const uint32_t band_w = r.range(1, 33);
    std::vector<uint8_t> color(static_cast<size_t>(w) * 4);
    for (uint32_t x = 0; x < w; ++x) {
      if ((x % band_w) == 0) {
        color[x * 4 + 0] = r.byte();
        color[x * 4 + 1] = r.byte();
        color[x * 4 + 2] = r.byte();
        color[x * 4 + 3] = r.byte();
      } else {
        memcpy(&color[x * 4], &color[(x - 1) * 4], 4);
      }
    }
    for (uint32_t y = 0; y < h; ++y) {
      uint8_t* row = &pixels[static_cast<size_t>(y) * w * ch];
      for (uint32_t x = 0; x < w; ++x)
        store_pixel(row + static_cast<size_t>(x) * ch, &color[x * 4], ch);
    }
    return;
  }

  if (spec.pattern == PAT_GRADIENT) {
    int base[4], dx[4], dy[4];
    for (int c = 0; c < 4; ++c) {
      base[c] = static_cast<int>(r.range(0, 256));
      dx[c] = static_cast<int>(r.range(0, 15)) - 7;
      dy[c] = static_cast<int>(r.range(0, 15)) - 7;
    }
    for (uint32_t y = 0; y < h; ++y) {
      uint8_t* row = &pixels[static_cast<size_t>(y) * w * ch];
      for (uint32_t x = 0; x < w; ++x) {
        for (uint32_t c = 0; c < ch; ++c) {
          const int v = base[c] + static_cast<int>(x) * dx[c] +
                        static_cast<int>(y) * dy[c];
          row[static_cast<size_t>(x) * ch + c] = static_cast<uint8_t>(v & 255);
        }
      }
    }
    return;
  }

  if (spec.pattern == PAT_CHECKER) {
    const uint32_t cell = r.range(1, 65);
    uint8_t c0[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
    uint8_t c1[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
    for (uint32_t y = 0; y < h; ++y) {
      uint8_t* row = &pixels[static_cast<size_t>(y) * w * ch];
      for (uint32_t x = 0; x < w; ++x) {
        const uint8_t* color = (((x / cell) + (y / cell)) & 1) ? c1 : c0;
        store_pixel(row + static_cast<size_t>(x) * ch, color, ch);
      }
    }
    return;
  }

  if (spec.pattern == PAT_RUNS) {
    uint64_t i = 0;
    while (i < total) {
      const uint32_t len = std::min<uint64_t>(r.range(1, 258), total - i);
      uint8_t color[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
      for (uint32_t j = 0; j < len; ++j)
        store_pixel(&pixels[static_cast<size_t>((i + j) * ch)], color, ch);
      i += len;
    }
    return;
  }

  if (spec.pattern == PAT_NOISE_BLOBS) {
    const uint32_t block = r.range(1, 65);
    uint8_t base[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
    for (uint32_t y = 0; y < h; ++y) {
      uint8_t* row = &pixels[static_cast<size_t>(y) * w * ch];
      for (uint32_t x = 0; x < w; ++x) {
        const uint64_t hval = splitmix64(
            pixel_seed ^ (static_cast<uint64_t>(x / block) << 32) ^
            static_cast<uint64_t>(y / block));
        const uint8_t noise = static_cast<uint8_t>(r.byte() & 0x1F);
        for (uint32_t c = 0; c < ch; ++c) {
          const uint8_t v = static_cast<uint8_t>(
              base[c] + static_cast<uint8_t>(hval >> (c * 8)) + noise);
          row[static_cast<size_t>(x) * ch + c] = v;
        }
      }
    }
    return;
  }

  // PAT_BORDER
  const uint32_t max_t = std::max<uint32_t>(1, std::min(w, h) / 2);
  const uint32_t thickness = r.range(1, max_t + 1);
  uint8_t bg[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
  uint8_t fg[4] = {r.byte(), r.byte(), r.byte(), r.byte()};
  for (uint32_t y = 0; y < h; ++y) {
    uint8_t* row = &pixels[static_cast<size_t>(y) * w * ch];
    for (uint32_t x = 0; x < w; ++x) {
      const bool border = (x < thickness) || (y < thickness) ||
                          (x >= w - thickness) || (y >= h - thickness);
      store_pixel(row + static_cast<size_t>(x) * ch, border ? fg : bg, ch);
    }
  }
}

uint64_t count_pixel_mismatches(const uint8_t* a, const uint8_t* b,
                                uint32_t w, uint32_t h, uint32_t ch) {
  const uint64_t total = static_cast<uint64_t>(w) * h;
  uint64_t bad = 0;
  for (uint64_t i = 0; i < total; ++i) {
    const uint8_t* pa = a + static_cast<size_t>(i * ch);
    const uint8_t* pb = b + static_cast<size_t>(i * ch);
    if (memcmp(pa, pb, ch) != 0) ++bad;
  }
  return bad;
}

bool write_file(const std::string& path, const void* data, size_t size) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  const bool ok = fwrite(data, 1, size, f) == size;
  fclose(f);
  return ok;
}

bool validate_case(const CaseSpec& spec, uint64_t pixel_seed, uint64_t index,
                   const std::string& save_dir, bool save_all,
                   FILE* manifest, Stats& stats) {
  const uint32_t w = spec.width;
  const uint32_t h = spec.height;
  const uint32_t ch = spec.channels;
  const uint64_t total_pixels = static_cast<uint64_t>(w) * h;
  const size_t raw_size = static_cast<size_t>(total_pixels * ch);

  ++stats.count;
  stats.total_pixels += total_pixels;
  stats.total_raw_bytes += raw_size;
  stats.min_width = std::min(stats.min_width, w);
  stats.max_width = std::max(stats.max_width, w);
  stats.min_height = std::min(stats.min_height, h);
  stats.max_height = std::max(stats.max_height, h);
  if (ch == 3)
    ++stats.rgb_images;
  else
    ++stats.rgba_images;
  ++stats.flags_count[spec.flags & 3];
  ++stats.pattern_count[spec.pattern];

  std::vector<uint8_t> source;
  generate_pixels(spec, pixel_seed, source);

  std::vector<uint8_t> png1;
  double t0 = now_secs();
  const bool encode_ok = fpng::fpng_encode_image_to_memory(
      source.data(), w, h, ch, png1, spec.flags);
  stats.encode_secs += now_secs() - t0;
  ++stats.encode_calls;

  bool deterministic = false;
  std::vector<uint8_t> png2;
  if (encode_ok) {
    const bool encode2_ok = fpng::fpng_encode_image_to_memory(
        source.data(), w, h, ch, png2, spec.flags);
    ++stats.encode_calls;
    deterministic = encode2_ok && (png1.size() == png2.size()) &&
                    (memcmp(png1.data(), png2.data(), png1.size()) == 0);
  } else {
    ++stats.hard_errors;
  }
  if (encode_ok && !deterministic) ++stats.nondeterministic;

  bool info_ok = false;
  if (encode_ok) {
    uint32_t iw = 0, ih = 0, ic = 0;
    const int ires = fpng::fpng_get_info(png1.data(),
                                         static_cast<uint32_t>(png1.size()),
                                         iw, ih, ic);
    info_ok = (ires == fpng::FPNG_DECODE_SUCCESS) && (iw == w) && (ih == h) &&
              (ic == ch);
    if (!info_ok) {
      ++stats.info_mismatch;
      ++stats.hard_errors;
    }
  }

  bool fpng_ok = false;
  uint64_t fpng_bad_pixels = 0;
  if (encode_ok) {
    std::vector<uint8_t> decoded;
    uint32_t dw = 0, dh = 0, dc = 0;
    t0 = now_secs();
    const int res = fpng::fpng_decode_memory(
        png1.data(), static_cast<uint32_t>(png1.size()), decoded, dw, dh, dc,
        ch);
    stats.fpng_decode_secs += now_secs() - t0;
    ++stats.decode_calls;
    if (res == fpng::FPNG_DECODE_SUCCESS && dw == w && dh == h &&
        dc == ch && decoded.size() == raw_size &&
        memcmp(decoded.data(), source.data(), raw_size) == 0) {
      fpng_ok = true;
    } else {
      ++stats.fpng_decode_failures;
      ++stats.hard_errors;
      if (res == fpng::FPNG_DECODE_SUCCESS && dw == w && dh == h &&
          decoded.size() == raw_size) {
        fpng_bad_pixels =
            count_pixel_mismatches(decoded.data(), source.data(), w, h, ch);
      }
    }
  }

  bool lodepng_ok = false;
  uint64_t lodepng_bad_pixels = 0;
  if (encode_ok) {
    unsigned lw = 0, lh = 0;
    unsigned char* decoded = NULL;
    t0 = now_secs();
    const unsigned lerr = lodepng_decode_memory(
        &decoded, &lw, &lh, png1.data(), png1.size(),
        (ch == 4) ? LCT_RGBA : LCT_RGB, 8);
    stats.lodepng_decode_secs += now_secs() - t0;
    ++stats.decode_calls;
    if (lerr == 0 && lw == w && lh == h &&
        memcmp(decoded, source.data(), raw_size) == 0) {
      lodepng_ok = true;
    } else {
      ++stats.lodepng_decode_failures;
      ++stats.hard_errors;
      if (lerr == 0 && lw == w && lh == h) {
        lodepng_bad_pixels =
            count_pixel_mismatches(decoded, source.data(), w, h, ch);
      }
    }
    free(decoded);
  }

  const uint64_t bad_pixels = fpng_bad_pixels + lodepng_bad_pixels;
  if (bad_pixels) {
    ++stats.pixel_mismatch_cases;
    stats.mismatched_pixels += bad_pixels;
    ++stats.hard_errors;
  }
  if (encode_ok && !deterministic) ++stats.hard_errors;

  const bool pass = encode_ok && deterministic && info_ok && fpng_ok &&
                    lodepng_ok && (bad_pixels == 0);
  const char* status = pass ? "PASS" : "FAIL";

  if (!pass) {
    fprintf(stderr,
            "FAIL index=%" PRIu64 " tag=%s %ux%u c=%u pattern=%s flags=%u "
            "encode=%d det=%d info=%d fpng=%d lodepng=%d bad_px=%" PRIu64 "\n",
            index, spec.tag.c_str(), w, h, ch,
            kPatternNames[spec.pattern], spec.flags, encode_ok ? 1 : 0,
            deterministic ? 1 : 0, info_ok ? 1 : 0, fpng_ok ? 1 : 0,
            lodepng_ok ? 1 : 0, bad_pixels);
  }

  const uint64_t raw_hash = fnv1a64(source.data(), source.size());
  const uint64_t png_hash =
      encode_ok ? fnv1a64(png1.data(), png1.size()) : 0;
  const size_t png_size = encode_ok ? png1.size() : 0;
  if (encode_ok) stats.total_png_bytes += png_size;

  fprintf(manifest,
          "%" PRIu64 ",%s,%u,%u,%u,%s,%u,%zu,%zu,%" PRIu64 ",%" PRIu64
          ",%d,%d,%" PRIu64 ",%d,%s\n",
          index, spec.tag.c_str(), w, h, ch, kPatternNames[spec.pattern],
          spec.flags, png_size, raw_size, raw_hash, png_hash,
          fpng_ok ? 1 : 0, lodepng_ok ? 1 : 0, bad_pixels,
          deterministic ? 1 : 0, status);

  if (!save_dir.empty() && encode_ok && (save_all || index < 32)) {
    const std::string path = save_dir + "/" + spec.tag + ".png";
    if (!write_file(path, png1.data(), png1.size())) {
      fprintf(stderr, "Failed writing sample %s\n", path.c_str());
      ++stats.hard_errors;
    }
  }

  return pass;
}

void print_stats(const char* mode, uint64_t seed, const Stats& s,
                 bool has_seed) {
  printf("mode=%s\n", mode);
  if (has_seed) printf("seed=%" PRIu64 "\n", seed);
  printf("count=%" PRIu64 "\n", s.count);
  printf("hard_errors=%" PRIu64 "\n", s.hard_errors);
  printf("pixel_mismatch_cases=%" PRIu64 "\n", s.pixel_mismatch_cases);
  printf("pixel_mismatch_pixels=%" PRIu64 "\n", s.mismatched_pixels);
  printf("fpng_decode_failures=%" PRIu64 "\n", s.fpng_decode_failures);
  printf("lodepng_decode_failures=%" PRIu64 "\n", s.lodepng_decode_failures);
  printf("info_mismatch=%" PRIu64 "\n", s.info_mismatch);
  printf("nondeterministic=%" PRIu64 "\n", s.nondeterministic);
  printf("encode_calls=%" PRIu64 "\n", s.encode_calls);
  printf("decode_calls=%" PRIu64 "\n", s.decode_calls);
  printf("width_range=%u..%u\n", s.min_width, s.max_width);
  printf("height_range=%u..%u\n", s.min_height, s.max_height);
  printf("rgb_images=%" PRIu64 "\n", s.rgb_images);
  printf("rgba_images=%" PRIu64 "\n", s.rgba_images);
  printf("total_pixels=%" PRIu64 "\n", s.total_pixels);
  printf("total_raw_bytes=%" PRIu64 "\n", s.total_raw_bytes);
  printf("total_png_bytes=%" PRIu64 "\n", s.total_png_bytes);
  if (s.total_raw_bytes)
    printf("ratio=%.4f\n",
           static_cast<double>(s.total_png_bytes) /
               static_cast<double>(s.total_raw_bytes));
  printf("flags[default]=%" PRIu64 " flags[slower]=%" PRIu64
         " flags[uncompressed]=%" PRIu64 "\n",
         s.flags_count[0], s.flags_count[1], s.flags_count[2]);
  for (int p = 0; p < PAT_COUNT; ++p) {
    if (s.pattern_count[p])
      printf("pattern[%s]=%" PRIu64 "\n", kPatternNames[p],
             s.pattern_count[p]);
  }
  printf("elapsed_secs=%.3f\n", s.elapsed_secs);
  printf("encode_secs=%.3f\n", s.encode_secs);
  printf("fpng_decode_secs=%.3f\n", s.fpng_decode_secs);
  printf("lodepng_decode_secs=%.3f\n", s.lodepng_decode_secs);
  if (s.encode_secs)
    printf("encode_mps=%.2f\n",
           static_cast<double>(s.total_pixels) / 1e6 / s.encode_secs);
  if (s.fpng_decode_secs)
    printf("fpng_decode_mps=%.2f\n",
           static_cast<double>(s.total_pixels) / 1e6 / s.fpng_decode_secs);
  if (s.lodepng_decode_secs)
    printf("lodepng_decode_mps=%.2f\n",
           static_cast<double>(s.total_pixels) / 1e6 /
               s.lodepng_decode_secs);
  printf("result=%s\n", (s.hard_errors == 0) ? "PASS" : "FAIL");
}

CaseSpec make_random_spec(uint64_t index, uint64_t seed) {
  Rng r(splitmix64(seed + index * 0x9E3779B97F4A7C15ULL));

  uint32_t max_dim;
  const uint32_t bucket = r.range(0, 100);
  if (bucket < 30)
    max_dim = 64;
  else if (bucket < 65)
    max_dim = 256;
  else if (bucket < 90)
    max_dim = 1024;
  else
    max_dim = 4096;

  uint32_t w = r.range(1, max_dim + 1);
  uint32_t h = r.range(1, max_dim + 1);

  const uint32_t special = r.range(0, 100);
  if (special < 6) {
    w = 1;
  } else if (special < 12) {
    h = 1;
  } else if (special < 16) {
    w = r.range(1, 9);
    h = r.range(1, 9);
  }

  const uint32_t ch = (r.range(0, 2) == 0) ? 3 : 4;
  const Pattern pattern =
      static_cast<Pattern>(r.range(0, static_cast<uint32_t>(PAT_COUNT)));

  const uint32_t f = r.range(0, 100);
  uint32_t flags = 0;
  if (f >= 70 && f < 90)
    flags = fpng::FPNG_ENCODE_SLOWER;
  else if (f >= 90)
    flags = fpng::FPNG_FORCE_UNCOMPRESSED;

  CaseSpec spec;
  char tag[32];
  snprintf(tag, sizeof(tag), "r%04" PRIu64, index);
  spec.tag = tag;
  spec.width = w;
  spec.height = h;
  spec.channels = ch;
  spec.pattern = pattern;
  spec.flags = flags;
  return spec;
}

int run_random(uint32_t count, uint64_t seed, const char* manifest_path,
               const char* sample_dir, bool save_all) {
  FILE* manifest = fopen(manifest_path, "wb");
  if (!manifest) {
    fprintf(stderr, "Failed opening manifest %s\n", manifest_path);
    return 1;
  }
  fprintf(manifest,
          "index,tag,width,height,channels,pattern,flags,png_bytes,raw_bytes,"
          "fnv1a_raw,fnv1a_png,fpng_decode_ok,lodepng_decode_ok,"
          "pixel_mismatch,deterministic,status\n");

  Stats stats;
  const double t0 = now_secs();
  for (uint32_t i = 0; i < count; ++i) {
    const CaseSpec spec = make_random_spec(i, seed);
    const uint64_t pixel_seed =
        splitmix64(seed ^ (static_cast<uint64_t>(i) + 1) *
                              0xD1B54A32D192ED03ULL);
    validate_case(spec, pixel_seed, i, sample_dir ? sample_dir : "", save_all,
                  manifest, stats);
  }
  stats.elapsed_secs = now_secs() - t0;
  fclose(manifest);
  print_stats("random", seed, stats, true);
  return (stats.hard_errors == 0) ? 0 : 1;
}

CaseSpec extreme_case(const char* tag, uint32_t w, uint32_t h, uint32_t ch,
                      Pattern pattern, uint32_t flags) {
  CaseSpec spec;
  spec.tag = tag;
  spec.width = w;
  spec.height = h;
  spec.channels = ch;
  spec.pattern = pattern;
  spec.flags = flags;
  return spec;
}

int run_extreme(const char* manifest_path, const char* sample_dir,
                bool save_all) {
  std::vector<CaseSpec> cases;
  cases.push_back(extreme_case("1x1_rgb", 1, 1, 3, PAT_CONST, 0));
  cases.push_back(extreme_case("1x1_rgba", 1, 1, 4, PAT_GRADIENT, 0));
  cases.push_back(extreme_case("1x2", 1, 2, 4, PAT_RANDOM, 0));
  cases.push_back(extreme_case("2x1", 2, 1, 4, PAT_RANDOM, 0));
  cases.push_back(extreme_case("1x8_strip", 1, 8, 3, PAT_VERT_BANDS, 0));
  cases.push_back(extreme_case("8x1_strip", 8, 1, 3, PAT_HORIZ_BANDS, 0));
  cases.push_back(extreme_case("1x1024_column", 1, 1024, 4, PAT_GRADIENT, 0));
  cases.push_back(extreme_case("1024x1_row", 1024, 1, 4, PAT_GRADIENT, 0));
  cases.push_back(
      extreme_case("1x65536_column", 1, 65536, 3, PAT_RUNS, 0));
  cases.push_back(
      extreme_case("65536x1_row", 65536, 1, 3, PAT_RUNS, 0));
  cases.push_back(extreme_case("1x1048576_column", 1, 1048576, 3,
                               PAT_HORIZ_BANDS, fpng::FPNG_ENCODE_SLOWER));
  cases.push_back(extreme_case("1048576x1_row", 1048576, 1, 3, PAT_VERT_BANDS,
                               fpng::FPNG_FORCE_UNCOMPRESSED));
  cases.push_back(extreme_case("4097x1_odd_row", 4097, 1, 4, PAT_RANDOM, 0));
  cases.push_back(
      extreme_case("1x4097_odd_column", 1, 4097, 4, PAT_RANDOM, 0));
  cases.push_back(extreme_case("3x3", 3, 3, 4, PAT_CHECKER, 0));
  cases.push_back(extreme_case("7x11_odd_dims", 7, 11, 3, PAT_NOISE_BLOBS, 0));
  cases.push_back(
      extreme_case("1920x1080_hd", 1920, 1080, 4, PAT_NOISE_BLOBS, 0));
  cases.push_back(
      extreme_case("2048x2048_square", 2048, 2048, 3, PAT_RANDOM, 0));
  cases.push_back(
      extreme_case("8192x64_ultrawide", 8192, 64, 4, PAT_GRADIENT, 0));
  cases.push_back(
      extreme_case("64x8192_ultratall", 64, 8192, 4, PAT_GRADIENT,
                   fpng::FPNG_ENCODE_SLOWER));
  cases.push_back(
      extreme_case("1x4194304_column", 1, 4194304, 3, PAT_RUNS, 0));
  cases.push_back(extreme_case("4194304x1_row", 4194304, 1, 3, PAT_RUNS, 0));

  FILE* manifest = fopen(manifest_path, "wb");
  if (!manifest) {
    fprintf(stderr, "Failed opening manifest %s\n", manifest_path);
    return 1;
  }
  fprintf(manifest,
          "index,tag,width,height,channels,pattern,flags,png_bytes,raw_bytes,"
          "fnv1a_raw,fnv1a_png,fpng_decode_ok,lodepng_decode_ok,"
          "pixel_mismatch,deterministic,status\n");

  Stats stats;
  const double t0 = now_secs();
  for (size_t i = 0; i < cases.size(); ++i) {
    const uint64_t pixel_seed =
        splitmix64(0xE7E1D2C3B4A59687ULL ^
                   (static_cast<uint64_t>(i) + 1) * 0xD1B54A32D192ED03ULL);
    validate_case(cases[i], pixel_seed, i, sample_dir ? sample_dir : "",
                  save_all, manifest, stats);
  }
  stats.elapsed_secs = now_secs() - t0;
  fclose(manifest);
  print_stats("extreme", 0, stats, false);
  return (stats.hard_errors == 0) ? 0 : 1;
}

int run_invalid(const char* manifest_path) {
  struct InvalidCase {
    const char* tag;
    uint32_t w;
    uint32_t h;
    uint32_t ch;
  };
  const InvalidCase cases[] = {
      {"zero_width", 0, 1, 3},
      {"zero_height", 1, 0, 3},
      {"zero_both", 0, 0, 4},
      {"chans_0", 1, 1, 0},
      {"chans_1", 1, 1, 1},
      {"chans_2", 1, 1, 2},
      {"chans_5", 1, 1, 5},
      {"width_over_max", (1u << 24) + 1, 1, 3},
      {"height_over_max", 1, (1u << 24) + 1, 4},
      {"pixel_count_overflow", 0xFFFFFFFFu, 2, 3},
  };

  FILE* manifest = fopen(manifest_path, "wb");
  if (!manifest) {
    fprintf(stderr, "Failed opening manifest %s\n", manifest_path);
    return 1;
  }
  fprintf(manifest, "index,tag,width,height,channels,encode_rejected,status\n");

  uint64_t failures = 0;
  uint64_t rejected = 0;
  uint8_t dummy[4] = {0, 0, 0, 0};
  const double t0 = now_secs();
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    std::vector<uint8_t> png;
    const bool ok = fpng::fpng_encode_image_to_memory(
        dummy, cases[i].w, cases[i].h, cases[i].ch, png, 0);
    const bool pass = !ok;
    if (!ok) ++rejected;
    if (!pass) ++failures;
    fprintf(manifest, "%zu,%s,%u,%u,%u,%d,%s\n", i, cases[i].tag,
            cases[i].w, cases[i].h, cases[i].ch, ok ? 0 : 1,
            pass ? "PASS" : "FAIL");
    if (!pass)
      fprintf(stderr, "FAIL invalid case %s (encode returned %d)\n",
              cases[i].tag, ok ? 1 : 0);
  }
  fclose(manifest);

  const double elapsed = now_secs() - t0;
  printf("mode=invalid\n");
  printf("cases=%zu\n", sizeof(cases) / sizeof(cases[0]));
  printf("rejected=%" PRIu64 "\n", rejected);
  printf("hard_errors=%" PRIu64 "\n", failures);
  printf("elapsed_secs=%.3f\n", elapsed);
  printf("result=%s\n", failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}

void usage(const char* exe) {
  fprintf(stderr,
          "Usage:\n"
          "  %s random  [--count N] [--seed N] --manifest FILE "
          "[--samples-dir DIR]\n"
          "  %s extreme --manifest FILE [--samples-dir DIR]\n"
          "  %s invalid --manifest FILE\n",
          exe, exe, exe);
}

const char* find_opt(int argc, char** argv, const char* name) {
  for (int i = 2; i + 1 < argc; ++i) {
    if (strcmp(argv[i], name) == 0) return argv[i + 1];
  }
  return NULL;
}

bool has_flag(int argc, char** argv, const char* name) {
  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], name) == 0) return true;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }
  if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage(argv[0]);
    return 0;
  }

  fpng::fpng_init();

  const char* manifest = find_opt(argc, argv, "--manifest");
  const char* samples_dir = find_opt(argc, argv, "--samples-dir");
  const bool save_all = has_flag(argc, argv, "--save-all");
  if (!manifest) {
    fprintf(stderr, "Missing --manifest\n");
    return 1;
  }

  if (strcmp(argv[1], "random") == 0) {
    uint32_t count = 2000;
    uint64_t seed = 20261114;
    const char* count_str = find_opt(argc, argv, "--count");
    const char* seed_str = find_opt(argc, argv, "--seed");
    if (count_str) count = static_cast<uint32_t>(strtoul(count_str, NULL, 10));
    if (seed_str) seed = strtoull(seed_str, NULL, 10);
    return run_random(count, seed, manifest, samples_dir, save_all);
  }
  if (strcmp(argv[1], "extreme") == 0)
    return run_extreme(manifest, samples_dir, save_all);
  if (strcmp(argv[1], "invalid") == 0) return run_invalid(manifest);

  fprintf(stderr, "Unknown mode: %s\n", argv[1]);
  usage(argv[0]);
  return 1;
}
