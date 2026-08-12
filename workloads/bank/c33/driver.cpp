// Faithful microbenchmark of the LiveVariables::HandlePhysRegDef change:
//   std::set<unsigned> Live;  ->  SmallSet<unsigned,32> Live;
//
// The changed line only affects the container holding `Live`, used via
// insert / count / erase / empty. We reproduce the EXACT usage pattern of the
// function (insert with subregister fan-out, count-guarded erase fan-out,
// empty check) over a diverse battery, comparing observable behavior + timing.

#include <set>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <chrono>
#include <random>
#include <algorithm>
#include <string>

// ---- Faithful inline-storage SmallVector: N inline slots, no heap alloc while
//      size<=N; grows to heap beyond. Only members used by SmallSet. ----
template <typename T, unsigned N>
class SmallVector {
  T Inline[N];
  T *Data;
  size_t Size;
  size_t Cap;
  void grow(size_t need) {
    size_t nc = Cap ? Cap * 2 : N * 2;
    while (nc < need) nc *= 2;
    T *nd = new T[nc];
    for (size_t i = 0; i < Size; ++i) nd[i] = Data[i];
    if (Data != Inline) delete[] Data;
    Data = nd; Cap = nc;
  }
public:
  SmallVector() : Data(Inline), Size(0), Cap(N) {}
  ~SmallVector() { if (Data != Inline) delete[] Data; }
  typedef T* iterator;
  typedef const T* const_iterator;
  iterator begin() { return Data; }
  iterator end() { return Data + Size; }
  const_iterator begin() const { return Data; }
  const_iterator end() const { return Data + Size; }
  bool empty() const { return Size == 0; }
  size_t size() const { return Size; }
  T &back() { return Data[Size - 1]; }
  void push_back(const T &V) { if (Size == Cap) grow(Size + 1); Data[Size++] = V; }
  void pop_back() { --Size; }
  void clear() { Size = 0; }
  void erase(iterator I) {
    for (iterator J = I + 1, E = end(); J != E; ++J) *(J - 1) = *J;
    --Size;
  }
};

// ---- Verbatim LLVM SmallSet (from the commit's include/llvm/ADT/SmallSet.h) ----
template <typename T, unsigned N>
class SmallSet {
  SmallVector<T, N> Vector;
  std::set<T> Set;
  typedef typename SmallVector<T, N>::const_iterator VIterator;
  typedef typename SmallVector<T, N>::iterator mutable_iterator;
public:
  SmallSet() {}
  bool empty() const { return Vector.empty() && Set.empty(); }
  unsigned size() const { return isSmall() ? Vector.size() : Set.size(); }
  bool count(const T &V) const {
    if (isSmall()) return vfind(V) != Vector.end();
    else return Set.count(V);
  }
  bool insert(const T &V) {
    if (!isSmall()) return Set.insert(V).second;
    VIterator I = vfind(V);
    if (I != Vector.end()) return false;
    if (Vector.size() < N) { Vector.push_back(V); return true; }
    while (!Vector.empty()) { Set.insert(Vector.back()); Vector.pop_back(); }
    Set.insert(V);
    return true;
  }
  bool erase(const T &V) {
    if (!isSmall()) return Set.erase(V);
    for (mutable_iterator I = Vector.begin(), E = Vector.end(); I != E; ++I)
      if (*I == V) { Vector.erase(I); return true; }
    return false;
  }
  void clear() { Vector.clear(); Set.clear(); }
private:
  bool isSmall() const { return Set.empty(); }
  VIterator vfind(const T &V) const {
    for (VIterator I = Vector.begin(), E = Vector.end(); I != E; ++I)
      if (*I == V) return I;
    return Vector.end();
  }
};

// TRI->getSubRegisters(Reg): null-terminated (0-sentinel) sub-register table.
struct RegFile {
  std::vector<std::vector<unsigned>> subs;
  const unsigned *getSubRegisters(unsigned r) const { return subs[r].data(); }
  unsigned numRegs() const { return (unsigned)subs.size(); }
};

// Replay the Live-set manipulation of HandlePhysRegDef for a given Reg,
// parameterized over the set container type.
template <typename SetT>
void replay(const RegFile &RF, unsigned Reg, const std::vector<char> &defined,
            const std::vector<char> &killSubReg, bool &emptyAtEnd,
            std::vector<unsigned> &members) {
  SetT Live;
  if (defined[Reg]) {
    Live.insert(Reg);
    for (const unsigned *SS = RF.getSubRegisters(Reg); *SS; ++SS)
      Live.insert(*SS);
  } else {
    for (const unsigned *SubRegs = RF.getSubRegisters(Reg);
         unsigned SubReg = *SubRegs; ++SubRegs) {
      if (defined[SubReg]) {
        Live.insert(SubReg);
        for (const unsigned *SS = RF.getSubRegisters(SubReg); *SS; ++SS)
          Live.insert(*SS);
      }
    }
  }
  for (const unsigned *SubRegs = RF.getSubRegisters(Reg);
       unsigned SubReg = *SubRegs; ++SubRegs) {
    if (!Live.count(SubReg)) continue;
    if (killSubReg[SubReg]) {
      Live.erase(SubReg);
      for (const unsigned *SS = RF.getSubRegisters(SubReg); *SS; ++SS)
        Live.erase(*SS);
    }
  }
  emptyAtEnd = Live.empty();
  members.clear();
  for (unsigned r = 0; r < RF.numRegs(); ++r)
    if (Live.count(r)) members.push_back(r);
}

RegFile buildRegFile(std::mt19937 &rng, unsigned maxReg, unsigned maxFan) {
  RegFile RF;
  RF.subs.resize(maxReg);
  std::uniform_int_distribution<unsigned> fanD(0, maxFan);
  for (unsigned r = 0; r < maxReg; ++r) {
    unsigned f = fanD(rng);
    std::vector<unsigned> v;
    for (unsigned i = 0; i < f; ++i) {
      unsigned s = 1 + (rng() % (maxReg - 1));
      if (s != r) v.push_back(s);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    v.push_back(0);
    RF.subs[r] = v;
  }
  return RF;
}

int main() {
  std::mt19937 rng(12345);

  // ---- Differential correctness over a large, diverse battery ----
  size_t cases = 0, mism = 0;
  std::string firstDiv;
  for (int trial = 0; trial < 4000; ++trial) {
    unsigned maxReg = 4 + (rng() % 300);
    unsigned maxFan = (trial % 5 == 0) ? (10 + rng() % 60) : (rng() % 8);
    RegFile RF = buildRegFile(rng, maxReg, maxFan);
    std::vector<char> defined(maxReg, 0), kill(maxReg, 0);
    for (unsigned r = 0; r < maxReg; ++r) {
      defined[r] = (rng() % 3 != 0);
      kill[r] = (rng() % 2);
    }
    unsigned Reg = 1 + (rng() % (maxReg - 1));

    bool e1, e2;
    std::vector<unsigned> m1, m2;
    replay<std::set<unsigned>>(RF, Reg, defined, kill, e1, m1);
    replay<SmallSet<unsigned, 32>>(RF, Reg, defined, kill, e2, m2);
    ++cases;
    if (e1 != e2 || m1 != m2) {
      ++mism;
      if (firstDiv.empty()) {
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "trial=%d maxReg=%u maxFan=%u Reg=%u empty(%d vs %d) sizes(%zu vs %zu)",
                 trial, maxReg, maxFan, Reg, (int)e1, (int)e2, m1.size(), m2.size());
        firstDiv = buf;
      }
    }
  }
  printf("EQUIV: cases=%zu mism=%zu\n", cases, mism);
  if (mism) printf("FIRST_DIVERGENCE: %s\n", firstDiv.c_str());

  // ---- Interleaved timing over realistic small-register scenarios ----
  std::mt19937 rng2(999);
  std::vector<RegFile> files;
  std::vector<std::vector<char>> defs, kills;
  std::vector<unsigned> regs;
  const int WORK = 20000;
  for (int i = 0; i < WORK; ++i) {
    unsigned maxReg = 8 + (rng2() % 20);
    RegFile RF = buildRegFile(rng2, maxReg, 6);
    std::vector<char> d(maxReg, 0), k(maxReg, 0);
    for (unsigned r = 0; r < maxReg; ++r) { d[r] = (rng2() % 3 != 0); k[r] = rng2() % 2; }
    files.push_back(std::move(RF));
    defs.push_back(std::move(d));
    kills.push_back(std::move(k));
    regs.push_back(1 + (rng2() % (maxReg - 1)));
  }

  std::vector<double> before_ns, after_ns;
  volatile unsigned sink = 0;
  for (int rep = 0; rep < 7; ++rep) {
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < WORK; ++i) {
        bool e; std::vector<unsigned> m;
        replay<std::set<unsigned>>(files[i], regs[i], defs[i], kills[i], e, m);
        sink += (unsigned)m.size() + e;
      }
      auto t1 = std::chrono::high_resolution_clock::now();
      before_ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / WORK);
    }
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < WORK; ++i) {
        bool e; std::vector<unsigned> m;
        replay<SmallSet<unsigned, 32>>(files[i], regs[i], defs[i], kills[i], e, m);
        sink += (unsigned)m.size() + e;
      }
      auto t1 = std::chrono::high_resolution_clock::now();
      after_ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / WORK);
    }
  }
  std::sort(before_ns.begin(), before_ns.end());
  std::sort(after_ns.begin(), after_ns.end());
  double b = before_ns[before_ns.size()/2];
  double a = after_ns[after_ns.size()/2];
  printf("TIMING: before_ns=%.1f after_ns=%.1f speedup=%.3f sink=%u\n",
         b, a, b / a, (unsigned)sink);
  return 0;
}
