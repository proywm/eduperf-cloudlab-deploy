#include <vector>
#include <array>
#include <cmath>

struct vector3 {
    float x, y, z;
    vector3 operator+(const vector3& other) const { return {x + other.x, y + other.y, z + other.z}; }
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

struct vector_point_norm {
    std::vector<vector3> p;
    std::vector<vector3> n;
};

namespace v_before {
void gen_cone_triangles(std::vector<vert_norm_tc>& verts, const vector_point_norm& vpn, int ndiv, bool two_sided_lighting, const vector3& xlate) {
    verts.resize(3 * ndiv);
    float const ndiv_inv(1.0 / ndiv);

    for (unsigned s = 0; s < (unsigned)ndiv; ++s) { // Note: always has tex coords
        unsigned const sp((s + ndiv - 1) % ndiv), sn((s + 1) % ndiv), vix(3 * s);
        verts[vix + 0].assign(vpn.p[(s << 1) + 1] + xlate, vpn.n[s], 0.5, 0.0f); // one big discontinuity at one position
        verts[vix + 1].assign(vpn.p[(sn << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sn]), (1.0 - (s + 1.0) * ndiv_inv), 0.0f); // normalize?
        verts[vix + 2].assign(vpn.p[(s << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sp]), (1.0 - (s + 0.0) * ndiv_inv), 0.0f); // normalize?
    }
}
}

namespace v_after {
void gen_cone_triangles(std::vector<vert_norm_tc>& verts, const vector_point_norm& vpn, int ndiv, bool two_sided_lighting, const vector3& xlate) {
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
            verts[vix + 0].assign(vpn.p[(s << 1) + 1] + xlate, vpn.n[s], 0.5, 0.0f); // one big discontinuity at one position
            verts[vix + 1].assign(vpn.p[(sn << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sn]), (1.0 - (s + 1.0) * ndiv_inv), 0.0f); // normalize?
            verts[vix + 2].assign(vpn.p[(s << 1) + 0] + xlate, (vpn.n[s] + vpn.n[sp]), (1.0 - (s + 0.0) * ndiv_inv), 0.0f); // normalize?
        }
    }
}
}

static const int REPS = 50;

static long long work(int version) {
    std::vector<vert_norm_tc> verts;
    vector_point_norm vpn;
    vpn.p.resize(10);
    vpn.n.resize(10);
    for (int i = 0; i < 10; ++i) {
        vpn.p[i] = {static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3)};
        vpn.n[i] = {static_cast<float>(i * 4), static_cast<float>(i * 5), static_cast<float>(i * 6)};
    }
    int ndiv = 10;
    bool two_sided_lighting = false;
    vector3 xlate = {0, 0, 0};

    long long checksum = 0;

    for (int i = 0; i < REPS; ++i) {
        verts.clear();
        if (version == 0) {
            v_before::gen_cone_triangles(verts, vpn, ndiv, two_sided_lighting, xlate);
        } else {
            v_after::gen_cone_triangles(verts, vpn, ndiv, two_sided_lighting, xlate);
        }
        for (const auto& vert : verts) {
            checksum += static_cast<long long>(vert.pos.x) + static_cast<long long>(vert.pos.y) + static_cast<long long>(vert.pos.z);
            checksum += static_cast<long long>(vert.norm.x) + static_cast<long long>(vert.norm.y) + static_cast<long long>(vert.norm.z);
            checksum += static_cast<long long>(vert.texCoord);
        }
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
    auto best_of = [](int v)->long long{
        long long best = LLONG_MAX;
        for(int r=0;r<9;r++){
            auto t0=clk::now();
            volatile long long sink=0;
            for(int k=0;k<REPS;k++) sink += work(v);
            auto t1=clk::now();
            long long ns=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
            if(ns<best) best=ns;
        }
        return best;
    };
    // interleave before/after to control for drift; take min over passes
    long long b=LLONG_MAX,a=LLONG_MAX;
    for(int pass=0;pass<3;pass++){
        long long bb=best_of(0); if(bb<b) b=bb;
        long long aa=best_of(1); if(aa<a) a=aa;
    }
    printf("EQUIV=%d\n", (c0==c1)?1:0);
    printf("BEFORE_NS=%lld\n", b);
    printf("AFTER_NS=%lld\n", a);
    printf("READY=1\n");
    return 0;
}
