// Differential + timing harness for unit smp_SendFrame_0483c88f1e69
// Method: CEXIETHERNET::SendFrame (Dolphin, BBA-TAP Win32)
// Real change (change.diff), the "Copy to write buffer" step:
//   BEFORE: mWriteBuffer.resize(size); memcpy(mWriteBuffer.data(), frame, size);
//   AFTER : mWriteBuffer.assign(frame, frame + size);
// mWriteBuffer is std::vector<u8>, reused across calls (reserve(1518) in Activate()).
// Only this copy step changed; the rest of SendFrame is Win32 async I/O and is unchanged.

#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <random>
#include <algorithm>
#include <string>

using u8 = uint8_t;
using u32 = uint32_t;

namespace v_before {
void copy_to_buffer(std::vector<u8>& mWriteBuffer, const u8* frame, u32 size)
{
  // Copy to write buffer.
  mWriteBuffer.resize(size);
  memcpy(mWriteBuffer.data(), frame, size);
}
}

namespace v_after {
void copy_to_buffer(std::vector<u8>& mWriteBuffer, const u8* frame, u32 size)
{
  // Copy to write buffer.
  mWriteBuffer.assign(frame, frame + size);
}
}

int main()
{
  std::mt19937_64 rng(0xC0FFEEULL);

  // Boundary/diverse/adversarial frame sizes.
  std::vector<size_t> sizes = {0, 1, 2, 3, 7, 8, 15, 16, 31, 32, 60, 63, 64, 100,
                               127, 128, 255, 256, 511, 512, 1000, 1023, 1024,
                               1500, 1517, 1518, 1519, 2000, 4096, 8192, 65535};

  bool equivalent = true;
  std::string divergent = "none";

  // Reused persistent buffers to mimic SendFrame's reuse of mWriteBuffer across calls,
  // including the case where the buffer already holds stale/larger content.
  for (int trial = 0; trial < 4 && equivalent; ++trial)
  {
    std::vector<u8> buf_before, buf_after;
    buf_before.reserve(1518);
    buf_after.reserve(1518);
    // Pre-seed with junk of varying length so resize/assign must overwrite/shrink/grow.
    for (int seed = 0; seed < 3 && equivalent; ++seed)
    {
      size_t pre = (size_t)(rng() % 3000);
      buf_before.assign(pre, (u8)(rng() & 0xFF));
      buf_after = buf_before;

      for (size_t sz : sizes)
      {
        std::vector<u8> frame(sz);
        for (auto& b : frame) b = (u8)(rng() & 0xFF);

        v_before::copy_to_buffer(buf_before, frame.data(), (u32)sz);
        v_after::copy_to_buffer(buf_after, frame.data(), (u32)sz);

        bool same = (buf_before.size() == buf_after.size()) &&
                    (sz == 0 || memcmp(buf_before.data(), buf_after.data(), sz) == 0);
        if (!same)
        {
          equivalent = false;
          divergent = "trial=" + std::to_string(trial) + " seed=" + std::to_string(seed) +
                      " size=" + std::to_string(sz);
          break;
        }
      }
    }
  }

  printf("equivalent=%d divergent=[%s]\n", equivalent ? 1 : 0, divergent.c_str());

  // Timing: interleaved before/after over realistic ethernet frame-size distribution.
  std::vector<std::vector<u8>> frames;
  std::uniform_int_distribution<int> szdist(60, 1518);
  for (int i = 0; i < 20000; ++i)
  {
    int sz = szdist(rng);
    std::vector<u8> f(sz);
    for (auto& b : f) b = (u8)(rng() & 0xFF);
    frames.push_back(std::move(f));
  }

  const int REP = 200;
  std::vector<double> tb, ta;
  double before_total = 0.0, after_total = 0.0;
  volatile size_t sink = 0;

  for (int r = 0; r < REP; ++r)
  {
    {
      std::vector<u8> buf; buf.reserve(1518);
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto& f : frames) { v_before::copy_to_buffer(buf, f.data(), (u32)f.size()); sink += buf.size(); }
      auto t1 = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double, std::nano>(t1 - t0).count();
      tb.push_back(elapsed); before_total += elapsed;
    }
    {
      std::vector<u8> buf; buf.reserve(1518);
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto& f : frames) { v_after::copy_to_buffer(buf, f.data(), (u32)f.size()); sink += buf.size(); }
      auto t1 = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double, std::nano>(t1 - t0).count();
      ta.push_back(elapsed); after_total += elapsed;
    }
  }

  std::sort(tb.begin(), tb.end());
  std::sort(ta.begin(), ta.end());
  double mb = tb[tb.size()/2];
  double ma = ta[ta.size()/2];
  printf("MEDIAN_SAMPLE_B_NS=%.0f MEDIAN_SAMPLE_A_NS=%.0f sink=%zu\n", mb, ma, (size_t)sink);
  printf("BEFORE_NS=%.0f\nAFTER_NS=%.0f\nSPEEDUP=%.4f\n", before_total, after_total, before_total/after_total);
  return 0;
}
