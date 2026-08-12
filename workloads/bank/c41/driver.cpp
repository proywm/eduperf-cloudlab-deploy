#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <string>
#include <unordered_map>

// Faithful stand-in for LLVM's DenseMap<const Value*, unsigned>:
// same semantics for count()/operator[]/find(). The optimization under test
// is purely about how the value map is queried, independent of the map impl.
struct Value {};                 // opaque project type (stub, trivial)
template <class K, class V>
using DenseMap = std::unordered_map<K, V>;

// ---- The ONLY code that changed lives verbatim below, in two variants. ----

namespace v_before {
  // returns (found, reg). found signals the early-return taken.
  inline std::pair<bool, unsigned>
  lookup(DenseMap<const Value *, unsigned> &ValueMap, const Value *V) {
    if (ValueMap.count(V))
      return {true, ValueMap[V]};
    return {false, 0};
  }
}

namespace v_after {
  inline std::pair<bool, unsigned>
  lookup(DenseMap<const Value *, unsigned> &ValueMap, const Value *V) {
    DenseMap<const Value *, unsigned>::iterator I = ValueMap.find(V);
    if (I != ValueMap.end())
      return {true, I->second};
    return {false, 0};
  }
}

int main() {
  std::mt19937_64 rng(12345);

  // Build a pool of Value objects (real distinct pointers, like real Values).
  const int POOL = 4000;
  std::vector<Value> pool(POOL);

  // Differential test over diverse maps: varying densities, hit/miss keys,
  // boundary (empty map, absent key, key mapped to 0), and adversarial reg vals.
  int mismatches = 0;
  std::string firstDiverge;
  for (int trial = 0; trial < 20000; ++trial) {
    DenseMap<const Value *, unsigned> mapB, mapA;
    int nInsert = (int)(rng() % (POOL + 1));
    for (int i = 0; i < nInsert; ++i) {
      int idx = (int)(rng() % POOL);
      unsigned reg = (unsigned)(rng() % 5); // include 0 (adversarial)
      mapB[&pool[idx]] = reg;
      mapA[&pool[idx]] = reg;
    }
    // query keys: mix of present and absent, plus nullptr boundary
    for (int q = 0; q < 8; ++q) {
      const Value *key;
      uint64_t r = rng() % 10;
      if (r == 0) key = nullptr;
      else key = &pool[rng() % POOL];
      auto rb = v_before::lookup(mapB, key);
      auto ra = v_after::lookup(mapA, key);
      if (rb != ra) {
        if (firstDiverge.empty())
          firstDiverge = "trial=" + std::to_string(trial) + " key present=" +
            std::to_string(mapB.count(key));
        ++mismatches;
      }
      // state must be unmodified equivalently (operator[] on before could
      // insert on miss! check map sizes stay in lock-step)
      if (mapB.size() != mapA.size()) {
        if (firstDiverge.empty())
          firstDiverge = "SIZE trial=" + std::to_string(trial) +
            " b=" + std::to_string(mapB.size()) +
            " a=" + std::to_string(mapA.size());
        ++mismatches;
      }
    }
  }
  printf("mismatches=%d firstDiverge=%s\n", mismatches, firstDiverge.c_str());

  // ---- Timing: interleaved before/after ----
  // Realistic workload: ~50% hit rate map queried repeatedly.
  DenseMap<const Value *, unsigned> tmap;
  for (int i = 0; i < POOL; i += 2) tmap[&pool[i]] = (unsigned)(i % 4);
  std::vector<const Value*> keys;
  for (int i = 0; i < 200000; ++i) keys.push_back(&pool[rng() % POOL]);

  std::vector<double> spds;
  volatile unsigned sink = 0;
  for (int rep = 0; rep < 15; ++rep) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto k : keys) { auto r = v_before::lookup(tmap, k); sink += r.second + r.first; }
    auto t1 = std::chrono::high_resolution_clock::now();
    for (auto k : keys) { auto r = v_after::lookup(tmap, k); sink += r.second + r.first; }
    auto t2 = std::chrono::high_resolution_clock::now();
    double bns = std::chrono::duration<double,std::nano>(t1-t0).count();
    double ans = std::chrono::duration<double,std::nano>(t2-t1).count();
    spds.push_back(bns/ans);
  }
  std::sort(spds.begin(), spds.end());
  printf("median_speedup=%.4f sink=%u\n", spds[spds.size()/2], sink);
  return 0;
}
