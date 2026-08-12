#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

// The optimization: replacing s.find(prefix) == 0 (searches whole string)
// with s.rfind(prefix, 0) == 0 (only checks position 0) for prefix tests.

namespace v_before {
    // Returns pair<isSession, isNotError-like flags> replicating the two checks.
    // We model the two boolean predicates that the diff changed.
    bool starts_with_session(const std::string& sid) {
        return sid.find("SESSION-ID=") == 0;
    }
    bool not_starts_with_error(const std::string& sid) {
        return sid.find("ERROR") != 0;
    }
}

namespace v_after {
    bool starts_with_session(const std::string& sid) {
        return sid.rfind("SESSION-ID=", 0) == 0;
    }
    bool not_starts_with_error(const std::string& sid) {
        return sid.rfind("ERROR", 0) != 0;
    }
}

int main() {
    std::vector<std::string> inputs;
    // Boundary / adversarial cases
    inputs.push_back("");
    inputs.push_back("SESSION-ID=");
    inputs.push_back("SESSION-ID=abc123");
    inputs.push_back("SESSION-ID");
    inputs.push_back("session-id=abc"); // case mismatch
    inputs.push_back("xSESSION-ID=abc"); // prefix not at 0
    inputs.push_back("ERROR");
    inputs.push_back("ERROR: something");
    inputs.push_back("xERROR");
    inputs.push_back("some ERROR here");
    inputs.push_back("SESSION-ID=ERROR"); // both
    inputs.push_back("E");
    inputs.push_back("SESSION-ID=SESSION-ID=");
    inputs.push_back(std::string(1000, 'S'));
    inputs.push_back("SESSION-ID=" + std::string(1000, 'x'));
    inputs.push_back(std::string(1000, 'x') + "SESSION-ID=");
    inputs.push_back(std::string(1000, 'E'));
    inputs.push_back("ERRO"); // partial prefix
    inputs.push_back("SESSION-ID"); // partial
    // random battery
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> lenD(0, 40);
    std::string alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-=abcdefghij0123456789 ";
    std::uniform_int_distribution<int> chD(0, (int)alpha.size()-1);
    for (int i = 0; i < 5000; ++i) {
        int L = lenD(rng);
        std::string s;
        for (int j = 0; j < L; ++j) s += alpha[chD(rng)];
        // occasionally seed prefixes
        if (i % 7 == 0) s = "SESSION-ID=" + s;
        if (i % 11 == 0) s = "ERROR" + s;
        inputs.push_back(s);
    }

    // Differential check
    bool equivalent = true;
    std::string divergent;
    for (const auto& s : inputs) {
        if (v_before::starts_with_session(s) != v_after::starts_with_session(s) ||
            v_before::not_starts_with_error(s) != v_after::not_starts_with_error(s)) {
            equivalent = false;
            divergent = s;
            break;
        }
    }
    std::cout << "EQUIVALENT=" << (equivalent ? "YES" : "NO") << "\n";
    if (!equivalent) std::cout << "DIVERGENT=[" << divergent << "]\n";

    // Timing: interleaved. Use large strings that do NOT start with prefix
    // (the pathological case where find scans the whole string).
    std::vector<std::string> timingInputs;
    std::uniform_int_distribution<int> chD2(0, (int)alpha.size()-1);
    for (int i = 0; i < 2000; ++i) {
        // Long strings, mostly non-matching, to exercise the scan-vs-noscan difference
        std::string s(2000, 'x');
        // sprinkle so find can't early-out trivially
        s[1500] = 'S';
        timingInputs.push_back(s);
    }

    volatile long sink = 0;
    const int REPS = 200;
    std::vector<double> ratios;
    for (int r = 0; r < REPS; ++r) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (const auto& s : timingInputs) {
            sink += v_before::starts_with_session(s);
            sink += v_before::not_starts_with_error(s);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        for (const auto& s : timingInputs) {
            sink += v_after::starts_with_session(s);
            sink += v_after::not_starts_with_error(s);
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        double bns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
        double ans = std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count();
        if (ans > 0) ratios.push_back(bns/ans);
    }
    std::sort(ratios.begin(), ratios.end());
    double median = ratios[ratios.size()/2];
    std::cout << "SPEEDUP_MEDIAN=" << median << "\n";
    std::cout << "sink=" << sink << "\n";
    return 0;
}
