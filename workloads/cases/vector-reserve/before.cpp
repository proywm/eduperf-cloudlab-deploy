#include "mesh.hpp"

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// BEFORE: each vector begins with zero capacity and grows geometrically.
PERFBANK_NOINLINE TubeMesh make_tube_before(std::size_t faces) {
  TubeMesh result;

  for (std::size_t index = 0; index < 6 * faces; ++index) {
    result.vertices.push_back(index * 17 + 3);
    result.normals.push_back(index * 31 + 7);
  }
  for (std::size_t index = 0; index < 12 * faces; ++index) {
    result.triangles.push_back(index % (6 * faces));
  }
  return result;
}
