#include "directory.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

std::string lowercase(std::string value) {
  for (char& character : value) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return value;
}

bool same_name(const std::string& left, const std::string& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto a = static_cast<unsigned char>(left[index]);
    const auto b = static_cast<unsigned char>(right[index]);
    if (std::tolower(a) != std::tolower(b)) return false;
  }
  return true;
}

namespace {
using Clock = std::chrono::steady_clock;
using AddFunction = bool (*)(PakDirectory&, const std::string&);
volatile std::size_t sink = 0;

std::vector<std::string> unique_names(std::size_t count) {
  std::vector<std::string> names;
  names.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    names.push_back("data/level/file_" + std::to_string(index) + ".pak");
  }
  return names;
}

std::string fingerprint(AddFunction function, const std::vector<std::string>& names) {
  PakDirectory directory;
  std::string accepted;
  for (const auto& name : names) accepted += function(directory, name) ? '1' : '0';
  accepted += "|" + std::to_string(directory.files.size());
  for (const auto& file : directory.files) accepted += "|" + file.name;
  accepted += "|map=" + std::to_string(directory.filesMap.size());
  return accepted;
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

double time_workload(
    AddFunction function,
    const std::vector<std::string>& names,
    int repetitions) {
  const auto start = Clock::now();
  for (int repetition = 0; repetition < repetitions; ++repetition) {
    PakDirectory directory;
    for (const auto& name : names) function(directory, name);
    sink += directory.files.size();
  }
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

int check() {
  const std::vector<std::vector<std::string>> cases = {
    {}, {"a"}, {"a", "a"}, {"a", "A"},
    {"readme.txt", "README.TXT", "other"},
    {"file1", "file2", "file1", "file3"},
    {"data/a", "data/b", "DATA/A", "data/c"},
    unique_names(128),
  };
  int checked = 0;
  for (const auto& names : cases) {
    if (fingerprint(add_file_before, names) != fingerprint(add_file_after, names)) {
      std::cout << "PERFBANK_RESULT mode=check status=fail cases=" << checked << "\n";
      return 1;
    }
    ++checked;
  }
  std::cout << "PERFBANK_RESULT mode=check status=pass cases=" << checked << "\n";
  return 0;
}

int benchmark() {
  const auto names = unique_names(320);
  int repetitions = 1;
  while (repetitions < 64
      && time_workload(add_file_before, names, repetitions) < 40'000.0) {
    repetitions *= 2;
  }
  constexpr int rounds = 7;
  std::vector<double> before;
  std::vector<double> after;
  std::vector<double> speedups;
  for (int round = 0; round < rounds; ++round) {
    double beforeTime;
    double afterTime;
    if (round % 2 == 0) {
      beforeTime = time_workload(add_file_before, names, repetitions);
      afterTime = time_workload(add_file_after, names, repetitions);
    } else {
      afterTime = time_workload(add_file_after, names, repetitions);
      beforeTime = time_workload(add_file_before, names, repetitions);
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
  constexpr int repetitions = 4096;
  const auto names = unique_names(256);
  const AddFunction function = variant == "before" ? add_file_before : add_file_after;
  const double elapsed = time_workload(function, names, repetitions);
  constexpr std::size_t calls = repetitions * 256;
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
