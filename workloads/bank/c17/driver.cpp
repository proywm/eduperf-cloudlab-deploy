#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace v_before {
static inline void matrix_mul(const float *a, const float *b, float *out, int acols, int brows, int bcols) {
    for (int ar = 0; ar < brows; ar++) {
        for (int bc = 0; bc < bcols; bc++) {
            float sum = 0.0f;
            const float *apos = a + ar * acols;
            const float *bpos = b + bc;

            for (int acbr = 0; acbr < acols; acbr++) {
                sum += apos[acbr] * bpos[acbr * bcols];
            }

            out[ar * bcols + bc] = sum;
        }
    }
}
}

namespace v_after {
static inline void matrix_mul(const float *a, const float *b, float *out, int acols, int brows, int bcols) {
    for (int ar = 0; ar < brows; ar++) {
        for (int bc = 0; bc < bcols; bc++) {
            float sum = 0.0f;
            const float *apos = a + ar * acols;
            const float *bpos = b + bc;

            int acbr;
            for (acbr = 0; acbr < (acols & (~3)); acbr += 4) {
                sum += apos[acbr] * (*bpos);
                bpos += bcols;
                sum += apos[acbr+1] * (*bpos);
                bpos += bcols;
                sum += apos[acbr+2] * (*bpos);
                bpos += bcols;
                sum += apos[acbr+3] * (*bpos);
                bpos += bcols;
            }

            for (; acbr < acols; acbr++) {
                sum += apos[acbr] * (*bpos);
                bpos += bcols;
            }

            out[ar * bcols + bc] = sum;
        }
    }
}
}

static const int REPS = 200;

static long long work(int version) {
    const int acols = 16, brows = 16, bcols = 16;
    float a[acols * brows], b[bcols * acols], out[brows * bcols];
    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    std::memset(out, 0, sizeof(out));

    for (int i = 0; i < acols * brows; i++) {
        a[i] = static_cast<float>(i);
    }
    for (int i = 0; i < bcols * acols; i++) {
        b[i] = static_cast<float>(i + 1);
    }

    long long checksum = 0;
    for (int rep = 0; rep < REPS; rep++) {
        if (version == 0) {
            v_before::matrix_mul(a, b, out, acols, brows, bcols);
        } else {
            v_after::matrix_mul(a, b, out, acols, brows, bcols);
        }
    }

    for (int i = 0; i < brows * bcols; i++) {
        checksum ^= static_cast<int64_t>(out[i] * 1000.0f);
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
