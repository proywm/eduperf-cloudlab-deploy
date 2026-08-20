#include "profile.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

std::atomic<std::uint64_t> location_copies{0};
std::atomic<std::uint64_t> state_copies{0};

namespace {
using Clock = std::chrono::steady_clock;
using ProfileFunction = std::uint64_t (*)(const ExplodedNode&);
using BatchFunction = std::uint64_t (*)(const std::vector<ExplodedNode>&, std::uint64_t);
volatile std::uint64_t sink = 0;

std::vector<ExplodedNode> make_nodes() {
  std::vector<ExplodedNode> nodes;
  nodes.reserve(2048);
  std::mt19937_64 generator(0xC0FFEE);
  for (int index = 0; index < 2048; ++index) {
    nodes.push_back({
      ProgramPoint(generator(), generator(), generator()),
      ProgramStateRef(reinterpret_cast<const void*>(0x1000ULL + 16ULL * index)),
      (index % 2) != 0,
    });
  }
  return nodes;
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

double time_function(
    ProfileFunction function,
    const std::vector<ExplodedNode>& nodes,
    std::uint64_t calls) {
  const auto start = Clock::now();
  for (std::uint64_t call = 0; call < calls; ++call) {
    sink ^= function(nodes[call % nodes.size()]);
  }
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

int check() {
  const auto nodes = make_nodes();
  int checked = 0;
  for (const auto& node : nodes) {
    if (profile_before(node) != profile_after(node)) {
      std::cout << "PERFBANK_RESULT mode=check status=fail cases=" << checked << "\n";
      return 1;
    }
    ++checked;
  }
  std::cout << "PERFBANK_RESULT mode=check status=pass cases=" << checked << "\n";
  return 0;
}

int benchmark() {
  const auto nodes = make_nodes();
  std::uint64_t calls = 1 << 16;
  while (calls < (1ULL << 28) && time_function(profile_before, nodes, calls) < 40'000.0) {
    calls *= 2;
  }
  constexpr int rounds = 7;
  std::vector<double> before;
  std::vector<double> after;
  std::vector<double> speedups;
  for (int round = 0; round < rounds; ++round) {
    double beforeTime;
    double afterTime;
    if (round % 2 == 0) {
      beforeTime = time_function(profile_before, nodes, calls);
      afterTime = time_function(profile_after, nodes, calls);
    } else {
      afterTime = time_function(profile_after, nodes, calls);
      beforeTime = time_function(profile_before, nodes, calls);
    }
    before.push_back(beforeTime);
    after.push_back(afterTime);
    speedups.push_back(beforeTime / afterTime);
  }
  std::cout << std::fixed << std::setprecision(3)
            << "PERFBANK_RESULT mode=benchmark status=pass rounds=" << rounds
            << " median_speedup=" << median(speedups)
            << " before_us=" << median(before)
            << " after_us=" << median(after) << " before_samples_us=";
  for (int i = 0; i < rounds; ++i) std::cout << (i ? "," : "") << before[i];
  std::cout << " after_samples_us=";
  for (int i = 0; i < rounds; ++i) std::cout << (i ? "," : "") << after[i];
  std::cout << " speedup_samples=";
  for (int i = 0; i < rounds; ++i) std::cout << (i ? "," : "") << speedups[i];
  std::cout << "\n";
  return 0;
}

int profile(const std::string& variant) {
  const auto nodes = make_nodes();
  constexpr std::uint64_t calls = 1ULL << 26;
  const BatchFunction function = variant == "before"
      ? profile_batch_before
      : profile_batch_after;
  const auto start = Clock::now();
  sink ^= function(nodes, calls);
  const double elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
  std::cout << std::fixed << std::setprecision(1)
            << "PERFBANK_RESULT mode=profile-" << variant
            << " status=pass variant=" << variant
            << " calls=" << calls << " iterations=" << calls
            << " elapsed_us=" << elapsed << "\n";
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  const std::string mode = argc == 2 ? argv[1] : "";
  if (mode == "--check") return check();
  if (mode == "--benchmark") return benchmark();
  if (mode == "--profile-before") return profile("before");
  if (mode == "--profile-after") return profile("after");
  return 2;
}
