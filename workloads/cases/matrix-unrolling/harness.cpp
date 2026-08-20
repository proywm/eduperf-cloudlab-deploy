#include "matrix.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using MatrixFunction = void (*)(const float*, const float*, float*, int, int, int);
using Clock = std::chrono::steady_clock;

volatile float benchmark_sink = 0.0F;

std::vector<float> make_values(std::size_t count, std::uint32_t seed) {
  std::mt19937 generator(seed);
  std::uniform_real_distribution<float> distribution(-3.0F, 3.0F);
  std::vector<float> values(count);
  std::generate(values.begin(), values.end(), [&] { return distribution(generator); });
  return values;
}

bool same_bits(const std::vector<float>& left, const std::vector<float>& right) {
  return left.size() == right.size()
      && std::memcmp(left.data(), right.data(), left.size() * sizeof(float)) == 0;
}

int check_equivalence() {
  int checked = 0;
  for (int a_rows = 1; a_rows <= 6; ++a_rows) {
    for (int shared = 1; shared <= 13; ++shared) {
      for (int b_columns = 1; b_columns <= 5; ++b_columns) {
        const auto seed = static_cast<std::uint32_t>(
            1000 * a_rows + 100 * shared + b_columns);
        const auto a = make_values(a_rows * shared, seed);
        const auto b = make_values(shared * b_columns, seed + 1);
        std::vector<float> before(a_rows * b_columns);
        std::vector<float> after(a_rows * b_columns);

        matrix_mul_before(
            a.data(), b.data(), before.data(), a_rows, shared, b_columns);
        matrix_mul_after(
            a.data(), b.data(), after.data(), a_rows, shared, b_columns);
        ++checked;

        if (!same_bits(before, after)) {
          std::cerr << "Mismatch for dimensions " << a_rows << "x" << shared
                    << " · " << shared << "x" << b_columns << "\n";
          std::cout << "PERFBANK_RESULT mode=check status=fail cases="
                    << checked << "\n";
          return 1;
        }
      }
    }
  }

  std::cout << "Behavioral-equivalence gate: PASS\n"
            << "Both versions produced bit-identical outputs for " << checked
            << " shared inputs.\n"
            << "The dimensions include shared lengths with remainders 0, 1, 2, and 3.\n"
            << "PERFBANK_RESULT mode=check status=pass cases=" << checked << "\n";
  return 0;
}

std::size_t safe_index(int iterations, std::size_t size) {
  return (static_cast<std::size_t>(iterations) * 17U) % size;
}

double time_function_safe(
    MatrixFunction function,
    const std::vector<float>& a,
    const std::vector<float>& b,
    std::vector<float>& out,
    int a_rows,
    int shared,
    int b_columns,
    int iterations) {
  const auto start = Clock::now();
  for (int iteration = 0; iteration < iterations; ++iteration) {
    function(a.data(), b.data(), out.data(), a_rows, shared, b_columns);
  }
  const auto stop = Clock::now();
  benchmark_sink = benchmark_sink + out[safe_index(iterations, out.size())];
  return std::chrono::duration<double, std::micro>(stop - start).count();
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

int benchmark() {
  // The dRonin rationale discusses small, fixed matrices. An 18x8 · 8x10
  // workload keeps that inner dimension while making each timed call useful.
  constexpr int a_rows = 18;
  constexpr int shared = 8;
  constexpr int b_columns = 10;
  const auto a = make_values(a_rows * shared, 310);
  const auto b = make_values(shared * b_columns, 450);
  std::vector<float> before_output(a_rows * b_columns);
  std::vector<float> after_output(a_rows * b_columns);

  int iterations = 256;
  while (iterations < (1 << 24)) {
    const double elapsed = time_function_safe(
        matrix_mul_before,
        a,
        b,
        before_output,
        a_rows,
        shared,
        b_columns,
        iterations);
    if (elapsed >= 40'000.0) {
      break;
    }
    iterations *= 2;
  }

  constexpr int rounds = 7;
  std::vector<double> before_times;
  std::vector<double> after_times;
  std::vector<double> speedups;
  before_times.reserve(rounds);
  after_times.reserve(rounds);
  speedups.reserve(rounds);

  std::cout << "Local benchmark (-O2, same input, alternating order)\n"
            << "Workload: " << a_rows << "x" << shared << " · " << shared
            << "x" << b_columns << ", " << iterations << " iterations per round\n\n"
            << "round   before (us)   after (us)   speedup\n";

  for (int round = 0; round < rounds; ++round) {
    double before_time;
    double after_time;
    if (round % 2 == 0) {
      before_time = time_function_safe(
          matrix_mul_before, a, b, before_output, a_rows, shared, b_columns, iterations);
      after_time = time_function_safe(
          matrix_mul_after, a, b, after_output, a_rows, shared, b_columns, iterations);
    } else {
      after_time = time_function_safe(
          matrix_mul_after, a, b, after_output, a_rows, shared, b_columns, iterations);
      before_time = time_function_safe(
          matrix_mul_before, a, b, before_output, a_rows, shared, b_columns, iterations);
    }
    before_times.push_back(before_time);
    after_times.push_back(after_time);
    speedups.push_back(before_time / after_time);
    std::cout << std::setw(5) << round + 1 << std::setw(14) << std::fixed
              << std::setprecision(1) << before_time << std::setw(13) << after_time
              << std::setw(10) << std::setprecision(3) << speedups.back() << "x\n";
  }

  const double median_before = median(before_times);
  const double median_after = median(after_times);
  const double median_speedup = median(speedups);
  std::cout << "\nMedian local speedup: " << std::fixed << std::setprecision(3)
            << median_speedup << "x\n"
            << "PerfBank's dedicated-machine result for this case was 1.33x.\n"
            << "A local result may differ with compiler, CPU, load, and frequency scaling.\n"
            << "PERFBANK_RESULT mode=benchmark status=pass rounds=" << rounds
            << " median_speedup=" << median_speedup
            << " before_us=" << std::setprecision(1) << median_before
            << " after_us=" << median_after << " before_samples_us=";
  for (int i = 0; i < rounds; ++i) std::cout << (i ? "," : "") << before_times[i];
  std::cout << " after_samples_us=";
  for (int i = 0; i < rounds; ++i) std::cout << (i ? "," : "") << after_times[i];
  std::cout << " speedup_samples=";
  for (int i = 0; i < rounds; ++i) std::cout << (i ? "," : "") << speedups[i];
  std::cout << "\n";
  return 0;
}

int profile_variant(const std::string& variant) {
  // A fixed workload makes before/after hardware-counter estimates comparable.
  // It is deliberately long enough to produce a useful statistical profile.
  constexpr int a_rows = 18;
  constexpr int shared = 8;
  constexpr int b_columns = 10;
  constexpr int iterations = 1 << 20;
  const auto a = make_values(a_rows * shared, 310);
  const auto b = make_values(shared * b_columns, 450);
  std::vector<float> output(a_rows * b_columns);
  const MatrixFunction function = variant == "before"
      ? matrix_mul_before
      : matrix_mul_after;

  const double elapsed = time_function_safe(
      function,
      a,
      b,
      output,
      a_rows,
      shared,
      b_columns,
      iterations);

  std::cout << "HPCToolkit workload: " << variant << " variant, "
            << iterations << " target-function calls\n"
            << "PERFBANK_RESULT mode=profile-" << variant
            << " status=pass variant=" << variant
            << " calls=" << iterations
            << " iterations=" << iterations
            << " elapsed_us=" << std::fixed << std::setprecision(1)
            << elapsed << "\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string mode = argc == 2 ? argv[1] : "";
  if (mode == "--check") {
    return check_equivalence();
  }
  if (mode == "--benchmark") {
    return benchmark();
  }
  if (mode == "--profile-before") {
    return profile_variant("before");
  }
  if (mode == "--profile-after") {
    return profile_variant("after");
  }
  std::cerr << "Usage: matrix-unrolling --check | --benchmark | "
            << "--profile-before | --profile-after\n";
  return 2;
}
