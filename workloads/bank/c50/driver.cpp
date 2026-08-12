#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

// ---- tiny trivial stubs for project types (behavior-faithful) ----
struct LocRecord {
    uintptr_t addr; uint16_t f, i, s;
    LocRecord(uintptr_t a, uint16_t func_id, uint16_t inst_id, uint16_t size)
        : addr(a), f(func_id), i(inst_id), s(size) {}
};

struct Thread {
    bool malloc_hook_active = true;
    int index = 0;
    long long calls = 0;
    long long checksum = 0;
    void log_load_store(const LocRecord &rec, bool is_write) {
        calls++;
        checksum += (long long)rec.addr + rec.f + rec.i + rec.s + (is_write ? 1 : 0);
    }
};

static uintptr_t heapStart_b, heapEnd_b;
static uintptr_t heapStart_a, heapEnd_a;

namespace v_before {
    thread_local Thread thr;
    Thread *current = &thr;
    uintptr_t &heapStart = heapStart_b;
    uintptr_t &heapEnd = heapEnd_b;

    class MallocHookDeactivator {
        Thread *current_copy;
    public:
        MallocHookDeactivator() noexcept: current_copy(current) { current_copy->malloc_hook_active = false; }
        ~MallocHookDeactivator() noexcept { current_copy->malloc_hook_active = true; }
        Thread *get_current() { return current_copy; }
    };

    // VERBATIM before.cpp handle_access
    void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                       size_t size, bool is_write) {
        MallocHookDeactivator deactiv;
        // Quickly return if even not in the range.
        bool is_heap = (addr >= heapStart && addr < heapEnd);
        if (!is_heap)
            return;
        LocRecord rec = LocRecord(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size);
        deactiv.get_current()->log_load_store(rec, is_write);
    }
}

namespace v_after {
    thread_local Thread thr;
    Thread *current = &thr;
    uintptr_t &heapStart = heapStart_a;
    uintptr_t &heapEnd = heapEnd_a;

    class MallocHookDeactivator {
        Thread *current_copy;
    public:
        MallocHookDeactivator() noexcept: current_copy(current) { current_copy->malloc_hook_active = false; }
        ~MallocHookDeactivator() noexcept { current_copy->malloc_hook_active = true; }
        Thread *get_current() { return current_copy; }
    };

    // VERBATIM after.cpp handle_access
    inline void handle_access(uintptr_t addr, uint64_t func_id, uint64_t inst_id,
                       size_t size, bool is_write) {
        // Quickly return if even not in the range.
        if (addr < heapStart || addr >= heapEnd)
            return;
        MallocHookDeactivator deactiv;
        LocRecord rec = LocRecord(addr, (uint16_t) func_id, (uint16_t) inst_id, (uint16_t) size);
        deactiv.get_current()->log_load_store(rec, is_write);
    }
}

int main() {
    uintptr_t hs = 0x100000, he = 0x200000;
    heapStart_b = hs; heapEnd_b = he;
    heapStart_a = hs; heapEnd_a = he;

    std::mt19937_64 rng(12345);

    std::vector<uintptr_t> addrs;
    addrs.push_back(0);
    addrs.push_back(hs - 1);
    addrs.push_back(hs);
    addrs.push_back(hs + 1);
    addrs.push_back(he - 1);
    addrs.push_back(he);
    addrs.push_back(he + 1);
    addrs.push_back((uintptr_t)-1);
    addrs.push_back(((uintptr_t)1) << 63);
    for (int k = 0; k < 20000; ++k) {
        uintptr_t a;
        int mode = rng() % 3;
        if (mode == 0) a = hs + (rng() % (he - hs));
        else if (mode == 1) a = rng() % hs;
        else a = he + (rng() % 0x100000);
        addrs.push_back(a);
    }

    for (size_t idx = 0; idx < addrs.size(); ++idx) {
        uintptr_t a = addrs[idx];
        uint64_t fid = rng() & 0xFFFF;
        uint64_t iid = rng() & 0xFFFF;
        size_t sz = (rng() % 16) + 1;
        bool w = (rng() & 1);

        v_before::handle_access(a, fid, iid, sz, w);
        v_after::handle_access(a, fid, iid, sz, w);

        if (v_before::current->malloc_hook_active != v_after::current->malloc_hook_active) {
            printf("DIVERGE hook_active at idx %zu addr=%p\n", idx, (void*)a);
            return 2;
        }
    }

    if (v_before::current->calls != v_after::current->calls ||
        v_before::current->checksum != v_after::current->checksum) {
        printf("DIVERGE state: before(calls=%lld,cs=%lld) after(calls=%lld,cs=%lld)\n",
               v_before::current->calls, v_before::current->checksum,
               v_after::current->calls, v_after::current->checksum);
        return 2;
    }

    printf("EQUIVALENT: calls=%lld checksum=%lld\n",
           v_before::current->calls, v_before::current->checksum);

    std::vector<uintptr_t> timing_addrs;
    for (int k = 0; k < 2000000; ++k) {
        int mode = rng() % 5;
        uintptr_t a;
        if (mode < 4) a = (rng() & 1) ? (rng() % hs) : (he + (rng() % 0x100000));
        else a = hs + (rng() % (he - hs));
        timing_addrs.push_back(a);
    }

    const int REPS = 7;
    std::vector<double> ratios;
    for (int r = 0; r < REPS; ++r) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (uintptr_t a : timing_addrs) v_before::handle_access(a, 1, 2, 8, true);
        auto t1 = std::chrono::high_resolution_clock::now();
        for (uintptr_t a : timing_addrs) v_after::handle_access(a, 1, 2, 8, true);
        auto t2 = std::chrono::high_resolution_clock::now();
        double bt = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        double at = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
        ratios.push_back(bt / at);
    }
    std::sort(ratios.begin(), ratios.end());
    double median = ratios[ratios.size() / 2];
    printf("SPEEDUP median(before/after) = %.4f\n", median);
    return 0;
}
