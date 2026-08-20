#include "mesh.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using TubeFunction = TubeMesh (*)(std::size_t);
volatile std::uint64_t sink = 0;

bool equal(const TubeMesh& left, const TubeMesh& right) {
  return left.vertices == right.vertices
      && left.normals == right.normals
      && left.triangles == right.triangles;
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

double time_function(TubeFunction function, std::size_t faces, int iterations) {
  const auto start = Clock::now();
  for (int iteration = 0; iteration < iterations; ++iteration) {
    const TubeMesh mesh = function(faces);
    sink += mesh.vertices.back() + mesh.triangles.back();
  }
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

int check() {
  const std::vector<std::size_t> inputs = {1, 2, 3, 4, 7, 8, 15, 16, 31, 64, 257, 1024};
  int checked = 0;
  for (const std::size_t faces : inputs) {
    if (!equal(make_tube_before(faces), make_tube_after(faces))) {
      std::cout << "PERFBANK_RESULT mode=check status=fail cases=" << checked << "\n";
      return 1;
    }
    ++checked;
  }
  std::cout << "PERFBANK_RESULT mode=check status=pass cases=" << checked << "\n";
  return 0;
}

int benchmark() {
  constexpr std::size_t faces = 4096;
  int iterations = 1;
  while (iterations < 4096 && time_function(make_tube_before, faces, iterations) < 40'000.0) {
    iterations *= 2;
  }
  constexpr int rounds = 7;
  std::vector<double> before;
  std::vector<double> after;
  std::vector<double> speedups;
  for (int round = 0; round < rounds; ++round) {
    double beforeTime;
    double afterTime;
    if (round % 2 == 0) {
      beforeTime = time_function(make_tube_before, faces, iterations);
      afterTime = time_function(make_tube_after, faces, iterations);
    } else {
      afterTime = time_function(make_tube_after, faces, iterations);
      beforeTime = time_function(make_tube_before, faces, iterations);
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
  constexpr std::size_t faces = 512;
  constexpr int calls = 4096;
  const TubeFunction function = variant == "before" ? make_tube_before : make_tube_after;
  const double elapsed = time_function(function, faces, calls);
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
