// Faithful self-contained harness for smp_Profile_fbe4d36f1f83
//
// Real change (change.diff), in ExplodedNode::Profile(FoldingSetNodeID&):
//   before:  Profile(ID, getLocation(), getState(), isSink());
//   after:   Profile(ID, Location, State, isSink());
//
// getLocation() returns ProgramPoint BY VALUE (copy ctor);
// getState()    returns ProgramStateRef BY VALUE (intrusive refcount bump/drop).
// The static Profile takes both by const-reference. So the before-form builds
// two temporaries that are immediately destroyed; the after-form binds the
// members directly. Semantics identical; after avoids the copies.
//
// We model ProgramPoint / ProgramStateRef / FoldingSetNodeID faithfully enough
// to (a) reproduce the copy costs and (b) let the FoldingSet id be observable,
// so the differential test proves the two Profile()s produce identical ids.

#include <atomic>
#include <algorithm>
#include <string>
#include <cstdint>
#include <vector>
#include <cstdio>
#include <chrono>
#include <random>
#include <cassert>

// ----- faithful-ish stand-ins for project types --------------------------

// llvm::FoldingSetNodeID: accumulates a sequence of ints; used as an equality
// key. We expose the accumulated bits so the test can compare ids.
// Fixed-capacity, no-heap accumulator (LLVM's FoldingSetNodeID uses a
// SmallVector with inline storage; Profile only appends a handful of entries,
// so the real thing does not heap-allocate here). Using inline storage keeps
// the measured cost focused on the Profile call itself rather than allocator
// noise, which is faithful to the real small-fixed-size id built by Profile.
struct FoldingSetNodeID {
  uint64_t Bits[8];
  unsigned N = 0;
  void Add(uint64_t v)        { Bits[N++] = 0x1000000000000000ULL ^ v; }
  void AddPointer(const void* p) { Bits[N++] = reinterpret_cast<uint64_t>(p); }
  void AddBoolean(bool b)     { Bits[N++] = b ? 0xB1u : 0xB0u; }
  uint64_t back() const { return Bits[N-1]; }
  bool operator==(const FoldingSetNodeID& o) const {
    if (N != o.N) return false;
    for (unsigned i=0;i<N;i++) if (Bits[i]!=o.Bits[i]) return false;
    return true;
  }
};

// ProgramPoint: a non-trivial value type. Its copy constructor does real work
// (LLVM's ProgramPoint holds several pointers/kind fields; copying is not free).
// We give it a small payload plus a copy-ctor side effect on a global counter
// to make copies observable, and Add() folds the value into the id.
static std::atomic<long> g_pp_copies{0};
struct ProgramPoint {
  uintptr_t data[3];
  ProgramPoint() { data[0]=data[1]=data[2]=0; }
  ProgramPoint(uintptr_t a, uintptr_t b, uintptr_t c) { data[0]=a; data[1]=b; data[2]=c; }
  ProgramPoint(const ProgramPoint& o) {
    g_pp_copies.fetch_add(1, std::memory_order_relaxed);
    data[0]=o.data[0]; data[1]=o.data[1]; data[2]=o.data[2];
  }
  ProgramPoint& operator=(const ProgramPoint&) = default;
  uint64_t fold() const {
    return static_cast<uint64_t>(data[0]*1315423911u + data[1]*2654435761u + data[2]);
  }
};
// FoldingSetNodeID::Add(ProgramPoint) overload
static inline void AddPP(FoldingSetNodeID& ID, const ProgramPoint& L) { ID.Add(L.fold()); }

// The dummy state object ProgramStateRef points at (intrusively refcounted).
static std::atomic<long> g_state_incr{0};
struct ProgramStateImpl {
  mutable std::atomic<unsigned> refcnt{0};
  int payload;
  explicit ProgramStateImpl(int p): payload(p) {}
};
// ProgramStateRef: intrusive_ref_cnt_ptr. Copy => atomic increment; dtor => decrement.
struct ProgramStateRef {
  ProgramStateImpl* Obj = nullptr;
  ProgramStateRef() = default;
  explicit ProgramStateRef(ProgramStateImpl* o): Obj(o) { retain(); }
  ProgramStateRef(const ProgramStateRef& o): Obj(o.Obj) { retain(); }
  ProgramStateRef& operator=(const ProgramStateRef& o) {
    if (this != &o) { release(); Obj = o.Obj; retain(); }
    return *this;
  }
  ~ProgramStateRef() { release(); }
  void retain() { if (Obj) { g_state_incr.fetch_add(1, std::memory_order_relaxed);
                             Obj->refcnt.fetch_add(1, std::memory_order_acq_rel); } }
  void release() { if (Obj) Obj->refcnt.fetch_sub(1, std::memory_order_acq_rel); }
  ProgramStateImpl* getPtr() const { return Obj; }
};

// ----- the class under test, two versions -------------------------------
// Everything below is shared framing; only the one Profile() line differs,
// exactly matching before.cpp / after.cpp.

#define MAKE_NODE_VERSION(NS, PROFILE_BODY)                                     \
namespace NS {                                                                  \
class ExplodedNode {                                                            \
  const ProgramPoint Location;                                                  \
  ProgramStateRef State;                                                        \
  bool Sink;                                                                    \
public:                                                                         \
  ExplodedNode(const ProgramPoint& loc, ProgramStateRef state, bool IsSink)     \
    : Location(loc), State(state), Sink(IsSink) {}                              \
  ProgramPoint getLocation() const { return Location; }                        \
  ProgramStateRef getState() const { return State; }                           \
  bool isSink() const { return Sink; }                                         \
  static void Profile(FoldingSetNodeID &ID, const ProgramPoint &Loc,           \
                      const ProgramStateRef &state, bool IsSink) {              \
    AddPP(ID, Loc);                                                            \
    ID.AddPointer(state.getPtr());                                            \
    ID.AddBoolean(IsSink);                                                     \
  }                                                                            \
  void Profile(FoldingSetNodeID& ID) const {                                   \
    PROFILE_BODY                                                               \
  }                                                                            \
};                                                                             \
}

// before: uses accessors (copies)
MAKE_NODE_VERSION(v_before, Profile(ID, getLocation(), getState(), isSink());)
// after: direct member access (no copies)
MAKE_NODE_VERSION(v_after,  Profile(ID, Location, State, isSink());)

// ----- differential test + timing ---------------------------------------

int main() {
  std::mt19937_64 rng(0xC0FFEE);

  // Build a diverse/boundary/adversarial battery of node inputs.
  struct Input { ProgramPoint pp; int stateVal; bool sink; };
  std::vector<Input> inputs;
  auto push = [&](uintptr_t a, uintptr_t b, uintptr_t c, int sv, bool sk){
    inputs.push_back({ProgramPoint(a,b,c), sv, sk});
  };
  // boundaries
  push(0,0,0,0,false); push(0,0,0,0,true);
  push(~0ull,~0ull,~0ull,-1,true); push(~0ull,~0ull,~0ull,0,false);
  push(1,2,3,7,false); push(3,2,1,7,true);
  // adversarial: values that could collide under a weak fold
  push(2654435761u,0,0,1,false); push(0,1315423911u,0,1,true);
  // random fill
  for (int i=0;i<2000;i++)
    push(rng(), rng(), rng(), (int)rng(), (rng()&1));

  // Shared state objects (pointer identity is what Profile folds via getPtr()).
  std::vector<ProgramStateImpl*> states;
  for (int i=0;i<64;i++) states.push_back(new ProgramStateImpl(i));

  bool equivalent = true;
  std::string firstDiverge;

  for (size_t i=0;i<inputs.size() && equivalent;i++) {
    ProgramStateImpl* st = states[i % states.size()];
    // occasionally use null state (getPtr()==nullptr path)
    ProgramStateRef sref = (i % 37 == 0) ? ProgramStateRef() : ProgramStateRef(st);

    v_before::ExplodedNode nb(inputs[i].pp, sref, inputs[i].sink);
    v_after::ExplodedNode  na(inputs[i].pp, sref, inputs[i].sink);

    FoldingSetNodeID idB, idA;
    nb.Profile(idB);
    na.Profile(idA);
    if (!(idB == idA)) {
      equivalent = false;
      char buf[128];
      std::snprintf(buf,sizeof buf,"input#%zu sink=%d stateNull=%d",
                    i, (int)inputs[i].sink, (int)(sref.getPtr()==nullptr));
      firstDiverge = buf;
    }
  }

  printf("EQUIVALENT=%d\n", (int)equivalent);
  if (!equivalent) printf("DIVERGE=%s\n", firstDiverge.c_str());

  // ---- timing: interleaved before/after ----
  const int REPS = 4000;
  std::vector<long long> tb, ta;
  // Build a set of nodes once (construction cost excluded from the measured loop).
  std::vector<v_before::ExplodedNode> nodesB;
  std::vector<v_after::ExplodedNode>  nodesA;
  nodesB.reserve(inputs.size()); nodesA.reserve(inputs.size());
  for (size_t i=0;i<inputs.size();i++) {
    ProgramStateImpl* st = states[i % states.size()];
    ProgramStateRef sref(st);
    nodesB.emplace_back(inputs[i].pp, sref, inputs[i].sink);
    nodesA.emplace_back(inputs[i].pp, sref, inputs[i].sink);
  }

  volatile uint64_t sink = 0;
  for (int r=0;r<REPS;r++) {
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto& n : nodesB) { FoldingSetNodeID id; n.Profile(id); sink ^= id.back(); }
      auto t1 = std::chrono::high_resolution_clock::now();
      tb.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count());
    }
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto& n : nodesA) { FoldingSetNodeID id; n.Profile(id); sink ^= id.back(); }
      auto t1 = std::chrono::high_resolution_clock::now();
      ta.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count());
    }
  }
  std::sort(tb.begin(), tb.end());
  std::sort(ta.begin(), ta.end());
  double medB = tb[tb.size()/2], medA = ta[ta.size()/2];
  printf("MED_BEFORE_NS=%.1f MED_AFTER_NS=%.1f SPEEDUP=%.4f\n", medB, medA, medB/medA);
  printf("pp_copies=%ld state_incr=%ld sink=%llu\n",
         g_pp_copies.load(), g_state_incr.load(), (unsigned long long)sink);
  return 0;
}
