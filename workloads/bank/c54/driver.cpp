#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <random>

typedef uint32_t uint;

// Tiny trivial stubs for project types/functions.
struct KernelGlobals { const uint* directions; int dir_len; };

// find_first_set: returns 1-based index of least-significant set bit, 0 if x==0.
static inline int find_first_set(uint x) {
  if (x == 0) return 0;
  return __builtin_ffs((int)x); // 1-based; 0 if zero
}

#define SOBOL_SKIP 64

// Stub for kernel_tex_fetch(__sobol_directions, idx): fetch from kg's array.
// We make it a macro-like access via a global kg pointer to keep signatures verbatim.
static const KernelGlobals* g_kg = nullptr;
#define kernel_tex_fetch(tex, idx) (g_kg->directions[(idx)])
#define __sobol_directions 0 /* unused token */

namespace v_before {
uint sobol_dimension(KernelGlobals *kg, int index, int dimension)
{
  uint result = 0;
  uint i = index + SOBOL_SKIP;
  for (uint j = 0; i; i >>= 1, j++) {
    if (i & 1) {
      result ^= kernel_tex_fetch(__sobol_directions, 32 * dimension + j);
    }
  }
  return result;
}
}

namespace v_after {
uint sobol_dimension(KernelGlobals *kg, int index, int dimension)
{
  uint result = 0;
  uint i = index + SOBOL_SKIP;
  for (int j = 0, x; (x = find_first_set(i)); i >>= x) {
    j += x;
    result ^= kernel_tex_fetch(__sobol_directions, 32 * dimension + j - 1);
  }
  return result;
}
}

int main() {
  // Build a large directions table so 32*dimension + j never overflows.
  // dimension up to say 200, j up to 32 -> need >= 200*32+32.
  std::vector<uint> dirs(32 * 300 + 64);
  std::mt19937 rng(12345);
  for (auto &d : dirs) d = rng();
  KernelGlobals kg{ dirs.data(), (int)dirs.size() };
  g_kg = &kg;

  // Differential test over diverse inputs.
  bool mismatch = false;
  long long firstBadIndex = 0, firstBadDim = 0;
  std::vector<int> indices;
  for (int idx = 0; idx <= 5000; ++idx) indices.push_back(idx);
  // boundary/adversarial values
  for (int e = 0; e < 31; ++e) { indices.push_back((1<<e)); indices.push_back((1<<e)-1); indices.push_back((1<<e)+1); }
  indices.push_back(0); indices.push_back(1);
  // large values that keep index+64 within positive int range
  for (int v = 2000000000; v > 1900000000; v -= 7777777) indices.push_back(v);

  for (int dim = 0; dim < 200; ++dim) {
    for (int idx : indices) {
      uint a = v_before::sobol_dimension(&kg, idx, dim);
      uint b = v_after::sobol_dimension(&kg, idx, dim);
      if (a != b) {
        if (!mismatch) { firstBadIndex = idx; firstBadDim = dim; }
        mismatch = true;
      }
    }
  }
  if (mismatch) {
    printf("MISMATCH first at index=%lld dim=%lld\n", firstBadIndex, firstBadDim);
  } else {
    printf("EQUIVALENT over all tested inputs\n");
  }

  // Timing: interleaved before/after.
  const int REP = 200;
  std::vector<long long> beforeNs, afterNs;
  volatile uint sink = 0;
  for (int r = 0; r < REP; ++r) {
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      uint acc = 0;
      for (int dim = 0; dim < 200; ++dim)
        for (int idx = 0; idx <= 5000; ++idx)
          acc ^= v_before::sobol_dimension(&kg, idx, dim);
      auto t1 = std::chrono::high_resolution_clock::now();
      sink ^= acc;
      beforeNs.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count());
    }
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      uint acc = 0;
      for (int dim = 0; dim < 200; ++dim)
        for (int idx = 0; idx <= 5000; ++idx)
          acc ^= v_after::sobol_dimension(&kg, idx, dim);
      auto t1 = std::chrono::high_resolution_clock::now();
      sink ^= acc;
      afterNs.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count());
    }
  }
  std::sort(beforeNs.begin(), beforeNs.end());
  std::sort(afterNs.begin(), afterNs.end());
  double mb = beforeNs[REP/2], ma = afterNs[REP/2];
  printf("median before=%.0f ns after=%.0f ns speedup=%.3f sink=%u\n", mb, ma, mb/ma, (unsigned)sink);
  return 0;
}
