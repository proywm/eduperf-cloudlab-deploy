#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

// Tiny stub for the missing project type: a Function pointer key.
struct Function { std::string name; };

// A map container standing in for LLVM's RegMasks (Function* -> vector<uint32_t>).
// It preserves the essential interface used by getRegUsageInfo: find() and end().
using RegMaskMap = std::unordered_map<const Function*, std::vector<uint32_t>>;

// ---- Class shells replicating just enough context ----
struct Base_before {
  RegMaskMap RegMasks;
  const std::vector<uint32_t> *getRegUsageInfo(const Function *FP);
};
struct Base_after {
  RegMaskMap RegMasks;
  const std::vector<uint32_t> *getRegUsageInfo(const Function *FP);
};

namespace v_before {
// VERBATIM body from before.cpp
const std::vector<uint32_t> *
getRegUsageInfo_impl(RegMaskMap &RegMasks, const Function *FP) {
  if (RegMasks.find(FP) != RegMasks.end())
    return &(RegMasks.find(FP)->second);
  return nullptr;
}
}

namespace v_after {
// VERBATIM body from after.cpp
const std::vector<uint32_t> *
getRegUsageInfo_impl(RegMaskMap &RegMasks, const Function *FP) {
  auto It = RegMasks.find(FP);
  if (It != RegMasks.end())
    return &(It->second);
  return nullptr;
}
}

int main() {
  std::mt19937_64 rng(12345);

  // Build a diverse map: many functions, some present, some absent.
  const int N = 2000;
  std::vector<Function> funcs(N);
  for (int i = 0; i < N; ++i) funcs[i].name = "f" + std::to_string(i);

  RegMaskMap map;
  // Insert only even-indexed functions.
  for (int i = 0; i < N; ++i) {
    if (i % 2 == 0) {
      std::vector<uint32_t> v = { (uint32_t)i, (uint32_t)(i*7+1), (uint32_t)(i^0xdeadbeef) };
      map[&funcs[i]] = v;
    }
  }
  // Also test nullptr key and a dangling/absent pointer.
  Function extern1{"ext"};
  const Function* nullkey = nullptr;

  // ---- Differential correctness ----
  bool ok = true;
  std::string divergent;
  auto check = [&](const Function* fp, const std::string &label){
    const std::vector<uint32_t>* b = v_before::getRegUsageInfo_impl(map, fp);
    const std::vector<uint32_t>* a = v_after::getRegUsageInfo_impl(map, fp);
    bool bothNull = (b==nullptr && a==nullptr);
    bool bothNonNull = (b!=nullptr && a!=nullptr && *b == *a && b == a);
    if (!(bothNull || bothNonNull)) {
      if (ok) divergent = label;
      ok = false;
    }
  };
  for (int i = 0; i < N; ++i) check(&funcs[i], "func_"+std::to_string(i));
  check(nullkey, "nullkey");
  check(&extern1, "absent_extern");
  // adversarial repeats
  for (int r = 0; r < 100; ++r) {
    int i = rng() % N;
    check(&funcs[i], "rand_"+std::to_string(i));
  }

  std::cout << "EQUIVALENT=" << (ok?1:0);
  if (!ok) std::cout << " DIVERGENT=" << divergent;
  std::cout << "\n";

  // ---- Timing: interleaved ----
  // Query pattern that heavily exercises present keys (the find hits, so before does 2 lookups).
  std::vector<const Function*> queries;
  for (int r = 0; r < 200000; ++r) queries.push_back(&funcs[(rng()%N)]);

  volatile uint64_t sink = 0;
  const int reps = 40;
  std::vector<double> ratios;
  for (int rep = 0; rep < reps; ++rep) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto q : queries) { auto p = v_before::getRegUsageInfo_impl(map, q); if (p) sink += (*p)[0]; }
    auto t1 = std::chrono::high_resolution_clock::now();
    for (auto q : queries) { auto p = v_after::getRegUsageInfo_impl(map, q); if (p) sink += (*p)[0]; }
    auto t2 = std::chrono::high_resolution_clock::now();
    double bns = std::chrono::duration<double,std::nano>(t1-t0).count();
    double ans = std::chrono::duration<double,std::nano>(t2-t1).count();
    ratios.push_back(bns/ans);
  }
  std::sort(ratios.begin(), ratios.end());
  double med = ratios[ratios.size()/2];
  std::cout << "SPEEDUP_MEDIAN=" << med << " sink=" << sink << "\n";
  return 0;
}
