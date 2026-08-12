#include "mesh.hpp"

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// AFTER: exact final sizes are known, so each vector allocates once.
PERFBANK_NOINLINE TubeMesh make_tube_after(std::size_t faces) {
  TubeMesh result;
  result.vertices.reserve(6 * faces);
  result.normals.reserve(6 * faces);
  result.triangles.reserve(12 * faces);

  for (std::size_t index = 0; index < 6 * faces; ++index) {
    result.vertices.push_back(index * 17 + 3);
    result.normals.push_back(index * 31 + 7);
  }
  for (std::size_t index = 0; index < 12 * faces; ++index) {
    result.triangles.push_back(index % (6 * faces));
  }
  return result;
}
