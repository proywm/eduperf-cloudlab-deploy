#include <vector>
#include <cstddef>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <algorithm>

// ---- Tiny trivial stubs modeling the LLVM types touched by the PHINode case.
// In real LLVM, PHINode derives from User which stores operands in a Use vector;
// addIncoming appends 2 operands (the incoming value and the basic block).
// op_reserve reserves capacity in that operand vector. We model operands as a
// vector<void*>.

struct Value {};                 // stand-in for llvm::Value
struct BasicBlock : Value {};    // stand-in for llvm::BasicBlock
struct Type {};                  // stand-in for llvm::Type

struct PHINode {
  std::vector<void*> Operands;   // models the Use/operand storage
  PHINode(const Type *) {}
  // real semantics: each incoming pair adds two operands
  void addIncoming(Value *V, BasicBlock *BB) {
    Operands.push_back((void*)V);
    Operands.push_back((void*)BB);
  }
  void op_reserve(unsigned N) { Operands.reserve(N); }
};

// Harness helpers mirroring getValue/getBasicBlock: return stable pointers so
// resulting operand pointers can be compared for equivalence.
static std::vector<Value> g_values(4096);
static std::vector<BasicBlock> g_blocks(4096);
static Value *getValue(unsigned /*ty*/, unsigned idx) { return &g_values[idx & 4095]; }
static BasicBlock *getBasicBlock(unsigned idx) { return &g_blocks[idx & 4095]; }

// ------------------------------------------------------------------
// BEFORE: the PHINode-construction block VERBATIM (without op_reserve).
namespace v_before {
  PHINode *build(const Type *InstTy, unsigned RIType, std::vector<unsigned> &Args) {
    // --- verbatim from before.cpp (PHINode case body) ---
    PHINode *PN = new PHINode(InstTy);
    for (unsigned i = 0, e = Args.size(); i != e; i += 2)
      PN->addIncoming(getValue(RIType, Args[i]), getBasicBlock(Args[i+1]));
    return PN;
    // --- end verbatim ---
  }
}

// AFTER: same block VERBATIM WITH op_reserve.
namespace v_after {
  PHINode *build(const Type *InstTy, unsigned RIType, std::vector<unsigned> &Args) {
    // --- verbatim from after.cpp (PHINode case body) ---
    PHINode *PN = new PHINode(InstTy);
    PN->op_reserve(Args.size());
    for (unsigned i = 0, e = Args.size(); i != e; i += 2)
      PN->addIncoming(getValue(RIType, Args[i]), getBasicBlock(Args[i+1]));
    return PN;
    // --- end verbatim ---
  }
}

// ------------------------------------------------------------------
static bool sameResult(PHINode *a, PHINode *b) {
  if (a->Operands.size() != b->Operands.size()) return false;
  for (size_t i = 0; i < a->Operands.size(); ++i)
    if (a->Operands[i] != b->Operands[i]) return false;
  return true;
}

int main() {
  Type InstTy;
  std::mt19937 rng(12345);

  // Differential correctness over a diverse/boundary/adversarial battery.
  // Valid PHINode Args always have EVEN size (pairs). Test sizes 0..2000.
  std::string divergent;
  bool ok = true;
  std::vector<size_t> sizes;
  for (size_t s = 0; s <= 64; s += 2) sizes.push_back(s);
  sizes.push_back(200); sizes.push_back(1000); sizes.push_back(2000);
  for (int rep = 0; rep < 50; ++rep) sizes.push_back((rng() % 1001) * 2);

  for (size_t s : sizes) {
    std::vector<unsigned> Args(s);
    for (auto &x : Args) x = rng() % 4096;
    unsigned tyidx = rng() % 32;
    PHINode *pa = v_after::build(&InstTy, tyidx, Args);
    PHINode *pb = v_before::build(&InstTy, tyidx, Args);
    if (!sameResult(pb, pa)) {
      ok = false;
      divergent = "Args.size()=" + std::to_string(s);
      delete pa; delete pb;
      break;
    }
    delete pa; delete pb;
  }

  printf("EQUIVALENT=%d divergent=%s\n", ok ? 1 : 0, divergent.c_str());

  // ---- Interleaved timing ----
  const int ITERS = 20000;
  std::vector<size_t> tsizes = {2,4,8,16,32,64,128,256,512,1024};
  std::vector<double> ratios;
  for (size_t s : tsizes) {
    std::vector<unsigned> Args(s);
    for (auto &x : Args) x = rng() % 4096;

    // warmup
    for (int i = 0; i < 100; ++i) { delete v_before::build(&InstTy, 1, Args); delete v_after::build(&InstTy, 1, Args); }

    std::vector<long long> bt, at;
    for (int trial = 0; trial < 7; ++trial) {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < ITERS; ++i) { PHINode *p = v_before::build(&InstTy, 1, Args); delete p; }
      auto t1 = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < ITERS; ++i) { PHINode *p = v_after::build(&InstTy, 1, Args); delete p; }
      auto t2 = std::chrono::high_resolution_clock::now();
      bt.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count());
      at.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count());
    }
    std::sort(bt.begin(), bt.end()); std::sort(at.begin(), at.end());
    double r = (double)bt[bt.size()/2] / (double)at[at.size()/2];
    ratios.push_back(r);
    printf("size=%zu before_ns=%lld after_ns=%lld ratio=%.3f\n", s, bt[bt.size()/2], at[at.size()/2], r);
  }
  std::sort(ratios.begin(), ratios.end());
  printf("MEDIAN_SPEEDUP=%.3f\n", ratios[ratios.size()/2]);
  return 0;
}
