#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

extern std::atomic<std::uint64_t> location_copies;
extern std::atomic<std::uint64_t> state_copies;

struct ProgramPoint {
  std::uint64_t first;
  std::uint64_t second;
  std::uint64_t third;

  ProgramPoint(std::uint64_t a, std::uint64_t b, std::uint64_t c)
      : first(a), second(b), third(c) {}
  ProgramPoint(const ProgramPoint& other)
      : first(other.first), second(other.second), third(other.third) {
    location_copies.fetch_add(1, std::memory_order_relaxed);
  }
};

struct ProgramStateRef {
  const void* pointer;

  explicit ProgramStateRef(const void* value) : pointer(value) {}
  ProgramStateRef(const ProgramStateRef& other) : pointer(other.pointer) {
    state_copies.fetch_add(1, std::memory_order_relaxed);
  }
};

struct ExplodedNode {
  ProgramPoint Location;
  ProgramStateRef State;
  bool Sink;

  ProgramPoint getLocation() const { return Location; }
  ProgramStateRef getState() const { return State; }
  bool isSink() const { return Sink; }
};

inline std::uint64_t build_profile(
    const ProgramPoint& location,
    const ProgramStateRef& state,
    bool sink) {
  std::uint64_t hash = location.first * 1315423911ULL;
  hash ^= location.second * 2654435761ULL;
  hash ^= location.third + reinterpret_cast<std::uintptr_t>(state.pointer);
  return hash ^ (sink ? 0xB1ULL : 0xB0ULL);
}

std::uint64_t profile_before(const ExplodedNode& node);
std::uint64_t profile_after(const ExplodedNode& node);
std::uint64_t profile_batch_before(
    const std::vector<ExplodedNode>& nodes,
    std::uint64_t calls);
std::uint64_t profile_batch_after(
    const std::vector<ExplodedNode>& nodes,
    std::uint64_t calls);
