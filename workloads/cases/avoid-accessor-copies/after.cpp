#include "profile.hpp"

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// AFTER: bind the helper's const references directly to existing members.
PERFBANK_NOINLINE std::uint64_t profile_after(const ExplodedNode& node) {
  return build_profile(node.Location, node.State, node.isSink());
}

PERFBANK_NOINLINE std::uint64_t profile_batch_after(
    const std::vector<ExplodedNode>& nodes,
    std::uint64_t calls) {
  std::uint64_t result = 0;
  for (std::uint64_t call = 0; call < calls; ++call) {
    result ^= profile_after(nodes[call % nodes.size()]);
  }
  return result;
}
