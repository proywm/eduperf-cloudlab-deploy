#include "profile.hpp"

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// BEFORE: both accessors return by value, constructing two temporaries.
PERFBANK_NOINLINE std::uint64_t profile_before(const ExplodedNode& node) {
  return build_profile(node.getLocation(), node.getState(), node.isSink());
}

PERFBANK_NOINLINE std::uint64_t profile_batch_before(
    const std::vector<ExplodedNode>& nodes,
    std::uint64_t calls) {
  std::uint64_t result = 0;
  for (std::uint64_t call = 0; call < calls; ++call) {
    result ^= profile_before(nodes[call % nodes.size()]);
  }
  return result;
}
