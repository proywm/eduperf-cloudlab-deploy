#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <cstdio>
#include <algorithm>

// ---- tiny trivial stubs (faithful to rocksdb) ----
struct Slice {
  const char* data_;
  size_t size_;
  Slice(const char* d, size_t s) : data_(d), size_(s) {}
  const char* data() const { return data_; }
  size_t size() const { return size_; }
};

static inline uint32_t DecodeFixed32(const char* ptr) {
  uint32_t result;
  memcpy(&result, ptr, sizeof(result));  // little-endian host
  return result;
}

#define PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)

// ============ BEFORE ============
namespace v_before {
bool HashMayMatch(const uint32_t& hash,
    const Slice& filter, const size_t& num_probes,
    const uint32_t& num_lines) {
  uint32_t len = static_cast<uint32_t>(filter.size());
  if (len <= 5) return false;

  assert(num_probes != 0);
  assert(num_lines != 0 && (len - 5) % num_lines == 0);
  uint32_t cache_line_size = (len - 5) / num_lines;
  const char* data = filter.data();

  uint32_t h = hash;
  const uint32_t delta = (h >> 17) | (h << 15);
  uint32_t b = (h % num_lines) * (cache_line_size * 8);

  for (uint32_t i = 0; i < num_probes; ++i) {
    const uint32_t bitpos = b + (h % (cache_line_size * 8));
    if (((data[bitpos / 8]) & (1 << (bitpos % 8))) == 0) {
      return false;
    }
    h += delta;
  }
  return true;
}
} // namespace v_before

// ============ AFTER ============
namespace v_after {
bool HashMayMatch(const uint32_t& hash,
    const Slice& filter, const size_t& num_probes,
    const uint32_t& num_lines) {
  uint32_t len = static_cast<uint32_t>(filter.size());
  if (len <= 5) return false;

  assert(num_probes != 0);
  assert(num_lines != 0 && (len - 5) % num_lines == 0);
  uint32_t cache_line_size = (len - 5) / num_lines;
  const char* data = filter.data();

  uint32_t h = hash;
  const uint32_t delta = (h >> 17) | (h << 15);
  uint32_t b = (h % num_lines) * (cache_line_size * 8);
  PREFETCH(&data[b / 8], 0 /* rw */, 1 /* locality */);
  PREFETCH(&data[b / 8 + cache_line_size - 1], 0 /* rw */, 1 /* locality */);

  for (uint32_t i = 0; i < num_probes; ++i) {
    const uint32_t bitpos = b + (h % (cache_line_size * 8));
    if (((data[bitpos / 8]) & (1 << (bitpos % 8))) == 0) {
      return false;
    }
    h += delta;
  }
  return true;
}
} // namespace v_after

// Build a valid filter buffer: num_lines cache lines of cache_line_size bytes,
// then trailing 5 bytes: [num_probes (1 byte)] [num_lines (4 bytes LE)].
static std::string MakeFilter(uint32_t num_lines, uint32_t cache_line_size,
                              uint8_t num_probes, std::mt19937& rng) {
  size_t body = (size_t)num_lines * cache_line_size;
  std::string buf(body + 5, '\0');
  for (size_t i = 0; i < body; ++i) buf[i] = (char)(rng() & 0xFF);
  buf[body] = (char)num_probes;                       // len-5
  uint32_t nl = num_lines;
  memcpy(&buf[body + 1], &nl, 4);                      // len-4..len-1
  return buf;
}

int main() {
  std::mt19937 rng(12345);

  // ---- differential correctness over diverse/boundary/adversarial inputs ----
  long long mismatches = 0;
  std::string firstDiv;
  uint32_t cls[] = {1, 2, 4, 8, 16, 32, 64};   // cache_line_size (2^n typical)
  uint32_t nls[] = {1, 2, 3, 7, 16, 100, 1000};
  uint8_t probes[] = {1, 2, 6, 10, 30};

  for (uint32_t cl : cls)
    for (uint32_t nl : nls)
      for (uint8_t np : probes) {
        std::string f = MakeFilter(nl, cl, np, rng);
        Slice s(f.data(), f.size());
        // many hashes incl boundaries
        std::vector<uint32_t> hashes = {0u, 1u, 0xFFFFFFFFu, 0x80000000u,
                                        0x7FFFFFFFu, 12345u, 0xDEADBEEFu};
        for (int k = 0; k < 200; ++k) hashes.push_back(rng());
        for (uint32_t hsh : hashes) {
          bool rb = v_before::HashMayMatch(hsh, s, np, nl);
          bool ra = v_after::HashMayMatch(hsh, s, np, nl);
          if (rb != ra) {
            if (firstDiv.empty()) {
              char tmp[256];
              snprintf(tmp, sizeof(tmp),
                "cl=%u nl=%u np=%u hash=%u before=%d after=%d",
                cl, nl, (unsigned)np, hsh, rb, ra);
              firstDiv = tmp;
            }
            ++mismatches;
          }
        }
      }

  // also test empty/broken (len<=5)
  {
    std::string tiny(3, 'x');
    Slice s(tiny.data(), tiny.size());
    bool rb = v_before::HashMayMatch(42, s, 1, 1);
    bool ra = v_after::HashMayMatch(42, s, 1, 1);
    if (rb != ra) { ++mismatches; if (firstDiv.empty()) firstDiv="tiny"; }
  }

  printf("MISMATCHES=%lld\n", mismatches);
  if (!firstDiv.empty()) printf("FIRST_DIVERGENT=%s\n", firstDiv.c_str());

  // ---- interleaved timing ----
  // Big filter to stress memory / prefetch.
  uint32_t cl = 64, nl = 200000; uint8_t np = 6;
  std::string big = MakeFilter(nl, cl, np, rng);
  Slice bs(big.data(), big.size());
  const int ITERS = 3;
  std::vector<double> ratios;
  volatile uint64_t sink = 0;
  const int N = 2000000;
  // warmup
  for (int i = 0; i < 100000; ++i) sink += v_before::HashMayMatch(rng(), bs, np, nl);

  std::vector<uint32_t> hs(N);
  for (int i = 0; i < N; ++i) hs[i] = rng();

  for (int t = 0; t < ITERS; ++t) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) sink += v_before::HashMayMatch(hs[i], bs, np, nl);
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) sink += v_after::HashMayMatch(hs[i], bs, np, nl);
    auto t2 = std::chrono::high_resolution_clock::now();
    double bef = std::chrono::duration<double,std::nano>(t1-t0).count();
    double aft = std::chrono::duration<double,std::nano>(t2-t1).count();
    ratios.push_back(bef/aft);
    printf("iter %d before_ns=%.0f after_ns=%.0f ratio=%.3f\n", t, bef, aft, bef/aft);
  }
  std::sort(ratios.begin(), ratios.end());
  printf("MEDIAN_SPEEDUP=%.3f\n", ratios[ratios.size()/2]);
  printf("sink=%llu\n", (unsigned long long)sink);
  return 0;
}
