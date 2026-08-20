#include <cstdint>
#include <cstdio>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

// Faithful reproduction of the real TwoFoldStack from src/search.h
struct TwoFoldStack {
    uint64_t keys[256];
    int rootEnd;
    int length;
    TwoFoldStack() { rootEnd = 0; length = 0; }
    void push(uint64_t pos) { keys[length] = pos; length++; }
    void pop() { length--; }
    void clear() { rootEnd = 0; length = 0; }
    void setRootEnd() { rootEnd = length - 1; }
    bool find(uint64_t pos) {
        for (int i = length-1; i >= 0; i--) {
            if (keys[i] == pos) {
                if (i <= rootEnd) {
                    for (int j = i-1; j >= 0; j--) {
                        if (keys[j] == pos) return true;
                    }
                }
                else return true;
            }
        }
        return false;
    }
};

// Tiny faithful Board stub: exposes only what the changed line touches.
struct Board {
    bool draw;
    int fiftyMoveCounter;
    uint64_t zobristKey;
    bool isDraw() const { return draw; }
    int getFiftyMoveCounter() const { return fiftyMoveCounter; }
    uint64_t getZobristKey() const { return zobristKey; }
};

// The changed predicate reproduced verbatim from the PVS draw-check block.
// Returns true iff the search returns 0 (draw) at this check.
namespace v_before {
    bool drawCheck(Board &b, TwoFoldStack &tf) {
        if (b.isDraw())
            return true;
        if (tf.find(b.getZobristKey()))
            return true;
        return false;
    }
}
namespace v_after {
    bool drawCheck(Board &b, TwoFoldStack &tf) {
        if (b.isDraw())
            return true;
        if (b.getFiftyMoveCounter() >= 2 && tf.find(b.getZobristKey()))
            return true;
        return false;
    }
}

int main() {
    std::mt19937_64 rng(12345);

    // ---- Differential test ----
    // Model the real chess invariant that the commit relies on:
    //   a two-fold repetition (a matching key in the stack) can only exist
    //   when at least 2 reversible moves have elapsed -> fiftyMoveCounter >= 2.
    // We build FAITHFUL inputs honoring that, plus an ADVERSARIAL sweep that
    // deliberately violates it to detect divergence.
    long long mism_faithful = 0, mism_adversarial = 0;
    std::string first_div;

    auto build_stack = [&](TwoFoldStack &tf, int len, int rootEnd,
                           uint64_t probe, bool insertMatch, int matchPos) {
        tf.clear();
        for (int i = 0; i < len; i++) tf.keys[i] = rng() | 1; // nonzero randoms
        tf.length = len;
        tf.rootEnd = rootEnd;
        if (insertMatch && len > 0) {
            int p = matchPos % len;
            tf.keys[p] = probe;
        }
    };

    // Faithful battery: keep the invariant (if a match is present, counter>=2)
    for (int iter = 0; iter < 200000; iter++) {
        Board b;
        b.draw = (rng() % 8 == 0);
        uint64_t probe = rng() | 1;
        b.zobristKey = probe;
        int len = rng() % 40;
        int rootEnd = len ? (int)(rng() % len) : 0;
        bool insertMatch = (rng() % 3 == 0);
        // may insert a second copy to make find() succeed in-tree
        bool insertSecond = insertMatch && (rng() % 2 == 0);
        TwoFoldStack tf;
        build_stack(tf, len, rootEnd, probe, insertMatch, (int)(rng()%(len?len:1)));
        if (insertSecond && len >= 2) {
            int p2 = (int)(rng() % len);
            tf.keys[p2] = probe;
        }
        // Enforce invariant: if any key equals probe, set counter>=2.
        bool hasMatch = tf.find(probe);
        if (hasMatch) b.fiftyMoveCounter = 2 + (int)(rng() % 100);
        else b.fiftyMoveCounter = (int)(rng() % 200); // free when no match

        Board b1 = b, b2 = b;
        TwoFoldStack t1 = tf, t2 = tf;
        bool r1 = v_before::drawCheck(b1, t1);
        bool r2 = v_after::drawCheck(b2, t2);
        if (r1 != r2) {
            mism_faithful++;
            if (first_div.empty()) {
                char buf[256];
                snprintf(buf, sizeof buf,
                    "FAITHFUL draw=%d fifty=%d len=%d rootEnd=%d before=%d after=%d",
                    b.draw, b.fiftyMoveCounter, len, rootEnd, r1, r2);
                first_div = buf;
            }
        }
    }

    // Adversarial battery: deliberately allow counter<2 with a real match.
    for (int iter = 0; iter < 200000; iter++) {
        Board b;
        b.draw = false; // isolate the two-fold branch
        uint64_t probe = rng() | 1;
        b.zobristKey = probe;
        int len = 1 + rng() % 40;
        int rootEnd = (int)(rng() % len);
        TwoFoldStack tf;
        build_stack(tf, len, rootEnd, probe, true, (int)(rng()%len));
        if (len >= 2 && (rng()%2)) { int p2=(int)(rng()%len); tf.keys[p2]=probe; }
        b.fiftyMoveCounter = (int)(rng() % 2); // 0 or 1 -> violates invariant

        Board b1 = b, b2 = b;
        TwoFoldStack t1 = tf, t2 = tf;
        bool r1 = v_before::drawCheck(b1, t1);
        bool r2 = v_after::drawCheck(b2, t2);
        if (r1 != r2) mism_adversarial++;
    }

    // ---- Timing (interleaved) ----
    // Realistic mix: mostly counter<2 (common in search) so the guard pays off,
    // plus some counter>=2 cases. Honors the invariant.
    const int N = 4000;
    std::vector<Board> boards(N);
    std::vector<TwoFoldStack> stacks(N);
    for (int i = 0; i < N; i++) {
        Board &b = boards[i];
        b.draw = false;
        uint64_t probe = rng() | 1;
        b.zobristKey = probe;
        TwoFoldStack &tf = stacks[i];
        int len = 20 + rng()%236; // deep stacks, realistic
        for (int k=0;k<len;k++) tf.keys[k]=rng()|1;
        tf.length = len; tf.rootEnd = len? (int)(rng()%len):0;
        // 70% of nodes have low reversible-move count (guard skips the scan)
        b.fiftyMoveCounter = (rng()%10 < 7) ? (int)(rng()%2) : (2+(int)(rng()%80));
        // never insert a match (search overwhelmingly finds none) -> full scan in BEFORE
    }

    std::vector<double> ratios;
    volatile long long sink = 0;
    for (int rep = 0; rep < 200; rep++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i=0;i<N;i++){ Board b=boards[i]; TwoFoldStack tf=stacks[i]; sink += v_before::drawCheck(b,tf); }
        auto t1 = std::chrono::high_resolution_clock::now();
        for (int i=0;i<N;i++){ Board b=boards[i]; TwoFoldStack tf=stacks[i]; sink += v_after::drawCheck(b,tf); }
        auto t2 = std::chrono::high_resolution_clock::now();
        double bns = std::chrono::duration<double,std::nano>(t1-t0).count();
        double ans = std::chrono::duration<double,std::nano>(t2-t1).count();
        if (ans > 0) ratios.push_back(bns/ans);
    }
    std::sort(ratios.begin(), ratios.end());
    double med = ratios[ratios.size()/2];

    printf("faithful_cases=200000\n");
    printf("faithful_mismatches=%lld\n", mism_faithful);
    printf("adversarial_cases=200000\n");
    printf("adversarial_mismatches=%lld\n", mism_adversarial);
    if (!first_div.empty()) printf("first_div=%s\n", first_div.c_str());
    printf("median_speedup=%.4f\n", med);
    printf("sink=%lld\n", (long long)sink);
    return 0;
}
