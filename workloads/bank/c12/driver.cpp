#include <vector>
#include <algorithm>
#include <numeric>

namespace v_before {
    void roundDown(const std::vector<int>& src, std::vector<int>& dst, const std::vector<int>& boundary_values) {
        size_t size = src.size();
        dst.resize(size);

        for (size_t i = 0; i < size; ++i) {
            auto it = std::upper_bound(boundary_values.begin(), boundary_values.end(), src[i]);
            if (it == boundary_values.end()) {
                dst[i] = boundary_values.back();
            } else if (it == boundary_values.begin()) {
                dst[i] = boundary_values.front();
            } else {
                dst[i] = *(it - 1);
            }
        }
    }
}

namespace v_after {
    void roundDown(const std::vector<int>& src, std::vector<int>& dst, const std::vector<int>& boundary_values) {
        size_t size = src.size();
        dst.resize(size);

        auto begin = boundary_values.begin();
        auto end = boundary_values.end();
        auto it = begin + (end - begin) / 2;

        for (size_t i = 0; i < size; ++i) {
            auto value = src[i];

            if (*it < value) {
                while (it != end && *it <= value)
                    ++it;
                if (it != begin)
                    --it;
            } else {
                while (*it > value && it != begin)
                    --it;
            }

            dst[i] = *it;
        }
    }
}

static const int REPS = 100;

static long long work(int version) {
    std::vector<int> src(1000);
    std::vector<int> dst;
    std::vector<int> boundary_values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Deterministic input
    for (size_t i = 0; i < src.size(); ++i) {
        src[i] = static_cast<int>(i % boundary_values.size());
    }

    long long checksum = 0;

    for (int rep = 0; rep < REPS; ++rep) {
        if (version == 0) {
            v_before::roundDown(src, dst, boundary_values);
        } else {
            v_after::roundDown(src, dst, boundary_values);
        }

        checksum ^= std::accumulate(dst.begin(), dst.end(), 0LL);
    }

    return checksum;
}

// ===== fixed harness (appended; do not edit) =====
#include <cstdio>
#include <chrono>
#include <climits>
#include <algorithm>
int main(){
    long long c0 = work(0);
    long long c1 = work(1);
    using clk = std::chrono::steady_clock;
    auto measure = [](int v)->long long{
        auto t0=clk::now();
        volatile long long sink=0;
        for(int k=0;k<REPS;k++) sink += work(v);
        auto t1=clk::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
    };
    long long before[9], after[9];
    for(int r=0;r<9;r++){
        if(r%2==0){before[r]=measure(0); after[r]=measure(1);}
        else {after[r]=measure(1); before[r]=measure(0);}
    }
    std::sort(before, before+9); std::sort(after, after+9);
    long long b=before[4], a=after[4];
    printf("EQUIV=%d\n", (c0==c1)?1:0);
    printf("BEFORE_NS=%lld\n", b);
    printf("AFTER_NS=%lld\n", a);
    printf("READY=1\n");
    return 0;
}
