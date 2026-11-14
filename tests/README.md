# fpng validation harness

`fpng_validate.cpp` is a deterministic round-trip and determinism test for
the fpng encoder/decoder. It generates images in memory, so the complete
2000-image corpus can be reproduced from a single seed without storing
hundreds of megabytes of PNGs.

## Run

```sh
./tests/run_validation.sh
```

The script builds a release binary and an ASan/UBSan scalar-path binary,
runs the random corpus twice, runs the extreme-dimension corpus twice, checks
invalid arguments, and then repeats the full corpus under ASan/UBSan.

Outputs:

- `artifacts/testset/random_2000_manifest.csv`
- `artifacts/testset/extreme_manifest.csv`
- `artifacts/testset/invalid_manifest.csv`
- `artifacts/testset/samples/random/` (first 32 PNGs)
- `artifacts/testset/samples/extreme/` (all extreme PNGs)
- `artifacts/verify/validation_report.txt`
- `artifacts/verify/manifest_sha256.txt`

To materialize the full random corpus as PNG files:

```sh
./artifacts/bin/fpng_validate random --count 2000 --seed 20261114 \
  --manifest /tmp/random_manifest.csv \
  --samples-dir /tmp/random_pngs --save-all
```

## Test set

- Seed: `20261114`; RNG: `splitmix64`.
- Dimensions: 30% up to 64, 35% up to 256, 25% up to 1024, 10% up to 4096.
- Dimension overrides: 6% width 1, 6% height 1, 4% both up to 8.
- Channels: deterministic 50/50 RGB and RGBA split in this corpus.
- Patterns: `random`, `const`, `horiz_bands`, `vert_bands`, `gradient`,
  `checker`, `runs`, `noise_blobs`, `border`.
- Flags: 70% default, 20% `FPNG_ENCODE_SLOWER`, 10%
  `FPNG_FORCE_UNCOMPRESSED`.

Each manifest row records dimensions, channels, pattern, flags, encoded/raw
sizes, FNV-1a hashes of the source pixels and encoded PNG, decode results,
pixel mismatch count, determinism result, and pass/fail status.

## Checks

For every valid case:

1. Encode twice into independent buffers and compare the PNG bytes.
2. Parse the encoded PNG with `fpng_get_info()`.
3. Decode with fpng and compare every pixel.
4. Decode independently with lodepng and compare every pixel.

The two process-level runs must produce byte-identical manifests. The ASan
run additionally requires a zero exit status and empty sanitizer stderr.
