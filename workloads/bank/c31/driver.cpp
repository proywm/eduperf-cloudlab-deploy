// Self-contained differential harness for MrMC commit 88f67f9c8024
// audio_process: manual 64-byte chunked memcpy+Write loop  ->  single pipe->Write.
// Behavior-preserving I/O optimization: removes redundant memcpy into a stack
// buffer and collapses N chunked writes into one write.
//
// Faithful reconstruction. Only trivial stub: CPipeFile with a Write() that
// captures all bytes it is handed (models a pipe that accepts the full buffer),
// returning the number of bytes written.

#include <cstring>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cstdio>
#include <cassert>

// --- trivial stub for the project type XFILE::CPipeFile -----------------------
namespace XFILE {
  struct CPipeFile {
    std::vector<unsigned char> sink;   // captured bytes = observable state
    // Real CPipeFile::Write returns number of bytes written (ssize_t-like).
    // A healthy pipe accepts the whole request; model that faithfully.
    int Write(const void* buf, int len) {
      if (len <= 0) return 0;
      const unsigned char* p = (const unsigned char*)buf;
      sink.insert(sink.end(), p, p + len);
      return len;
    }
  };
}

// --- BEFORE -------------------------------------------------------------------
namespace v_before {
  void audio_process(void *cls, void *session, const void *buffer, int buflen)
  {
    #define NUM_OF_BYTES 64
    XFILE::CPipeFile *pipe=(XFILE::CPipeFile *)cls;
    int sentBytes = 0;
    unsigned char buf[NUM_OF_BYTES];

    while (sentBytes < buflen)
    {
      int n = (buflen - sentBytes < NUM_OF_BYTES ? buflen - sentBytes : NUM_OF_BYTES);
      memcpy(buf, (char*) buffer + sentBytes, n);

      if (pipe->Write(buf, n) == 0)
        return;

      sentBytes += n;
    }
    #undef NUM_OF_BYTES
  }
}

// --- AFTER --------------------------------------------------------------------
namespace v_after {
  void audio_process(void *cls, void *session, const void *buffer, int buflen)
  {
    XFILE::CPipeFile *pipe=(XFILE::CPipeFile *)cls;
    pipe->Write(buffer, buflen);
  }
}

// -----------------------------------------------------------------------------
static bool run_case(const std::vector<unsigned char>& data, int buflen) {
  XFILE::CPipeFile pb, pa;
  const void* bufptr = data.empty() ? (const void*)"" : (const void*)data.data();
  v_before::audio_process(&pb, nullptr, bufptr, buflen);
  v_after ::audio_process(&pa, nullptr, bufptr, buflen);
  return pb.sink == pa.sink;
}

int main() {
  std::mt19937 rng(12345);

  // ---- differential correctness over a diverse/boundary/adversarial battery ----
  bool ok = true;
  std::string firstdiv;

  // boundary lengths around the 64-byte chunk size and 0/negative
  std::vector<int> lens = {-5,-1,0,1,2,63,64,65,127,128,129,191,192,193,
                           255,256,257,1000,4096,65535};
  for (int L : lens) {
    int alloc = L > 0 ? L : 1;
    std::vector<unsigned char> d(alloc);
    for (auto &b : d) b = (unsigned char)(rng() & 0xFF);
    if (!run_case(d, L)) { ok=false; if(firstdiv.empty()) firstdiv="len="+std::to_string(L); }
  }
  // random fuzz
  for (int it=0; it<20000 && ok; ++it) {
    int L = (int)(rng() % 2049);
    std::vector<unsigned char> d(L>0?L:1);
    for (auto &b : d) b = (unsigned char)(rng() & 0xFF);
    if (!run_case(d, L)) { ok=false; if(firstdiv.empty()) firstdiv="fuzz L="+std::to_string(L); }
  }
  printf("equivalent=%s%s%s\n", ok?"true":"false",
         ok?"":"  first_div=", ok?"":firstdiv.c_str());

  // ---- interleaved timing ----
  // realistic audio_process payload sizes (a few KB); interleave to cancel drift
  std::vector<int> tlens = {352, 1024, 4096, 8192};
  std::vector<std::vector<unsigned char>> payloads;
  for (int L : tlens) {
    std::vector<unsigned char> d(L);
    for (auto &b : d) b = (unsigned char)(rng() & 0xFF);
    payloads.push_back(std::move(d));
  }
  const int REPS = 4000;
  std::vector<double> ratios;
  for (size_t pi=0; pi<payloads.size(); ++pi) {
    auto& d = payloads[pi]; int L = tlens[pi];
    volatile size_t sink_guard = 0;
    // before
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int r=0;r<REPS;++r){ XFILE::CPipeFile p; v_before::audio_process(&p,nullptr,d.data(),L); sink_guard+=p.sink.size(); }
    auto t1 = std::chrono::high_resolution_clock::now();
    // after
    for (int r=0;r<REPS;++r){ XFILE::CPipeFile p; v_after::audio_process(&p,nullptr,d.data(),L); sink_guard+=p.sink.size(); }
    auto t2 = std::chrono::high_resolution_clock::now();
    double bns = std::chrono::duration<double,std::nano>(t1-t0).count();
    double ans = std::chrono::duration<double,std::nano>(t2-t1).count();
    ratios.push_back(bns/ans);
    printf("len=%d before_ns=%.0f after_ns=%.0f ratio=%.3f guard=%zu\n",
           L, bns/REPS, ans/REPS, bns/ans, (size_t)sink_guard);
  }
  std::sort(ratios.begin(), ratios.end());
  double med = ratios[ratios.size()/2];
  printf("median_speedup=%.3f\n", med);
  return 0;
}
