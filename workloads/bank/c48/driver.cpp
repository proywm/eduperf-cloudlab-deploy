// Differential test + timing for Operon::RankOrdinalSorter::Sort optimization.
// before: comparator reads pop[a][i] (pointer indirection per compare)
// after : hoist fitness column into contiguous buf, sort over buf[a]

#include <Eigen/Core>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdio>
#include <stdexcept>

// ---- tiny faithful stubs for Operon project types ----
namespace Operon {
    using Scalar = float;

    template <typename T>
    struct Span {
        T* ptr;
        std::size_t n;
        Span(T* p, std::size_t c) : ptr(p), n(c) {}
        std::size_t size() const { return n; }
        T& front() const { return ptr[0]; }
        T* begin() const { return ptr; }
        T* end() const { return ptr + n; }
        T& operator[](std::size_t i) const { return ptr[i]; }
    };

    struct Individual {
        std::vector<Scalar> fitness;
        std::size_t Size() const { return fitness.size(); }
        Scalar operator[](std::size_t i) const { return fitness[i]; }
    };

    // Operon::Less: strict less with eps tolerance (as used in comparator)
    struct Less {
        bool operator()(Scalar a, Scalar b, Scalar eps) const {
            return a + eps < b;
        }
    };

    struct NondominatedSorterBase {
        using Result = std::vector<std::vector<std::size_t>>;
    };
}

// =================== BEFORE ===================
namespace v_before {
using namespace Operon;
auto Sort(Operon::Span<Operon::Individual const> pop, Operon::Scalar eps) -> NondominatedSorterBase::Result
{
    using Vec = Eigen::Array<Eigen::Index, -1, 1>;
    using Mat = Eigen::Array<Eigen::Index, -1, -1>;

    const auto n = static_cast<Eigen::Index>(pop.size());
    const auto m = static_cast<Eigen::Index>(pop.front().Size());
    assert(m >= 2);

    Mat p(n, m);
    Mat r(m, n);
    p.col(0) = Vec::LinSpaced(n, 0, n - 1);
    r(0, p.col(0)) = Vec::LinSpaced(n, 0, n - 1);

    Operon::Less cmp;
    for (auto i = 1; i < m; ++i) {
        p.col(i) = p.col(i - 1);
        std::stable_sort(p.col(i).begin(), p.col(i).end(), [&](auto a, auto b) { return cmp(pop[a][i], pop[b][i], eps); });
        r(i, p.col(i)) = Vec::LinSpaced(n, 0, n - 1);
    }
    Vec maxc(n);
    Vec maxp(n);
    for (auto i = 0; i < n; ++i) {
        auto c = r.col(i);
        auto max = std::max_element(c.begin(), c.end());
        maxp(i) = *max;
        maxc(i) = std::distance(c.begin(), max);
    }

    Vec rank = Vec::Zero(n);
    for (auto i : p(Eigen::seq(0, n - 2), 0)) {
        if (maxp(i) == n - 1) {
            continue;
        }
        for (auto j : p(Eigen::seq(maxp(i) + 1, n - 1), maxc(i))) {
            rank(j) += static_cast<int64_t>(rank(i) == rank(j) && (r.col(i) < r.col(j)).all());
        }
    }
    std::vector<std::vector<size_t>> fronts(rank.maxCoeff() + 1);
    for (auto i = 0; i < n; ++i) {
        fronts[rank(i)].push_back(i);
    }
    return fronts;
}
}

// =================== AFTER ===================
namespace v_after {
using namespace Operon;
auto Sort(Operon::Span<Operon::Individual const> pop, Operon::Scalar eps) -> NondominatedSorterBase::Result
{
    using Vec = Eigen::Array<Eigen::Index, -1, 1>;
    using Mat = Eigen::Array<Eigen::Index, -1, -1>;

    const auto n = static_cast<Eigen::Index>(pop.size());
    const auto m = static_cast<Eigen::Index>(pop.front().Size());
    assert(m >= 2);

    Mat p(n, m);
    Mat r(m, n);
    p.col(0) = Vec::LinSpaced(n, 0, n - 1);
    r(0, p.col(0)) = Vec::LinSpaced(n, 0, n - 1);

    Operon::Less cmp;
    std::vector<Operon::Scalar> buf(n);
    for (auto i = 1; i < m; ++i) {
        std::transform(pop.begin(), pop.end(), buf.begin(), [i](auto const& ind) { return ind[i]; });
        p.col(i) = p.col(i - 1);
        std::stable_sort(p.col(i).begin(), p.col(i).end(), [&](auto a, auto b) { return cmp(buf[a], buf[b], eps); });
        r(i, p.col(i)) = Vec::LinSpaced(n, 0, n - 1);
    }
    Vec maxc(n);
    Vec maxp(n);
    for (auto i = 0; i < n; ++i) {
        auto c = r.col(i);
        auto max = std::max_element(c.begin(), c.end());
        maxp(i) = *max;
        maxc(i) = std::distance(c.begin(), max);
    }

    Vec rank = Vec::Zero(n);
    for (auto i : p(Eigen::seq(0, n - 2), 0)) {
        if (maxp(i) == n - 1) {
            continue;
        }
        for (auto j : p(Eigen::seq(maxp(i) + 1, n - 1), maxc(i))) {
            rank(j) += static_cast<int64_t>(rank(i) == rank(j) && (r.col(i) < r.col(j)).all());
        }
    }
    std::vector<std::vector<size_t>> fronts(rank.maxCoeff() + 1);
    for (auto i = 0; i < n; ++i) {
        fronts[rank(i)].push_back(i);
    }
    return fronts;
}
}

// ---- helpers ----
using Pop = std::vector<Operon::Individual>;

Pop make_pop(std::mt19937& rng, int n, int m, int distinct_levels) {
    Pop pop(n);
    std::uniform_int_distribution<int> lvl(0, distinct_levels - 1);
    for (int i = 0; i < n; ++i) {
        pop[i].fitness.resize(m);
        for (int j = 0; j < m; ++j) {
            // quantize to create ties / eps-boundary situations
            pop[i].fitness[j] = static_cast<float>(lvl(rng)) * 0.5f;
        }
    }
    return pop;
}

bool run_case(Pop& pop, float eps, std::string& why) {
    Operon::Span<Operon::Individual const> sp(pop.data(), pop.size());
    auto rb = v_before::Sort(sp, eps);
    auto ra = v_after::Sort(sp, eps);
    if (rb.size() != ra.size()) { why = "front count differs"; return false; }
    for (size_t f = 0; f < rb.size(); ++f) {
        if (rb[f] != ra[f]) { why = "front content differs at " + std::to_string(f); return false; }
    }
    return true;
}

int main() {
    std::mt19937 rng(12345);

    // ---- correctness battery ----
    int fails = 0;
    std::string firstdiv;
    auto check = [&](Pop pop, float eps, const char* tag) {
        std::string why;
        if (!run_case(pop, eps, why)) {
            if (firstdiv.empty()) firstdiv = std::string(tag) + ": " + why;
            ++fails;
        }
    };

    // boundary sizes and shapes
    for (int n : {2, 3, 5, 8, 16, 32, 64, 128}) {
        for (int m : {2, 3, 5, 8}) {
            for (int levels : {1, 2, 3, n, n * m}) {
                for (float eps : {0.0f, 0.1f, 0.25f, 0.5f, 1.0f}) {
                    Pop pop = make_pop(rng, n, std::min(m, 8), std::max(1, levels));
                    check(pop, eps, "grid");
                }
            }
        }
    }
    // all-identical
    {
        Pop pop(20);
        for (auto& ind : pop) ind.fitness = {1.0f, 1.0f, 1.0f, 1.0f};
        check(pop, 0.0f, "all-identical");
        check(pop, 0.3f, "all-identical-eps");
    }
    // monotone increasing (already sorted)
    {
        Pop pop(30);
        for (int i = 0; i < 30; ++i) pop[i].fitness = {(float)i, (float)i, (float)i};
        check(pop, 0.0f, "monotone");
        check(pop, 0.7f, "monotone-eps");
    }
    // reverse sorted
    {
        Pop pop(30);
        for (int i = 0; i < 30; ++i) pop[i].fitness = {(float)(30 - i), (float)i, (float)(30 - i)};
        check(pop, 0.0f, "mixed");
    }
    // negative + fractional values near eps boundary
    {
        std::mt19937 r2(99);
        std::uniform_real_distribution<float> d(-2.0f, 2.0f);
        Pop pop(50);
        for (auto& ind : pop) { ind.fitness.resize(4); for (auto& x : ind.fitness) x = d(r2); }
        check(pop, 0.0f, "cont");
        check(pop, 0.05f, "cont-eps");
    }

    if (fails) {
        printf("EQUIV_FAIL count=%d first=%s\n", fails, firstdiv.c_str());
        return 0;
    }
    printf("EQUIV_OK\n");

    // ---- timing (interleaved) ----
    // large-ish population, several objectives -> sort dominates
    // Regime where the sort phase (m-1 passes over n) dominates, so the
    // pointer-indirection avoidance in the comparator is exercised.
    const int N = 3000, M = 60;
    Pop pop = make_pop(rng, N, M, N); // mostly distinct
    Operon::Span<Operon::Individual const> sp(pop.data(), pop.size());
    float eps = 0.0f;

    const int reps = 60;
    std::vector<double> bt, at;
    // warmup
    for (int w = 0; w < 3; ++w) { auto x = v_before::Sort(sp, eps); auto y = v_after::Sort(sp, eps); (void)x;(void)y; }
    for (int rep = 0; rep < reps; ++rep) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto rb = v_before::Sort(sp, eps);
        auto t1 = std::chrono::high_resolution_clock::now();
        auto ra = v_after::Sort(sp, eps);
        auto t2 = std::chrono::high_resolution_clock::now();
        bt.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        at.push_back(std::chrono::duration<double, std::nano>(t2 - t1).count());
        if (rb.size() != ra.size()) { printf("timing mismatch\n"); }
    }
    std::sort(bt.begin(), bt.end());
    std::sort(at.begin(), at.end());
    double bmed = bt[bt.size()/2];
    double amed = at[at.size()/2];
    printf("before_ns=%.0f after_ns=%.0f speedup=%.3f\n", bmed, amed, bmed / amed);
    return 0;
}
