#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct TubeMesh {
  std::vector<std::uint64_t> vertices;
  std::vector<std::uint64_t> normals;
  std::vector<std::uint64_t> triangles;
};

TubeMesh make_tube_before(std::size_t faces);
TubeMesh make_tube_after(std::size_t faces);
