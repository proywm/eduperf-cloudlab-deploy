#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <chrono>
#include <string>
#include <algorithm>

// ---- tiny trivial stubs for project types ----
enum signalflow_filter_type_t {
    SIGNALFLOW_FILTER_TYPE_LOW_PASS = 0,
    SIGNALFLOW_FILTER_TYPE_BAND_PASS,
    SIGNALFLOW_FILTER_TYPE_HIGH_PASS,
    SIGNALFLOW_FILTER_TYPE_NOTCH,
    SIGNALFLOW_FILTER_TYPE_PEAK,
};

static void signalflow_audio_thread_error(const char *) { /* no-op stub */ }

// Property stub: int_value() returns the stored int. Made deliberately
// non-trivial (virtual + volatile) to mimic a real property lookup cost.
struct Property {
    int v;
    volatile int spin = 0;
    virtual int int_value() {
        // emulate a small amount of indirection work per query
        int acc = v;
        for (int i = 0; i < 4; i++) acc ^= (spin + i);
        spin = 0;
        return v;
    }
    virtual ~Property() {}
};

// Node output stub: provides out[channel][frame]
struct Node {
    std::vector<std::vector<float>> out;
};
typedef Node *NodeRef;

// Buffer stub: out[channel][frame]
struct Buffer {
    std::vector<std::vector<float>> data;
    std::vector<float> &operator[](int ch) { return data[ch]; }
};

// ---- shared state base mirroring SVFilter members used by process ----
struct SVFBase {
    Property *filter_type;
    NodeRef input;
    int num_output_channels;
    std::vector<float> ic1eq, ic2eq, a1, a2, a3, k;
    // _recalculate is not exercised (would need graph/cutoff/resonance);
    // we pre-populate coefficients and stub _recalculate to a no-op so the
    // hot loop under test is faithful and identical between versions.
    void _recalculate(int) {}
};

namespace v_before {
struct SVFilter : SVFBase {
    void process(Buffer &out, int num_frames)
    {
        for (int frame = 0; frame < num_frames; frame++)
        {
            this->_recalculate(frame);

            for (int channel = 0; channel < num_output_channels; channel++)
            {
                float v0 = this->input->out[channel][frame];
                float v3 = v0 - ic2eq[channel];
                float v1 = a1[channel] * ic1eq[channel] + a2[channel] * v3;
                float v2 = ic2eq[channel] + a2[channel] * ic1eq[channel] + a3[channel] * v3;
                ic1eq[channel] = 2 * v1 - ic1eq[channel];
                ic2eq[channel] = 2 * v2 - ic2eq[channel];

                switch (this->filter_type->int_value())
                {
                    case SIGNALFLOW_FILTER_TYPE_LOW_PASS:
                        out[channel][frame] = v2;
                        break;
                    case SIGNALFLOW_FILTER_TYPE_BAND_PASS:
                        out[channel][frame] = v1;
                        break;
                    case SIGNALFLOW_FILTER_TYPE_HIGH_PASS:
                        out[channel][frame] = v0 - k[channel] * v1 - v2;
                        break;
                    case SIGNALFLOW_FILTER_TYPE_NOTCH:
                        out[channel][frame] = v2 + (v0 - k[channel] * v1 - v2);
                        break;
                    case SIGNALFLOW_FILTER_TYPE_PEAK:
                        out[channel][frame] = v2 - (v0 - k[channel] * v1 - v2);
                        break;
                    default:
                        signalflow_audio_thread_error("SVFilter: Unsupported filter type");
                }
            }
        }
    }
};
}

namespace v_after {
struct SVFilter : SVFBase {
    void process(Buffer &out, int num_frames)
    {
        // Cache filter_type rather than querying property each iteration, for efficiency
        signalflow_filter_type_t filter_type = (signalflow_filter_type_t) this->filter_type->int_value();

        for (int frame = 0; frame < num_frames; frame++)
        {
            this->_recalculate(frame);

            for (int channel = 0; channel < num_output_channels; channel++)
            {
                float v0 = this->input->out[channel][frame];
                float v3 = v0 - ic2eq[channel];
                float v1 = a1[channel] * ic1eq[channel] + a2[channel] * v3;
                float v2 = ic2eq[channel] + a2[channel] * ic1eq[channel] + a3[channel] * v3;
                ic1eq[channel] = 2 * v1 - ic1eq[channel];
                ic2eq[channel] = 2 * v2 - ic2eq[channel];

                switch (filter_type)
                {
                    case SIGNALFLOW_FILTER_TYPE_LOW_PASS:
                        out[channel][frame] = v2;
                        break;
                    case SIGNALFLOW_FILTER_TYPE_BAND_PASS:
                        out[channel][frame] = v1;
                        break;
                    case SIGNALFLOW_FILTER_TYPE_HIGH_PASS:
                        out[channel][frame] = v0 - k[channel] * v1 - v2;
                        break;
                    case SIGNALFLOW_FILTER_TYPE_NOTCH:
                        out[channel][frame] = v2 + (v0 - k[channel] * v1 - v2);
                        break;
                    case SIGNALFLOW_FILTER_TYPE_PEAK:
                        out[channel][frame] = v2 - (v0 - k[channel] * v1 - v2);
                        break;
                    default:
                        signalflow_audio_thread_error("SVFilter: Unsupported filter type");
                }
            }
        }
    }
};
}

// ---- helpers to build identical initial state for both versions ----
template <class T>
static void init_filter(T &f, Property &prop, Node &innode, int nch, int nframes,
                        std::mt19937 &rng)
{
    std::uniform_real_distribution<float> ud(-1.0f, 1.0f);
    std::uniform_real_distribution<float> up(0.01f, 0.99f);
    f.filter_type = &prop;
    f.input = &innode;
    f.num_output_channels = nch;
    f.ic1eq.assign(nch, 0.0f);
    f.ic2eq.assign(nch, 0.0f);
    f.a1.resize(nch); f.a2.resize(nch); f.a3.resize(nch); f.k.resize(nch);
    for (int c = 0; c < nch; c++) {
        // plausible SVF coefficients
        float g = up(rng);
        float k = 2.0f - 2.0f * up(rng);
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        f.a1[c] = a1; f.a2[c] = a2; f.a3[c] = a3; f.k[c] = k;
        f.ic1eq[c] = ud(rng);
        f.ic2eq[c] = ud(rng);
    }
}

struct Result {
    std::vector<float> out;      // flattened output buffer
    std::vector<float> ic1, ic2; // mutated state
};

template <class SVF>
static Result run_case(int ftype, int nch, int nframes, uint32_t seed)
{
    std::mt19937 rng(seed);
    Property prop; prop.v = ftype;
    Node innode;
    innode.out.assign(nch, std::vector<float>(nframes));
    std::uniform_real_distribution<float> ud(-2.0f, 2.0f);
    for (int c = 0; c < nch; c++)
        for (int fr = 0; fr < nframes; fr++)
            innode.out[c][fr] = ud(rng);

    SVF f;
    std::mt19937 rng2(seed ^ 0x9e3779b9u);
    init_filter(f, prop, innode, nch, nframes, rng2);

    Buffer out;
    out.data.assign(nch, std::vector<float>(nframes, 0.0f));
    f.process(out, nframes);

    Result r;
    for (int c = 0; c < nch; c++)
        for (int fr = 0; fr < nframes; fr++)
            r.out.push_back(out[c][fr]);
    r.ic1 = f.ic1eq;
    r.ic2 = f.ic2eq;
    return r;
}

static bool bitidentical(const std::vector<float> &a, const std::vector<float> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (std::isnan(a[i]) && std::isnan(b[i])) continue;
        if (a[i] != b[i]) return false;
    }
    return true;
}

int main()
{
    // ---- differential test battery ----
    int ftypes[] = {
        SIGNALFLOW_FILTER_TYPE_LOW_PASS, SIGNALFLOW_FILTER_TYPE_BAND_PASS,
        SIGNALFLOW_FILTER_TYPE_HIGH_PASS, SIGNALFLOW_FILTER_TYPE_NOTCH,
        SIGNALFLOW_FILTER_TYPE_PEAK, 99 /* default/unsupported branch */
    };
    int chans[] = {1, 2, 4, 8, 17};
    int frames[] = {0, 1, 2, 63, 256, 1000};

    bool ok = true;
    std::string divergent = "";
    int cases = 0;
    for (int ft : ftypes)
        for (int nc : chans)
            for (int nf : frames)
                for (uint32_t seed = 1; seed <= 3; seed++) {
                    cases++;
                    Result rb = run_case<v_before::SVFilter>(ft, nc, nf, seed);
                    Result ra = run_case<v_after::SVFilter>(ft, nc, nf, seed);
                    if (!bitidentical(rb.out, ra.out) ||
                        !bitidentical(rb.ic1, ra.ic1) ||
                        !bitidentical(rb.ic2, ra.ic2)) {
                        ok = false;
                        char buf[128];
                        snprintf(buf, sizeof(buf), "ftype=%d nch=%d nframes=%d seed=%u",
                                 ft, nc, nf, seed);
                        divergent = buf;
                        goto done;
                    }
                }
done:
    printf("EQUIV=%s cases=%d divergent=%s\n", ok ? "YES" : "NO", cases,
           divergent.empty() ? "(none)" : divergent.c_str());

    // ---- interleaved timing ----
    // Large frame count, few channels: filter_type query dominates inner loop.
    const int T_NCH = 2;
    const int T_FRAMES = 4096;
    const int REPS = 4000;

    std::vector<double> ratios;
    volatile float sink = 0.0f;
    for (int rep = 0; rep < 9; rep++) {
        // before
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < REPS; i++) {
            Result r = run_case<v_before::SVFilter>(SIGNALFLOW_FILTER_TYPE_NOTCH,
                                                    T_NCH, T_FRAMES, 12345 + i);
            sink += r.out.empty() ? 0.0f : r.out[0];
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        // after
        for (int i = 0; i < REPS; i++) {
            Result r = run_case<v_after::SVFilter>(SIGNALFLOW_FILTER_TYPE_NOTCH,
                                                   T_NCH, T_FRAMES, 12345 + i);
            sink += r.out.empty() ? 0.0f : r.out[0];
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        double bt = std::chrono::duration<double, std::nano>(t1 - t0).count();
        double at = std::chrono::duration<double, std::nano>(t2 - t1).count();
        if (rep >= 1) ratios.push_back(bt / at); // drop warmup
    }
    std::sort(ratios.begin(), ratios.end());
    double med = ratios[ratios.size() / 2];
    printf("SPEEDUP_MEDIAN=%.4f (sink=%.3f)\n", med, (float) sink);
    return 0;
}
