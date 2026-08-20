#include "io_case.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using WriteFunction = void (*)(const ScaffoldSet&, std::ostream&, std::ostream&);
volatile std::uint64_t sink = 0;

ScaffoldSet input(std::size_t scaffoldCount, std::size_t componentsPerScaffold) {
  ScaffoldSet result(scaffoldCount);
  for (std::size_t scaffold = 0; scaffold < scaffoldCount; ++scaffold) {
    for (std::size_t component = 0; component < componentsPerScaffold; ++component) {
      result[scaffold].push_back({
        scaffold * componentsPerScaffold + component,
        component % 2 == 0 ? '+' : '-',
        80 + static_cast<int>((scaffold * 31 + component * 17) % 900),
        component + 1 == componentsPerScaffold ? 0 : 12 + static_cast<int>(component % 19),
      });
    }
  }
  return result;
}

double time_function(WriteFunction function, const ScaffoldSet& scaffolds, int calls) {
  std::ofstream lengths("/dev/null");
  std::ofstream components("/dev/null");
  const auto start = Clock::now();
  for (int call = 0; call < calls; ++call) {
    function(scaffolds, lengths, components);
    sink += static_cast<std::uint64_t>(call + scaffolds.size());
  }
  lengths.flush();
  components.flush();
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

int check() {
  int checked = 0;
  for (const auto [scaffolds, components] : std::vector<std::pair<int, int>>{
         {0, 0}, {1, 1}, {2, 3}, {7, 9}, {32, 64}}) {
    const ScaffoldSet data = input(scaffolds, components);
    std::ostringstream beforeLengths;
    std::ostringstream beforeComponents;
    std::ostringstream afterLengths;
    std::ostringstream afterComponents;
    write_scaffolds_before(data, beforeLengths, beforeComponents);
    write_scaffolds_after(data, afterLengths, afterComponents);
    if (beforeLengths.str() != afterLengths.str()
        || beforeComponents.str() != afterComponents.str()) {
      std::cout << "PERFBANK_RESULT mode=check status=fail cases=" << checked << "\n";
      return 1;
    }
    ++checked;
  }
  std::cout << "PERFBANK_RESULT mode=check status=pass cases=" << checked << "\n";
  return 0;
}

int benchmark() {
  const ScaffoldSet data = input(24, 48);
  int calls = 1;
  while (calls < 128 && time_function(write_scaffolds_before, data, calls) < 40'000.0) {
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
      beforeTime = time_function(write_scaffolds_before, data, calls);
      afterTime = time_function(write_scaffolds_after, data, calls);
    } else {
      afterTime = time_function(write_scaffolds_after, data, calls);
      beforeTime = time_function(write_scaffolds_before, data, calls);
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
  const ScaffoldSet data = input(32, 64);
  constexpr int calls = 64;
  const WriteFunction function = variant == "before"
    ? write_scaffolds_before
    : write_scaffolds_after;
  const double elapsed = time_function(function, data, calls);
  std::cout << std::fixed << std::setprecision(1)
            << "PERFBANK_RESULT mode=profile-" << variant
            << " status=pass variant=" << variant
            << " calls=" << calls
            << " iterations=" << calls
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
