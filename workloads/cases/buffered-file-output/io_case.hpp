#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

struct ScaffoldComponent {
  std::uint64_t contig;
  char strand;
  int length;
  int gapAfter;
};

using Scaffold = std::vector<ScaffoldComponent>;
using ScaffoldSet = std::vector<Scaffold>;

void write_scaffolds_before(
    const ScaffoldSet& scaffolds,
    std::ostream& lengthFile,
    std::ostream& componentFile);

void write_scaffolds_after(
    const ScaffoldSet& scaffolds,
    std::ostream& lengthFile,
    std::ostream& componentFile);
