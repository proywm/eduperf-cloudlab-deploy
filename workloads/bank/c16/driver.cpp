#include <vector>
#include <numeric>
#include <algorithm>
#include <string>

namespace casadi {
    void casadi_scal(int n, double a, std::vector<double>& x) {
        for (int i = 0; i < n; ++i) {
            x[i] *= a;
        }
    }

    void casadi_qr_solve(std::vector<double>& b, int m, int n, const std::vector<int>& sp_v,
                        const std::vector<double>& v, const std::vector<int>& sp_r,
                        const std::vector<double>& r, double beta, const std::vector<double>& prinv,
                        const std::vector<double>& pc, std::vector<double>& w) {
        // Stub implementation
    }

    void print_vector(const std::string& name, const std::vector<double>& v, int n) {
        // Stub implementation
    }
}

namespace v_before {
    void compute_dz(int nx_, int na_, const std::vector<double>& lam, const std::vector<double>& z,
                    const std::vector<double>& ubz, const std::vector<double>& lbz,
                    const std::vector<double>& glag, bool verbose_, std::vector<double>& dz) {
        for (int i = 0; i < nx_ + na_; ++i) {
            if (lam[i] > 0.) {
                dz[i] = z[i] - ubz[i];
            } else if (lam[i] < 0.) {
                dz[i] = z[i] - lbz[i];
            } else if (i < nx_) {
                dz[i] = glag[i];
            } else {
                dz[i] = -lam[i];
            }
        }

        if (verbose_) {
            casadi::print_vector("kkt residual", dz, nx_ + na_);
        }

        casadi::casadi_scal(nx_ + na_, -1., dz);
    }
}

namespace v_after {
    void compute_dz(int nx_, int na_, const std::vector<double>& lam, const std::vector<double>& z,
                    const std::vector<double>& ubz, const std::vector<double>& lbz,
                    const std::vector<double>& glag, bool verbose_, std::vector<double>& dz) {
        for (int i = 0; i < nx_ + na_; ++i) {
            if (lam[i] > 0.) {
                dz[i] = ubz[i] - z[i];
            } else if (lam[i] < 0.) {
                dz[i] = lbz[i] - z[i];
            } else if (i < nx_) {
                dz[i] = -glag[i];
            } else {
                dz[i] = lam[i];
            }
        }

        if (verbose_) {
            casadi::print_vector("neg kkt residual", dz, nx_ + na_);
        }
    }
}

static const int REPS = 400;

static long long work(int version) {
    const int nx_ = 100;
    const int na_ = 50;
    std::vector<double> lam(nx_ + na_, 0.5);
    std::vector<double> z(nx_ + na_, 1.0);
    std::vector<double> ubz(nx_ + na_, 2.0);
    std::vector<double> lbz(nx_ + na_, 0.0);
    std::vector<double> glag(nx_, 0.3);
    bool verbose_ = false;
    std::vector<double> dz(nx_ + na_);

    for (int rep = 0; rep < REPS; ++rep) {
        if (version == 0) {
            v_before::compute_dz(nx_, na_, lam, z, ubz, lbz, glag, verbose_, dz);
        } else {
            v_after::compute_dz(nx_, na_, lam, z, ubz, lbz, glag, verbose_, dz);
        }
    }

    long long checksum = 0;
    for (double val : dz) {
        checksum += static_cast<long long>(val * 1e9);
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
