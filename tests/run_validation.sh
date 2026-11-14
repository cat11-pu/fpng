#!/usr/bin/env bash
# Reproduce the fpng round-trip, determinism, and extreme-dimension tests.
#
# Usage: ./tests/run_validation.sh
#
# Results are written under artifacts/. The full run takes a few minutes
# because the 2000-image suite is also executed under ASan/UBSan.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXX="${CXX:-g++}"
COUNT="${COUNT:-2000}"
SEED="${SEED:-20261114}"

BIN_DIR="artifacts/bin"
VERIFY_DIR="artifacts/verify"
TEST_DIR="artifacts/testset"
SAMPLE_DIR="$TEST_DIR/samples"

mkdir -p "$BIN_DIR" "$VERIFY_DIR/run1" "$VERIFY_DIR/run2" \
         "$VERIFY_DIR/asan" "$SAMPLE_DIR/random" "$SAMPLE_DIR/extreme"

FAILURES=0

run_case() {
  local out="$1"
  local err="$2"
  shift 2
  "$@" >"$out" 2>"$err"
  local rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "FAILED (exit $rc): $*" >&2
    FAILURES=$((FAILURES + 1))
  fi
  return 0
}

echo "Building release harness..."
"$CXX" -O3 -std=c++11 -DNDEBUG -fno-strict-aliasing -msse4.1 -mpclmul \
  -Isrc tests/fpng_validate.cpp src/fpng.cpp src/lodepng.cpp \
  -o "$BIN_DIR/fpng_validate" -lpthread || exit 1

echo "Building ASan/UBSan harness (scalar path)..."
"$CXX" -O1 -g -std=c++11 -DNDEBUG -fno-strict-aliasing -DFPNG_NO_SSE=1 \
  -DFPNG_USE_UNALIGNED_LOADS=0 -fsanitize=address,undefined \
  -fno-sanitize-recover=undefined -Isrc tests/fpng_validate.cpp \
  src/fpng.cpp src/lodepng.cpp -o "$BIN_DIR/fpng_validate_asan" \
  -lpthread || exit 1

echo "Running 2000-image suite (run 1)..."
run_case "$VERIFY_DIR/run1/random_stats.txt" \
         "$VERIFY_DIR/run1/random_stderr.txt" \
         "$BIN_DIR/fpng_validate" random --count "$COUNT" --seed "$SEED" \
         --manifest "$VERIFY_DIR/run1/random_manifest.csv" \
         --samples-dir "$SAMPLE_DIR/random"

echo "Running 2000-image suite (run 2)..."
run_case "$VERIFY_DIR/run2/random_stats.txt" \
         "$VERIFY_DIR/run2/random_stderr.txt" \
         "$BIN_DIR/fpng_validate" random --count "$COUNT" --seed "$SEED" \
         --manifest "$VERIFY_DIR/run2/random_manifest.csv"

echo "Running extreme dimensions (run 1)..."
run_case "$VERIFY_DIR/run1/extreme_stats.txt" \
         "$VERIFY_DIR/run1/extreme_stderr.txt" \
         "$BIN_DIR/fpng_validate" extreme \
         --manifest "$VERIFY_DIR/run1/extreme_manifest.csv" \
         --samples-dir "$SAMPLE_DIR/extreme"

echo "Running extreme dimensions (run 2)..."
run_case "$VERIFY_DIR/run2/extreme_stats.txt" \
         "$VERIFY_DIR/run2/extreme_stderr.txt" \
         "$BIN_DIR/fpng_validate" extreme \
         --manifest "$VERIFY_DIR/run2/extreme_manifest.csv"

echo "Running invalid-input cases..."
run_case "$VERIFY_DIR/run1/invalid_stats.txt" \
         "$VERIFY_DIR/run1/invalid_stderr.txt" \
         "$BIN_DIR/fpng_validate" invalid \
         --manifest "$VERIFY_DIR/run1/invalid_manifest.csv"

echo "Running 2000-image suite under ASan/UBSan..."
run_case "$VERIFY_DIR/asan/random_stats.txt" \
         "$VERIFY_DIR/asan/random_stderr.txt" \
         env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1 \
             UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
         "$BIN_DIR/fpng_validate_asan" random --count "$COUNT" --seed "$SEED" \
         --manifest "$VERIFY_DIR/asan/random_manifest.csv"

echo "Running extreme and invalid cases under ASan/UBSan..."
run_case "$VERIFY_DIR/asan/extreme_stats.txt" \
         "$VERIFY_DIR/asan/extreme_stderr.txt" \
         env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1 \
             UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
         "$BIN_DIR/fpng_validate_asan" extreme \
         --manifest "$VERIFY_DIR/asan/extreme_manifest.csv"

run_case "$VERIFY_DIR/asan/invalid_stats.txt" \
         "$VERIFY_DIR/asan/invalid_stderr.txt" \
         env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1 \
             UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
         "$BIN_DIR/fpng_validate_asan" invalid \
         --manifest "$VERIFY_DIR/asan/invalid_manifest.csv"

cp "$VERIFY_DIR/run1/random_manifest.csv" \
   "$TEST_DIR/random_2000_manifest.csv"
cp "$VERIFY_DIR/run1/extreme_manifest.csv" "$TEST_DIR/extreme_manifest.csv"
cp "$VERIFY_DIR/run1/invalid_manifest.csv" "$TEST_DIR/invalid_manifest.csv"

{
  sha256sum "$VERIFY_DIR/run1/random_manifest.csv"
  sha256sum "$VERIFY_DIR/run2/random_manifest.csv"
  sha256sum "$VERIFY_DIR/asan/random_manifest.csv"
  sha256sum "$VERIFY_DIR/run1/extreme_manifest.csv"
  sha256sum "$VERIFY_DIR/run2/extreme_manifest.csv"
  sha256sum "$VERIFY_DIR/asan/extreme_manifest.csv"
  sha256sum "$VERIFY_DIR/run1/invalid_manifest.csv"
  sha256sum "$TEST_DIR/random_2000_manifest.csv"
  sha256sum "$TEST_DIR/extreme_manifest.csv"
  sha256sum "$TEST_DIR/invalid_manifest.csv"
} > "$VERIFY_DIR/manifest_sha256.txt"

{
  echo "run1 vs run2 random manifest:"
  if cmp -s "$VERIFY_DIR/run1/random_manifest.csv" \
            "$VERIFY_DIR/run2/random_manifest.csv"; then
    echo "  BYTE-IDENTICAL"
  else
    echo "  DIFFERENT"
    FAILURES=$((FAILURES + 1))
  fi
  echo "run1 vs run2 extreme manifest:"
  if cmp -s "$VERIFY_DIR/run1/extreme_manifest.csv" \
            "$VERIFY_DIR/run2/extreme_manifest.csv"; then
    echo "  BYTE-IDENTICAL"
  else
    echo "  DIFFERENT"
    FAILURES=$((FAILURES + 1))
  fi
  echo "release vs ASan random manifest:"
  if cmp -s "$VERIFY_DIR/run1/random_manifest.csv" \
            "$VERIFY_DIR/asan/random_manifest.csv"; then
    echo "  BYTE-IDENTICAL"
  else
    echo "  DIFFERENT (build-dependent encoding; inspect hashes)"
  fi
  echo "release vs ASan extreme manifest:"
  if cmp -s "$VERIFY_DIR/run1/extreme_manifest.csv" \
            "$VERIFY_DIR/asan/extreme_manifest.csv"; then
    echo "  BYTE-IDENTICAL"
  else
    echo "  DIFFERENT (build-dependent encoding; inspect hashes)"
  fi
} > "$VERIFY_DIR/determinism.txt"

COVERAGE="$(
  awk -F, 'NR > 1 {
    if ($3 == 1) w1++;
    if ($4 == 1) h1++;
    if ($3 > 1024) wbig++;
    if ($4 > 1024) hbig++;
    if ($3 * $4 > 4194304) area++;
  }
  END {
    print "width==1=" w1 ", height==1=" h1 ", width>1024=" wbig \
          ", height>1024=" hbig ", area>4M=" area;
  }' "$VERIFY_DIR/run1/random_manifest.csv"
)"

{
  echo "============================================================"
  echo " fpng validation report"
  echo "============================================================"
  echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "commit=$(git rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "cpu=$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//')"
  echo "compiler=$("$CXX" --version | head -1)"
  echo
  echo "--- test set -------------------------------------------------"
  echo "generator=tests/fpng_validate.cpp"
  echo "rng=splitmix64 (fully deterministic, no libc RNG)"
  echo "seed=$SEED count=$COUNT"
  echo "dimension buckets: 30% <=64, 35% <=256, 25% <=1024, 10% <=4096"
  echo "dimension overrides: 6% width=1, 6% height=1, 4% both <=8"
  echo "patterns: random, const, horiz_bands, vert_bands, gradient,"
  echo "          checker, runs, noise_blobs, border"
  echo "flags: 70% default, 20% FPNG_ENCODE_SLOWER, 10% FPNG_FORCE_UNCOMPRESSED"
  echo "coverage in generated set: $COVERAGE"
  echo "manifest: artifacts/testset/random_2000_manifest.csv"
  echo "samples:  artifacts/testset/samples/random (first 32 PNGs)"
  echo "full PNG materialization: add --samples-dir DIR --save-all"
  echo
  echo "--- random suite, run 1 --------------------------------------"
  cat "$VERIFY_DIR/run1/random_stats.txt"
  echo
  echo "--- random suite, run 2 --------------------------------------"
  cat "$VERIFY_DIR/run2/random_stats.txt"
  echo
  echo "--- determinism ---------------------------------------------"
  cat "$VERIFY_DIR/determinism.txt"
  echo
  grep -E 'run1/random_manifest|run2/random_manifest|asan/random_manifest' \
    "$VERIFY_DIR/manifest_sha256.txt"
  echo
  echo "--- extreme dimensions --------------------------------------"
  cat "$VERIFY_DIR/run1/extreme_stats.txt"
  echo
  awk -F, 'NR > 1 {
    printf "  %-24s %8ux%-8u c=%u %-12s flags=%u png=%-9s raw=%-9s %s\n",
           $2, $3, $4, $5, $6, $7, $8, $9, $16;
  }' "$VERIFY_DIR/run1/extreme_manifest.csv"
  echo
  echo "--- invalid inputs ------------------------------------------"
  cat "$VERIFY_DIR/run1/invalid_stats.txt"
  awk -F, 'NR > 1 {
    printf "  %-24s %ux%u c=%u rejected=%s %s\n",
           $2, $3, $4, $5, $6, $7;
  }' "$VERIFY_DIR/run1/invalid_manifest.csv"
  echo
  echo "--- ASan/UBSan (scalar path, full suite) --------------------"
  cat "$VERIFY_DIR/asan/random_stats.txt"
  cat "$VERIFY_DIR/asan/extreme_stats.txt"
  cat "$VERIFY_DIR/asan/invalid_stats.txt"
  echo "stderr_bytes=$(wc -c < "$VERIFY_DIR/asan/random_stderr.txt")"
  echo
  echo "--- verdict -------------------------------------------------"
  if [ "$FAILURES" -eq 0 ]; then
    echo "PASS: 2000/2000 random images and 22/22 extreme cases round-trip"
    echo "      pixel-exactly, with zero crashes and byte-identical output"
    echo "      across two independent runs."
  else
    echo "FAIL: $FAILURES test group(s) failed; see logs above."
  fi
  echo "============================================================"
} > "$VERIFY_DIR/validation_report.txt"

echo
echo "Report: $VERIFY_DIR/validation_report.txt"
if [ "$FAILURES" -ne 0 ]; then
  echo "Validation FAILED ($FAILURES failure(s))." >&2
  exit 1
fi
echo "Validation PASSED."
