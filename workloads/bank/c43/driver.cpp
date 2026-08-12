#include <vector>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <random>
using namespace std;

// Faithful VideoFrame stub: only the fields the changed code touches.
struct VideoFrame {
    int height;
    int lineSize;
    vector<uint8_t> frameData;
};

// v_before: the changed region VERBATIM from before.cpp
namespace v_before {
vector<uint8_t*> build(VideoFrame& videoFrame) {
    vector<uint8_t*> rowPointers;
    for (int i = 0; i < videoFrame.height; ++i)
    {
        rowPointers.push_back(&(videoFrame.frameData[i * videoFrame.lineSize]));
    }
    return rowPointers;
}
}

// v_after: the changed region VERBATIM from after.cpp (adds reserve)
namespace v_after {
vector<uint8_t*> build(VideoFrame& videoFrame) {
    vector<uint8_t*> rowPointers;
    rowPointers.reserve(videoFrame.height);
    for (int i = 0; i < videoFrame.height; ++i)
    {
        rowPointers.push_back(&(videoFrame.frameData[i * videoFrame.lineSize]));
    }
    return rowPointers;
}
}

static bool eq(VideoFrame& f) {
    auto a = v_before::build(f);
    auto b = v_after::build(f);
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
    return true;
}

int main() {
    // Differential correctness over diverse/boundary inputs
    vector<int> heights = {0,1,2,3,7,8,15,16,17,31,63,64,100,255,256,1000,4096,10000,65536};
    for (int h : heights) {
        int ls = (h==0)?0:16;
        VideoFrame f; f.height=h; f.lineSize=ls;
        f.frameData.resize((size_t)(h>0?h:1)*(size_t)(ls>0?ls:1)+16, 0);
        if (!eq(f)) { printf("DIVERGENCE at height=%d\n", h); return 2; }
    }
    // randomized
    std::mt19937 rng(123);
    for (int t=0;t<2000;++t){
        int h = rng()%2000;
        int ls = 1 + rng()%64;
        VideoFrame f; f.height=h; f.lineSize=ls;
        f.frameData.resize((size_t)(h>0?h:1)*(size_t)ls+64,0);
        if(!eq(f)){printf("DIVERGENCE rand height=%d ls=%d\n",h,ls);return 2;}
    }
    printf("EQUIVALENT_OK\n");

    // Timing: interleaved before/after
    const int H = 8192, LS = 16;
    VideoFrame f; f.height=H; f.lineSize=LS;
    f.frameData.resize((size_t)H*LS+64,0);
    const int ITERS = 20000;
    vector<double> rb, ra;
    volatile size_t sink=0;
    for (int rep=0; rep<21; ++rep) {
        auto t0=chrono::high_resolution_clock::now();
        for(int k=0;k<ITERS;++k){ auto v=v_before::build(f); sink+=v.size(); }
        auto t1=chrono::high_resolution_clock::now();
        for(int k=0;k<ITERS;++k){ auto v=v_after::build(f); sink+=v.size(); }
        auto t2=chrono::high_resolution_clock::now();
        rb.push_back(chrono::duration<double,nano>(t1-t0).count()/ITERS);
        ra.push_back(chrono::duration<double,nano>(t2-t1).count()/ITERS);
    }
    sort(rb.begin(),rb.end()); sort(ra.begin(),ra.end());
    double mb=rb[rb.size()/2], ma=ra[ra.size()/2];
    printf("median_before_ns=%.1f median_after_ns=%.1f speedup=%.3f sink=%zu\n", mb, ma, mb/ma, (size_t)sink);
    return 0;
}
