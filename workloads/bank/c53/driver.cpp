#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

typedef unsigned int uint;
typedef std::string QString; // faithful stub: value-comparison semantics identical

// ---- BEFORE ----
namespace v_before {
struct styleStruct {
    QString key;
    uint state;
    uint direction;
    uint complex;
    uint palette;
    int width;
    int height;
    bool operator==(const styleStruct &str) const
    {
        return str.key == key && str.state == state && str.direction == direction
                && str.complex == complex && str.palette == palette && str.width == width
                && str.height == height;
    }
};
}

// ---- AFTER ----
namespace v_after {
struct styleStruct {
    QString key;
    uint state;
    uint direction;
    uint complex;
    uint palette;
    int width;
    int height;
    bool operator==(const styleStruct &str) const
    {
        return  str.state == state && str.direction == direction
                && str.complex == complex && str.palette == palette && str.width == width
                && str.height == height && str.key == key;
    }
};
}

struct Fields {
    std::string key;
    uint state, direction, complex, palette;
    int width, height;
};

template<class S> S make(const Fields& f){ S s; s.key=f.key; s.state=f.state; s.direction=f.direction; s.complex=f.complex; s.palette=f.palette; s.width=f.width; s.height=f.height; return s; }

int main(){
    std::mt19937_64 rng(12345);
    std::vector<Fields> corpus;
    // boundary/adversarial: many with identical cheap fields but different keys,
    // identical keys but differing cheap fields, all-equal, all-different, empty keys, long keys.
    std::vector<std::string> keys = {"", "a", "my-progressbar-1", "my-progressbar-2",
        std::string(1000,'x'), std::string(1000,'x')+"y", "zzz"};
    std::uniform_int_distribution<uint> du(0,3);
    for(int i=0;i<400;i++){
        Fields f;
        f.key=keys[rng()%keys.size()];
        f.state=du(rng); f.direction=du(rng); f.complex=du(rng); f.palette=du(rng);
        f.width=(int)du(rng); f.height=(int)du(rng);
        corpus.push_back(f);
    }

    // differential correctness over all pairs
    long mismatches=0; std::string first_div;
    for(size_t i=0;i<corpus.size();i++) for(size_t j=0;j<corpus.size();j++){
        auto ba=make<v_before::styleStruct>(corpus[i]);
        auto bb=make<v_before::styleStruct>(corpus[j]);
        auto aa=make<v_after::styleStruct>(corpus[i]);
        auto ab=make<v_after::styleStruct>(corpus[j]);
        bool rb = (ba==bb);
        bool ra = (aa==ab);
        if(rb!=ra){ if(mismatches==0){ first_div="i="+std::to_string(i)+" j="+std::to_string(j); } mismatches++; }
    }
    std::cout << "mismatches=" << mismatches << (mismatches?(" first="+first_div):"") << "\n";

    // timing: build a realistic workload. Keys mostly identical (short-circuit relevance):
    // compare a fixed probe against corpus where cheap fields often differ.
    std::vector<Fields> probe_src;
    for(int i=0;i<2000;i++){
        Fields f; f.key = std::string(64,'k'); // long common key
        f.state=du(rng); f.direction=du(rng); f.complex=du(rng); f.palette=du(rng);
        f.width=(int)du(rng); f.height=(int)du(rng);
        probe_src.push_back(f);
    }
    std::vector<v_before::styleStruct> B; std::vector<v_after::styleStruct> A;
    for(auto&f:probe_src){ B.push_back(make<v_before::styleStruct>(f)); A.push_back(make<v_after::styleStruct>(f)); }
    Fields pf; pf.key=std::string(64,'k'); pf.state=0;pf.direction=0;pf.complex=0;pf.palette=0;pf.width=0;pf.height=0;
    auto pb=make<v_before::styleStruct>(pf); auto pa=make<v_after::styleStruct>(pf);

    const int REP=2000;
    std::vector<double> sb, sa;
    volatile long sink=0;
    for(int r=0;r<21;r++){
        {
        auto t0=std::chrono::high_resolution_clock::now();
        long c=0; for(int rep=0; rep<REP; rep++) for(auto&x:B) c+=(x==pb);
        auto t1=std::chrono::high_resolution_clock::now();
        sink+=c; sb.push_back(std::chrono::duration<double,std::nano>(t1-t0).count());
        }
        {
        auto t0=std::chrono::high_resolution_clock::now();
        long c=0; for(int rep=0; rep<REP; rep++) for(auto&x:A) c+=(x==pa);
        auto t1=std::chrono::high_resolution_clock::now();
        sink+=c; sa.push_back(std::chrono::duration<double,std::nano>(t1-t0).count());
        }
    }
    std::sort(sb.begin(),sb.end()); std::sort(sa.begin(),sa.end());
    double mb=sb[sb.size()/2], ma=sa[sa.size()/2];
    std::cout << "median_before_ns=" << mb << " median_after_ns=" << ma
              << " speedup=" << (mb/ma) << " sink=" << (long)sink << "\n";
    return 0;
}
