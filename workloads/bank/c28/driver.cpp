// Faithful self-contained reproduction of SerenityOS commit d5dce448ea67
// AK/Buffered.h  Buffered<T>::read(Bytes)  before vs after
//
// The optimization: bypass Buffered's internal buffer for large reads to avoid
// superfluous memmoves and recursion.  Behavior (bytes returned + destination
// contents) must be identical for a given stream + buffer state.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstdio>
#include <chrono>
#include <random>
#include <algorithm>

// ---- Minimal faithful Span<uint8_t> ("Bytes") ----
// Semantics copied verbatim from AK/Span.h (trim/slice/copy_to/copy_trimmed_to).
struct Bytes {
    uint8_t* m_values { nullptr };
    size_t m_size { 0 };

    Bytes() = default;
    Bytes(uint8_t* v, size_t s) : m_values(v), m_size(s) {}

    size_t size() const { return m_size; }
    uint8_t* data() const { return m_values; }

    Bytes slice(size_t start, size_t length) const {
        assert(start + length <= size());
        return { m_values + start, length };
    }
    Bytes slice(size_t start) const {
        assert(start <= size());
        return { m_values + start, size() - start };
    }
    Bytes trim(size_t length) const {
        return { m_values, std::min(size(), length) };
    }
    // memmove-based copy of size() elements (VERIFY other >= size())
    size_t copy_to(Bytes other) const {
        assert(other.size() >= size());
        __builtin_memmove(other.data(), data(), size());
        return size();
    }
    size_t copy_trimmed_to(Bytes other) const {
        size_t count = std::min(size(), other.size());
        __builtin_memmove(other.data(), data(), count);
        return count;
    }
};

// ---- A concrete source InputStream over a byte vector ----
// read(Bytes) reads at most bytes.size(), advancing an internal cursor.
// To exercise the "partial refill" paths we let it return a bounded chunk per call.
struct SourceStream {
    std::vector<uint8_t> data;
    size_t pos { 0 };
    size_t max_chunk { SIZE_MAX }; // cap bytes returned per read() call

    size_t read(Bytes b) {
        size_t avail = data.size() - pos;
        size_t n = std::min({ b.size(), avail, max_chunk });
        std::memcpy(b.data(), data.data() + pos, n);
        pos += n;
        return n;
    }
    bool has_any_error() const { return false; }
};

namespace v_before {
    // Buffered<T> harness; read() is the VERBATIM before version.
    template <size_t BUFSZ>
    struct Buffered {
        SourceStream m_stream;
        uint8_t m_buffer[BUFSZ];
        size_t m_buffered { 0 };

        Bytes buffer() { return Bytes(m_buffer, BUFSZ); }
        bool has_any_error() const { return false; }

        // ===== VERBATIM src_before =====
        size_t read(Bytes bytes)
        {
            if (has_any_error())
                return 0;

            auto nread = buffer().trim(m_buffered).copy_trimmed_to(bytes);

            m_buffered -= nread;
            buffer().slice(nread, m_buffered).copy_to(buffer());

            if (nread < bytes.size()) {
                m_buffered = m_stream.read(buffer());

                if (m_buffered == 0)
                    return nread;

                nread += read(bytes.slice(nread));
            }

            return nread;
        }
        // ===============================
    };
}

namespace v_after {
    template <size_t BUFSZ>
    struct Buffered {
        SourceStream m_stream;
        uint8_t m_buffer[BUFSZ];
        size_t m_buffered { 0 };

        Bytes buffer() { return Bytes(m_buffer, BUFSZ); }
        bool has_any_error() const { return false; }

        // ===== VERBATIM src_after =====
        size_t read(Bytes bytes)
        {
            if (has_any_error())
                return 0;

            auto nread = buffer().trim(m_buffered).copy_trimmed_to(bytes);

            m_buffered -= nread;
            if (m_buffered > 0)
                buffer().slice(nread, m_buffered).copy_to(buffer());

            if (nread < bytes.size()) {
                nread += m_stream.read(bytes.slice(nread));

                m_buffered = m_stream.read(buffer());
            }

            return nread;
        }
        // ==============================
    };
}

// ---- Differential harness ----
// A single read() call only reads one buffer-refill worth in the before version
// per top-level invocation chain; realistic use drains the stream via repeated
// read() calls.  We model a full drain: repeatedly read into a dest span until
// the stream is exhausted, and compare the full reconstructed output.

template <size_t BUFSZ>
struct DrainResult {
    std::vector<uint8_t> out;
    size_t total { 0 };
    size_t calls { 0 };
};

template <typename Buf, size_t BUFSZ>
DrainResult<BUFSZ> drain(const std::vector<uint8_t>& src, size_t max_chunk,
                         size_t req_size, size_t initial_prefill)
{
    Buf b;
    b.m_stream.data = src;
    b.m_stream.max_chunk = max_chunk;
    // Optionally pre-fill the internal buffer to exercise buffered state.
    if (initial_prefill > 0) {
        b.m_buffered = b.m_stream.read(b.buffer().trim(std::min(initial_prefill, BUFSZ)));
    }

    DrainResult<BUFSZ> r;
    std::vector<uint8_t> dst(req_size);
    // Drain until a read returns 0 (nothing left in buffer or stream).
    for (;;) {
        std::fill(dst.begin(), dst.end(), 0xAB);
        size_t n = b.read(Bytes(dst.data(), req_size));
        r.calls++;
        if (n == 0) break;
        r.out.insert(r.out.end(), dst.begin(), dst.begin() + n);
        r.total += n;
        if (r.calls > 1000000) break; // safety
    }
    return r;
}

static bool g_ok = true;
static std::string g_div;

template <size_t BUFSZ>
void check_case(const std::vector<uint8_t>& src, size_t max_chunk,
                size_t req_size, size_t prefill, const std::string& tag)
{
    auto rb = drain<v_before::Buffered<BUFSZ>, BUFSZ>(src, max_chunk, req_size, prefill);
    auto ra = drain<v_after::Buffered<BUFSZ>, BUFSZ>(src, max_chunk, req_size, prefill);
    if (rb.total != ra.total || rb.out != ra.out) {
        if (g_ok) {
            g_div = tag + " (bufsz=" + std::to_string(BUFSZ) +
                    " srclen=" + std::to_string(src.size()) +
                    " chunk=" + std::to_string(max_chunk) +
                    " req=" + std::to_string(req_size) +
                    " prefill=" + std::to_string(prefill) +
                    " before_total=" + std::to_string(rb.total) +
                    " after_total=" + std::to_string(ra.total) + ")";
        }
        g_ok = false;
    }
}

int main()
{
    std::mt19937_64 rng(12345);

    auto mkseq = [&](size_t n) {
        std::vector<uint8_t> v(n);
        for (size_t i = 0; i < n; i++) v[i] = (uint8_t)(i * 131 + 7);
        return v;
    };

    // Diverse / boundary / adversarial battery.
    std::vector<size_t> lens = {0,1,2,3,4,7,8,15,16,17,31,32,33,63,64,100,255,256,257,1000,4096,5000};
    std::vector<size_t> chunks = {1,2,3,7,8,16,64,SIZE_MAX};
    std::vector<size_t> reqs = {1,2,3,7,8,16,17,64,100,256,1000,4096,5000};

    // BUFSZ = 8
    for (auto L : lens) { auto s = mkseq(L);
      for (auto c : chunks) for (auto r : reqs) for (size_t pf : {(size_t)0,(size_t)3,(size_t)8}) {
        if (r == 0) continue;
        check_case<8>(s, c, r, pf, "bufsz8");
      }}
    // BUFSZ = 16
    for (auto L : lens) { auto s = mkseq(L);
      for (auto c : chunks) for (auto r : reqs) for (size_t pf : {(size_t)0,(size_t)5,(size_t)16}) {
        if (r == 0) continue;
        check_case<16>(s, c, r, pf, "bufsz16");
      }}
    // BUFSZ = 4096 (large-read path, the point of the optimization)
    {
      std::vector<size_t> L2 = {0,1,100,4095,4096,4097,8192,20000};
      std::vector<size_t> c2 = {64,1024,SIZE_MAX};
      std::vector<size_t> r2 = {1,100,4096,8192,20000};
      for (auto L : L2) { auto s = mkseq(L);
        for (auto c : c2) for (auto r : r2) for (size_t pf : {(size_t)0,(size_t)1000}) {
          if (r == 0) continue;
          check_case<4096>(s, c, r, pf, "bufsz4096");
        }}
    }

    if (!g_ok) {
        printf("EQUIVALENT: NO\n");
        printf("DIVERGENT: %s\n", g_div.c_str());
        return 0;
    }
    printf("EQUIVALENT: YES\n");

    // ---- Interleaved timing: large-read workload (the optimization target) ----
    // Big stream, request span larger than buffer -> before does memmoves+recursion,
    // after bypasses the buffer.
    // Isolate the read() work itself.  We drain the stream into a single
    // reused destination span (no per-call full-buffer memset, no vector
    // growth) so the timing reflects the actual memmove/source-read work that
    // differs between the two versions, not harness bookkeeping.
    const size_t BUF = 4096;
    auto big = mkseq(4 * 1024 * 1024); // 4 MiB
    const size_t REQ = 64 * 1024;      // 64 KiB requests
    const int ITERS = 60;

    std::vector<uint8_t> dstbuf(REQ);
    volatile uint64_t sink = 0;
    double best_before = 1e18, best_after = 1e18;

    auto time_before = [&]() {
        auto t0 = std::chrono::steady_clock::now();
        for (int it = 0; it < ITERS; it++) {
            v_before::Buffered<BUF> b;
            b.m_stream.data = big; b.m_stream.max_chunk = SIZE_MAX;
            size_t total = 0;
            for (;;) {
                size_t n = b.read(Bytes(dstbuf.data(), REQ));
                if (n == 0) break;
                total += n; sink += dstbuf[0];
                if (total > big.size() + 16) break;
            }
            sink += total;
        }
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double,std::nano>(t1-t0).count();
    };
    auto time_after = [&]() {
        auto t0 = std::chrono::steady_clock::now();
        for (int it = 0; it < ITERS; it++) {
            v_after::Buffered<BUF> b;
            b.m_stream.data = big; b.m_stream.max_chunk = SIZE_MAX;
            size_t total = 0;
            for (;;) {
                size_t n = b.read(Bytes(dstbuf.data(), REQ));
                if (n == 0) break;
                total += n; sink += dstbuf[0];
                if (total > big.size() + 16) break;
            }
            sink += total;
        }
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double,std::nano>(t1-t0).count();
    };

    for (int rep = 0; rep < 9; rep++) {
        best_before = std::min(best_before, time_before());
        best_after  = std::min(best_after,  time_after());
    }

    double speedup = best_before / best_after;
    printf("before_ns=%.0f after_ns=%.0f speedup=%.3f sink=%llu\n",
           best_before, best_after, speedup, (unsigned long long)sink);
    return 0;
}
