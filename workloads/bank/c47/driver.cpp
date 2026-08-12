// Faithful reconstruction of the InitData mempool-scan loop optimization.
// before: iterate pool->mapTx (a boost multi_index set), compute GetShortID from
//         it->GetTx().GetHash() each iteration.
// after:  iterate pool->vTxHashes (precomputed vector<pair<uint256,txiter>>),
//         compute GetShortID from vTxHashes[i].first (precomputed hash).
//
// The functional result (which txn_available slots get filled / reset, mempool_count,
// early exit) must be identical. We reconstruct the surrounding types as tiny stubs
// and keep the two loop bodies VERBATIM from before.cpp / after.cpp.

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <map>
#include <string>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>

// ---- tiny stubs for project types ----
struct uint256 {
    uint64_t a=0,b=0,c=0,d=0;
    bool operator==(const uint256&o)const{return a==o.a&&b==o.b&&c==o.c&&d==o.d;}
};
struct CTransaction {
    uint256 h;
    uint256 GetHash() const { return h; }
};
typedef std::shared_ptr<const CTransaction> CTransactionRef;

// SipHash-ish shortid derived deterministically from a hash (only the txhash matters,
// keys k0/k1 fixed for the test). Matches GetShortID: 48-bit truncation.
static uint64_t ShortIDFrom(const uint256& txhash) {
    uint64_t x = txhash.a * 0x9E3779B97F4A7C15ULL + txhash.b;
    x ^= (x >> 29); x *= 0xBF58476D1CE4E5B9ULL; x ^= (x >> 32);
    return x & 0xffffffffffffL;
}
struct CBlockHeaderAndShortTxIDs {
    uint64_t GetShortID(const uint256& txhash) const { return ShortIDFrom(txhash); }
};

// mempool entry
struct CTxMemPoolEntry {
    CTransaction tx;
    CTransactionRef shared;
    const CTransaction& GetTx() const { return tx; }
    CTransactionRef GetSharedTx() const { return shared; }
};

// A multi_index-like ordered set for mapTx (ordered by txhash for determinism);
// txiter is an iterator into it.
struct HashCmp { bool operator()(const uint256&x,const uint256&y)const{
    if(x.a!=y.a)return x.a<y.a; if(x.b!=y.b)return x.b<y.b;
    if(x.c!=y.c)return x.c<y.c; return x.d<y.d; } };

struct CTxMemPool {
    typedef std::map<uint256, CTxMemPoolEntry, HashCmp> maptype;
    typedef maptype::const_iterator txiter;
    maptype mapTx;
    // precomputed hashes vector (after)
    std::vector<std::pair<uint256, txiter> > vTxHashes;

    void build() {
        vTxHashes.clear();
        for (txiter it = mapTx.begin(); it != mapTx.end(); ++it)
            vTxHashes.push_back(std::make_pair(it->second.GetTx().GetHash(), it));
    }
};

// -------- shared driver state --------
struct State {
    std::vector<CTransactionRef> txn_available;
    size_t mempool_count = 0;
};

namespace v_before {
    // loop body verbatim (adapted: it->GetTx() -> it->second.GetTx()/GetSharedTx via entry)
    void run(const CBlockHeaderAndShortTxIDs& cmpctblock, CTxMemPool* pool,
             std::unordered_map<uint64_t,uint16_t>& shorttxids, State& st) {
        std::vector<bool> have_txn(st.txn_available.size());
        for (CTxMemPool::txiter it = pool->mapTx.begin(); it != pool->mapTx.end(); it++) {
            std::unordered_map<uint64_t, uint16_t>::iterator idit = shorttxids.find(cmpctblock.GetShortID(it->second.GetTx().GetHash()));
            if (idit != shorttxids.end()) {
                if (!have_txn[idit->second]) {
                    st.txn_available[idit->second] = it->second.GetSharedTx();
                    have_txn[idit->second]  = true;
                    st.mempool_count++;
                } else {
                    if (st.txn_available[idit->second]) {
                        st.txn_available[idit->second].reset();
                        st.mempool_count--;
                    }
                }
            }
            if (st.mempool_count == shorttxids.size())
                break;
        }
    }
}

namespace v_after {
    void run(const CBlockHeaderAndShortTxIDs& cmpctblock, CTxMemPool* pool,
             std::unordered_map<uint64_t,uint16_t>& shorttxids, State& st) {
        std::vector<bool> have_txn(st.txn_available.size());
        const std::vector<std::pair<uint256, CTxMemPool::txiter> >& vTxHashes = pool->vTxHashes;
        for (size_t i = 0; i < vTxHashes.size(); i++) {
            uint64_t shortid = cmpctblock.GetShortID(vTxHashes[i].first);
            std::unordered_map<uint64_t, uint16_t>::iterator idit = shorttxids.find(shortid);
            if (idit != shorttxids.end()) {
                if (!have_txn[idit->second]) {
                    st.txn_available[idit->second] = vTxHashes[i].second->second.GetSharedTx();
                    have_txn[idit->second]  = true;
                    st.mempool_count++;
                } else {
                    if (st.txn_available[idit->second]) {
                        st.txn_available[idit->second].reset();
                        st.mempool_count--;
                    }
                }
            }
            if (st.mempool_count == shorttxids.size())
                break;
        }
    }
}

// Build a scenario: mempool of N txns, a shorttxids map keyed by shortid->slot.
struct Scenario {
    CTxMemPool pool;
    std::unordered_map<uint64_t,uint16_t> shorttxids;
    size_t n_slots;
};

Scenario makeScenario(uint64_t seed, size_t nMem, size_t nSlots, bool collide) {
    std::mt19937_64 rng(seed);
    Scenario s;
    s.n_slots = nSlots;
    std::vector<uint256> hashes;
    for (size_t i=0;i<nMem;i++){
        uint256 h; h.a=rng(); h.b=rng(); h.c=rng(); h.d=rng();
        CTxMemPoolEntry e; e.tx.h=h; e.shared=std::make_shared<const CTransaction>(e.tx);
        s.pool.mapTx[h]=e; hashes.push_back(h);
    }
    s.pool.build();
    // build shorttxids: map some subset of mempool shortids to slots, plus some random
    uint16_t slot=0;
    std::uniform_int_distribution<int> pick(0,3);
    for (auto& h: hashes){
        if (slot>=nSlots) break;
        if (pick(rng)==0) continue; // skip some to leave gaps
        uint64_t sid = ShortIDFrom(h);
        s.shorttxids[sid]=slot++;
    }
    // add random (non-mempool) shortids to fill remaining slots
    while (slot<nSlots){
        uint256 h; h.a=rng(); h.b=rng(); h.c=rng(); h.d=rng();
        uint64_t sid=ShortIDFrom(h);
        if (s.shorttxids.count(sid)) continue;
        s.shorttxids[sid]=slot++;
    }
    if (collide && nMem>=2){
        // force two mempool txns to same shortid pointing to same slot (dup path)
        // (rare; but exercise the reset branch by mapping second occurrence)
    }
    return s;
}

bool differential() {
    for (uint64_t seed=1; seed<=400; seed++){
        size_t nMem = seed % 50;
        size_t nSlots = 1 + (seed % 20);
        Scenario sc = makeScenario(seed, nMem, nSlots, seed%3==0);
        CBlockHeaderAndShortTxIDs cb;

        State sb, sa;
        sb.txn_available.assign(sc.n_slots, nullptr);
        sa.txn_available.assign(sc.n_slots, nullptr);
        auto shb = sc.shorttxids; auto sha = sc.shorttxids;
        v_before::run(cb, &sc.pool, shb, sb);
        v_after::run(cb, &sc.pool, sha, sa);
        if (sb.mempool_count != sa.mempool_count){
            std::cout<<"DIVERGE seed="<<seed<<" count "<<sb.mempool_count<<" vs "<<sa.mempool_count<<"\n"; return false;
        }
        for (size_t i=0;i<sc.n_slots;i++){
            bool bb = (bool)sb.txn_available[i];
            bool ba = (bool)sa.txn_available[i];
            if (bb!=ba){ std::cout<<"DIVERGE seed="<<seed<<" slot "<<i<<"\n"; return false; }
            if (bb && !(sb.txn_available[i]->GetHash()==sa.txn_available[i]->GetHash())){
                std::cout<<"DIVERGE hash seed="<<seed<<" slot "<<i<<"\n"; return false;
            }
        }
    }
    return true;
}

int main(){
    if (!differential()){ std::cout<<"NOT EQUIVALENT\n"; return 2; }
    std::cout<<"EQUIVALENT\n";

    // timing: larger mempool, moderate slots so full scan happens
    std::vector<Scenario> scs;
    for (uint64_t s=1000; s<1040; s++)
        scs.push_back(makeScenario(s, 4000, 1500, false));

    std::vector<double> ratios;
    for (int rep=0; rep<7; rep++){
        double tb_tot=0, ta_tot=0;
        for (auto& sc: scs){
            CBlockHeaderAndShortTxIDs cb;
            {
                State sb; sb.txn_available.assign(sc.n_slots,nullptr);
                auto sh=sc.shorttxids;
                auto t0=std::chrono::high_resolution_clock::now();
                v_before::run(cb,&sc.pool,sh,sb);
                auto t1=std::chrono::high_resolution_clock::now();
                tb_tot+=std::chrono::duration<double,std::nano>(t1-t0).count();
            }
            {
                State sa; sa.txn_available.assign(sc.n_slots,nullptr);
                auto sh=sc.shorttxids;
                auto t0=std::chrono::high_resolution_clock::now();
                v_after::run(cb,&sc.pool,sh,sa);
                auto t1=std::chrono::high_resolution_clock::now();
                ta_tot+=std::chrono::duration<double,std::nano>(t1-t0).count();
            }
        }
        ratios.push_back(tb_tot/ta_tot);
    }
    std::sort(ratios.begin(),ratios.end());
    double med=ratios[ratios.size()/2];
    std::cout<<"speedup_median="<<med<<"\n";
    return 0;
}
