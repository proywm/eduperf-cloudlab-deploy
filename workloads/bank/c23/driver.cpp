#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

struct vector3 {
    float x, y, z;
    vector3 operator+(const vector3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    vector3 operator-() const { return {-x, -y, -z}; }
};

struct vert_norm_tc {
    vector3 pos;
    vector3 norm;
    float texCoord;
    void assign(const vector3& p, const vector3& n, float tc, float /*unused*/) {
        pos = p;
        norm = n;
        texCoord = tc;
    }
};

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

static const vector3 camera_position = {19.0f, -7.0f, 13.0f};

static vector3 calc_oriented_normal(const vector3& p, const vector3& n, bool two_sided_lighting) {
    const float facing = n.x * (camera_position.x - p.x)
        + n.y * (camera_position.y - p.y) + n.z * (camera_position.z - p.z);
    return (two_sided_lighting && facing < 0.0f) ? -n : n;
}

NOINLINE static void create_vert(vert_norm_tc& v, const vector3& p, const vector3& n,
                                  float tc, float unused, bool two_sided_lighting) {
    v.assign(p, calc_oriented_normal(p, n, two_sided_lighting), tc, unused);
}

struct vector_point_norm {
    std::vector<vector3> p;
    std::vector<vector3> n;
};

namespace v_before {
NOINLINE void gen_cone_triangles(std::vector<vert_norm_tc>& verts, const vector_point_norm& vpn, int ndiv, bool two_sided_lighting, const vector3& xlate) {
    verts.resize(3 * ndiv);
    float const ndiv_inv(1.0 / ndiv);

    for (unsigned s = 0; s < (unsigned)ndiv; ++s) { // Note: always has tex coords
        unsigned const sp((s + ndiv - 1) % ndiv), sn((s + 1) % ndiv), vix(3 * s);
        create_vert(verts[vix + 0], vpn.p[(s << 1) + 1] + xlate, vpn.n[s], 0.5, 0.0f, two_sided_lighting); // one big discontinuity at one position
        create_vert(verts[vix + 1], vpn.p[(sn << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sn]), (1.0 - (s + 1.0) * ndiv_inv), 0.0f, two_sided_lighting); // normalize?
        create_vert(verts[vix + 2], vpn.p[(s << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sp]), (1.0 - (s + 0.0) * ndiv_inv), 0.0f, two_sided_lighting); // normalize?
    }
}
}

namespace v_after {
NOINLINE void gen_cone_triangles(std::vector<vert_norm_tc>& verts, const vector_point_norm& vpn, int ndiv, bool two_sided_lighting, const vector3& xlate) {
    verts.resize(3 * ndiv);
    float const ndiv_inv(1.0 / ndiv);

    if (!two_sided_lighting && xlate.x == 0 && xlate.y == 0 && xlate.z == 0) { // common case optimization, for example for tree trunks
        for (unsigned s = 0; s < (unsigned)ndiv; ++s) { // Note: always has tex coords
            unsigned const sp((s + ndiv - 1) % ndiv), sn((s + 1) % ndiv), vix(3 * s);
            verts[vix + 0].assign(vpn.p[(s << 1) + 1], vpn.n[s], 0.5, 0.0f); // one big discontinuity at one position
            verts[vix + 1].assign(vpn.p[(sn << 1) + 0], (vpn.n[s] + vpn.n[sn]), (1.0 - (s + 1.0) * ndiv_inv), 0.0f); // normalize?
            verts[vix + 2].assign(vpn.p[(s << 1) + 0], (vpn.n[s] + vpn.n[sp]), (1.0 - (s + 0.0) * ndiv_inv), 0.0f); // normalize?
        }
    } else {
        for (unsigned s = 0; s < (unsigned)ndiv; ++s) { // Note: always has tex coords
            unsigned const sp((s + ndiv - 1) % ndiv), sn((s + 1) % ndiv), vix(3 * s);
            create_vert(verts[vix + 0], vpn.p[(s << 1) + 1] + xlate, vpn.n[s], 0.5, 0.0f, two_sided_lighting); // one big discontinuity at one position
            create_vert(verts[vix + 1], vpn.p[(sn << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sn]), (1.0 - (s + 1.0) * ndiv_inv), 0.0f, two_sided_lighting); // normalize?
            create_vert(verts[vix + 2], vpn.p[(s << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sp]), (1.0 - (s + 0.0) * ndiv_inv), 0.0f, two_sided_lighting); // normalize?
        }
    }
}
}

static const int NDIV = 4096;
static const int REPS = 384;

static vector_point_norm make_input(int ndiv) {
    vector_point_norm vpn;
    vpn.p.resize(2 * ndiv);
    vpn.n.resize(ndiv);
    for (int i = 0; i < 2 * ndiv; ++i) {
        vpn.p[i] = {
            static_cast<float>((i * 17) % 101) / 7.0f,
            static_cast<float>((i * 29) % 97) / 11.0f,
            static_cast<float>((i * 43) % 89) / 13.0f,
        };
    }
    for (int i = 0; i < ndiv; ++i) {
        vpn.n[i] = {
            static_cast<float>((i * 11) % 31) / 17.0f,
            static_cast<float>((i * 7) % 37) / 19.0f,
            static_cast<float>((i * 5) % 41) / 23.0f,
        };
    }
    return vpn;
}

static std::uint64_t mix_float(std::uint64_t hash, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return (hash ^ bits) * 1099511628211ULL;
}

static std::uint64_t checksum(const std::vector<vert_norm_tc>& verts) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& vert : verts) {
        hash = mix_float(hash, vert.pos.x); hash = mix_float(hash, vert.pos.y); hash = mix_float(hash, vert.pos.z);
        hash = mix_float(hash, vert.norm.x); hash = mix_float(hash, vert.norm.y); hash = mix_float(hash, vert.norm.z);
        hash = mix_float(hash, vert.texCoord);
    }
    return hash;
}

static void generate(int version, std::vector<vert_norm_tc>& verts, const vector_point_norm& vpn,
                     int ndiv, bool two_sided_lighting, const vector3& xlate) {
    if (version == 0) v_before::gen_cone_triangles(verts, vpn, ndiv, two_sided_lighting, xlate);
    else v_after::gen_cone_triangles(verts, vpn, ndiv, two_sided_lighting, xlate);
}

static bool equivalent_for(int ndiv, bool two_sided_lighting, const vector3& xlate) {
    const auto vpn = make_input(ndiv);
    std::vector<vert_norm_tc> before, after;
    generate(0, before, vpn, ndiv, two_sided_lighting, xlate);
    generate(1, after, vpn, ndiv, two_sided_lighting, xlate);
    return checksum(before) == checksum(after);
}

static long long work(int version) {
    std::vector<vert_norm_tc> verts;
    const vector_point_norm vpn = make_input(NDIV);
    const bool two_sided_lighting = false;
    const vector3 xlate = {0, 0, 0};
    std::uint64_t result = 0;
    for (int i = 0; i < REPS; ++i) {
        generate(version, verts, vpn, NDIV, two_sided_lighting, xlate);
        result ^= mix_float(result + static_cast<std::uint64_t>(i), verts[(i * 97) % verts.size()].pos.x);
    }
    return static_cast<long long>(result);
}

// ===== fixed harness (appended; do not edit) =====
#include <cstdio>
#include <chrono>
#include <climits>
#include <algorithm>
int main(){
    const bool common_small = equivalent_for(3, false, {0, 0, 0});
    const bool common_large = equivalent_for(127, false, {0, 0, 0});
    const bool translated = equivalent_for(31, false, {1.5f, -2.0f, 0.25f});
    const bool two_sided = equivalent_for(31, true, {0, 0, 0});
    const bool equivalent = common_small && common_large && translated && two_sided;
    using clk = std::chrono::steady_clock;
    auto measure = [](int v)->long long{
        auto t0=clk::now();
        volatile long long sink=work(v);
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
    printf("EQUIV=%d\n", equivalent?1:0);
    printf("EQUIV_DOMAINS common-small=%d common-large=%d translated=%d two-sided=%d\n",
           common_small?1:0, common_large?1:0, translated?1:0, two_sided?1:0);
    printf("BEFORE_NS=%lld\n", b);
    printf("AFTER_NS=%lld\n", a);
    printf("READY=1\n");
    return 0;
}
