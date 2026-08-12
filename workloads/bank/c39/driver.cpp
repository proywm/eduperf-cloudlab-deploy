#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <climits>
#include <random>

// Real changed function is C4Script "global func Sign(int x)".
// Translated VERBATIM in semantics to C++ (pure integer arithmetic; int == 32-bit).

namespace v_before {
    int Sign(int x)
    {
        if (x > 0)
            return 1;
        else if (x < 0)
            return -1;
        return 0;
    }
}

namespace v_after {
    int Sign(int x)
    {
        return (x>0)-(x<0);
    }
}

int main() {
    // Differential battery
    std::vector<int> inputs;
    for (int v = -10000; v <= 10000; ++v) inputs.push_back(v);
    int boundary[] = {0, 1, -1, 2, -2, INT_MAX, INT_MIN, INT_MAX-1, INT_MIN+1,
                      100000, -100000, 123456789, -123456789, 42, -42};
    for (int v : boundary) inputs.push_back(v);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
    for (int i = 0; i < 200000; ++i) inputs.push_back(dist(rng));

    long mismatches = 0;
    int first_bad = 0; bool have_bad = false;
    for (int x : inputs) {
        int b = v_before::Sign(x);
        int a = v_after::Sign(x);
        if (b != a) { if (!have_bad) { first_bad = x; have_bad = true; } mismatches++; }
    }
    if (have_bad) { printf("DIVERGENCE first_bad=%d mismatches=%ld\n", first_bad, mismatches); }
    else printf("EQUIVALENT over %zu inputs\n", inputs.size());

    // Timing, interleaved
    volatile long sink = 0;
    const int REPS = 400;
    std::vector<double> before_times, after_times;
    for (int r = 0; r < REPS; ++r) {
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            long s = 0;
            for (int x : inputs) s += v_before::Sign(x);
            auto t1 = std::chrono::high_resolution_clock::now();
            sink += s;
            before_times.push_back(std::chrono::duration<double,std::nano>(t1-t0).count());
        }
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            long s = 0;
            for (int x : inputs) s += v_after::Sign(x);
            auto t1 = std::chrono::high_resolution_clock::now();
            sink += s;
            after_times.push_back(std::chrono::duration<double,std::nano>(t1-t0).count());
        }
    }
    std::sort(before_times.begin(), before_times.end());
    std::sort(after_times.begin(), after_times.end());
    double mb = before_times[before_times.size()/2];
    double ma = after_times[after_times.size()/2];
    printf("median_before_ns=%.1f median_after_ns=%.1f speedup=%.3f sink=%ld\n", mb, ma, mb/ma, (long)sink);
    return 0;
}
