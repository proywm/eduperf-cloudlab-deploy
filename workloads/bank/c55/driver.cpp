// Differential harness for smp_onHeadersComplete_7bb5c25c9fad
// The real change in onHeadersComplete is the case-insensitive comparison of
// the Expect header value against "100-continue":
//   before: !boost::iequals(expectation, "100-continue")
//   after : !k100Continue.equals(expectation, folly::AsciiCaseInsensitive())
// where k100Continue is a folly::StringPiece{"100-continue"}.
//
// We reproduce both comparison mechanisms faithfully and drive the exact
// predicate used in the changed line. The surrounding adaptor scaffolding
// (upstream_, ResponseBuilder, HTTPMessage) is orthogonal control flow that
// is byte-identical before/after; the only differing computation is this
// string predicate, which we isolate here verbatim.

#include <string>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>

// ---- Faithful stand-ins for the two library facilities used ----

namespace boost {
// boost::iequals: locale-independent-ish ASCII in these tests; boost uses the
// current locale's std::tolower via is_iequal. Both operands are ASCII HTTP
// tokens here. Faithful semantics: equal length AND each char tolower-equal.
inline bool iequals(const std::string& a, const char* blit) {
  std::string b(blit);
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
      return false;
  }
  return true;
}
} // namespace boost

namespace folly {
// AsciiCaseInsensitive comparator, matching folly's implementation:
// lowercases via a branch on 'A'..'Z' (pure ASCII, locale-free).
struct AsciiCaseInsensitive {
  bool operator()(char lhs, char rhs) const {
    auto to_lower = [](char c) -> char {
      return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    };
    return to_lower(lhs) == to_lower(rhs);
  }
};

// Minimal StringPiece with the equals(piece, comparator) overload used.
class StringPiece {
 public:
  constexpr StringPiece(const char* s) : b_(s), e_(s + ce_len(s)) {}
  StringPiece(const std::string& s) : b_(s.data()), e_(s.data() + s.size()) {}

  size_t size() const { return size_t(e_ - b_); }
  const char* begin() const { return b_; }
  const char* end() const { return e_; }

  template <class Cmp>
  bool equals(StringPiece other, Cmp cmp) const {
    if (size() != other.size()) return false;
    return std::equal(begin(), end(), other.begin(), cmp);
  }

 private:
  static constexpr size_t ce_len(const char* s) {
    return *s ? 1 + ce_len(s + 1) : 0;
  }
  const char* b_;
  const char* e_;
};
} // namespace folly

// ---- The exact predicates as they appear in before/after source ----

namespace v_before {
// from before.cpp: if (!boost::iequals(expectation, "100-continue"))
inline bool is_unsupported(const std::string& expectation) {
  return !boost::iequals(expectation, "100-continue");
}
} // namespace v_before

namespace v_after {
// from after.cpp: constexpr StringPiece k100Continue{"100-continue"};
//                 if (!k100Continue.equals(expectation, AsciiCaseInsensitive()))
constexpr folly::StringPiece k100Continue{"100-continue"};
inline bool is_unsupported(const std::string& expectation) {
  return !k100Continue.equals(expectation, folly::AsciiCaseInsensitive());
}
} // namespace v_after

int main() {
  std::vector<std::string> inputs;
  // Direct/boundary cases
  const char* seeds[] = {
    "100-continue", "100-CONTINUE", "100-Continue", "100-cOnTiNuE",
    "", "1", "100", "100-continu", "100-continue ", " 100-continue",
    "100-continue\t", "100_continue", "200-continue", "100-continue100",
    "expect", "0-continue", "100-continueX", "X100-continue",
    "100-CONTINUE", "1OO-continue", "100-continu\0e",
  };
  for (auto* s : seeds) inputs.emplace_back(s);

  // Adversarial: mixed case permutations of the target, near-matches, long strings
  std::mt19937 rng(12345);
  std::string base = "100-continue";
  for (int t = 0; t < 2000; ++t) {
    std::string s = base;
    // random-case each char
    for (auto& c : s) if (rng() & 1) c = (char)std::toupper((unsigned char)c);
    // sometimes mutate: truncate, extend, flip a char
    int m = rng() % 5;
    if (m == 1 && !s.empty()) s.pop_back();
    else if (m == 2) s.push_back((char)('a' + rng() % 26));
    else if (m == 3 && !s.empty()) s[rng() % s.size()] = (char)(rng() % 128);
    else if (m == 4) s.clear();
    inputs.push_back(s);
  }
  // Random arbitrary strings (full byte range, various lengths)
  for (int t = 0; t < 3000; ++t) {
    int len = rng() % 20;
    std::string s;
    for (int i = 0; i < len; ++i) s.push_back((char)(rng() % 256));
    inputs.push_back(s);
  }

  // Differential equivalence check
  std::string divergent;
  bool found_div = false;
  for (auto& in : inputs) {
    bool rb = v_before::is_unsupported(in);
    bool ra = v_after::is_unsupported(in);
    if (rb != ra) {
      found_div = true;
      divergent = in;
      break;
    }
  }

  // Timing: interleaved
  volatile int sink = 0;
  const int reps = 4000;
  std::vector<long long> before_ns, after_ns;
  for (int r = 0; r < reps; ++r) {
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto& in : inputs) sink ^= v_before::is_unsupported(in);
      auto t1 = std::chrono::high_resolution_clock::now();
      before_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }
    {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto& in : inputs) sink ^= v_after::is_unsupported(in);
      auto t1 = std::chrono::high_resolution_clock::now();
      after_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }
  }
  std::sort(before_ns.begin(), before_ns.end());
  std::sort(after_ns.begin(), after_ns.end());
  double mb = before_ns[before_ns.size()/2];
  double ma = after_ns[after_ns.size()/2];

  std::cout << "EQUIVALENT=" << (found_div ? "NO" : "YES") << "\n";
  if (found_div) {
    std::cout << "DIVERGENT_LEN=" << divergent.size() << " BYTES=";
    for (unsigned char c : divergent) std::cout << (int)c << ",";
    std::cout << "\n";
  }
  std::cout << "median_before_ns=" << mb << " median_after_ns=" << ma << "\n";
  std::cout << "SPEEDUP=" << (mb/ma) << "\n";
  std::cout << "sink=" << sink << "\n";
  return 0;
}
