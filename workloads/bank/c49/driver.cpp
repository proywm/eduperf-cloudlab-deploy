#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

// tiny trivial stub for the project exception type
struct InvalidExpressionException : std::runtime_error {
  explicit InvalidExpressionException(const std::string& s) : std::runtime_error(s) {}
};

namespace v_before {
  static size_t MoveToNextLine(const std::string& str, size_t newline)
  {
    if (str.find("\r\n", newline) == newline)
    {
      return newline + 2;
    }

    if (str.find("\n", newline) == newline)
    {
      return newline + 1;
    }

    if (str.find("\r", newline) == newline)
    {
      return newline + 1;
    }

    std::ostringstream stm;
    stm << "This string seems to contain an invalid line ending at position "
        << newline << ":" << std::endl
        << str << std::endl;
    throw InvalidExpressionException(stm.str());
  }
}

namespace v_after {
  static size_t MoveToNextLine(const std::string& str, size_t newline)
  {
    if (str.find("\r\n", newline) == newline)
    {
      return newline + 2;
    }

    if (str.find('\n', newline) == newline)
    {
      return newline + 1;
    }

    if (str.find('\r', newline) == newline)
    {
      return newline + 1;
    }

    std::ostringstream stm;
    stm << "This string seems to contain an invalid line ending at position "
        << newline << ":" << std::endl
        << str << std::endl;
    throw InvalidExpressionException(stm.str());
  }
}

// evaluate: returns (threw?, value). Value undefined if threw.
struct R { bool threw; size_t val; };
template<size_t (*F)(const std::string&, size_t)>
R eval(const std::string& s, size_t pos) {
  try { return {false, F(s, pos)}; }
  catch (const InvalidExpressionException&) { return {true, 0}; }
}

int main() {
  std::vector<std::pair<std::string,size_t>> cases;

  // boundary / adversarial cases where newline points at various positions
  cases.push_back({"\r\n", 0});
  cases.push_back({"\n", 0});
  cases.push_back({"\r", 0});
  cases.push_back({"abc\ndef", 3});
  cases.push_back({"abc\rdef", 3});
  cases.push_back({"abc\r\ndef", 3});
  cases.push_back({"line1\r\nline2\r\n", 5});
  cases.push_back({"line1\r\nline2\r\n", 12});
  cases.push_back({"x", 0});          // invalid -> throw
  cases.push_back({"abc", 1});        // invalid -> throw
  cases.push_back({"a\nb\rc\r\nd", 1});
  cases.push_back({"a\nb\rc\r\nd", 3});
  cases.push_back({"a\nb\rc\r\nd", 5});
  cases.push_back({"", 0});           // empty -> throw
  cases.push_back({"\r\r", 0});
  cases.push_back({"\r\r", 1});
  cases.push_back({"\n\r", 0});
  cases.push_back({"\n\r", 1});
  cases.push_back({std::string(1000,'a')+"\r\n", 1000});

  // randomized battery
  std::mt19937 rng(12345);
  const char alphabet[] = {'a','b','\r','\n','X','\t',' '};
  for (int i = 0; i < 20000; ++i) {
    int len = rng() % 40;
    std::string s;
    for (int j = 0; j < len; ++j) s += alphabet[rng() % 7];
    size_t pos = len ? (rng() % len) : 0;
    cases.push_back({s, pos});
  }

  bool equivalent = true;
  std::string divergent;
  for (auto& c : cases) {
    R rb = eval<v_before::MoveToNextLine>(c.first, c.second);
    R ra = eval<v_after::MoveToNextLine>(c.first, c.second);
    if (rb.threw != ra.threw || (!rb.threw && rb.val != ra.val)) {
      equivalent = false;
      divergent = "pos=" + std::to_string(c.second) + " str_len=" + std::to_string(c.first.size());
      break;
    }
  }

  std::cout << "equivalent=" << (equivalent ? "YES" : "NO") << "\n";
  if (!equivalent) std::cout << "divergent=" << divergent << "\n";

  // timing: interleaved. Use cases that mostly hit the '\n'/'\r' branches.
  std::vector<std::pair<std::string,size_t>> tcases;
  for (int i = 0; i < 200; ++i) {
    tcases.push_back({std::string(200,'a') + "\nX", 200});
    tcases.push_back({std::string(200,'b') + "\rY", 200});
    tcases.push_back({"\r\n" + std::string(200,'c'), 0});
  }

  const int iters = 4000;
  std::vector<double> ratios;
  volatile size_t sink = 0;
  for (int r = 0; r < 15; ++r) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it)
      for (auto& c : tcases) sink += v_before::MoveToNextLine(c.first, c.second);
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it)
      for (auto& c : tcases) sink += v_after::MoveToNextLine(c.first, c.second);
    auto t2 = std::chrono::high_resolution_clock::now();
    double bns = std::chrono::duration<double,std::nano>(t1-t0).count();
    double ans = std::chrono::duration<double,std::nano>(t2-t1).count();
    ratios.push_back(bns/ans);
  }
  std::sort(ratios.begin(), ratios.end());
  std::cout << "median_speedup=" << ratios[ratios.size()/2] << "\n";
  return 0;
}
