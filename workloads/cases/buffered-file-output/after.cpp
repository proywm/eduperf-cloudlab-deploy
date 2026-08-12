#include "io_case.hpp"

#include <ostream>
#include <sstream>

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

namespace {
constexpr std::size_t kBufferSize = 4096;

void flush_if_full(std::ostringstream& buffer, std::ostream& file, bool finalFlush) {
  if (finalFlush || buffer.tellp() >= static_cast<std::streamoff>(kBufferSize)) {
    file << buffer.str();
    buffer.str("");
    buffer.clear();
  }
}
}  // namespace

// AFTER: records accumulate in memory and cross the stream boundary in blocks.
PERFBANK_NOINLINE void write_scaffolds_after(
    const ScaffoldSet& scaffolds,
    std::ostream& lengthFile,
    std::ostream& componentFile) {
  std::ostringstream lengths;
  std::ostringstream components;
  for (std::size_t scaffoldIndex = 0; scaffoldIndex < scaffolds.size(); ++scaffoldIndex) {
    int totalLength = 0;
    for (const ScaffoldComponent& component : scaffolds[scaffoldIndex]) {
      components << "scaffold-" << scaffoldIndex
                 << "\tcontig-" << component.contig
                 << "\t" << component.strand
                 << "\t" << component.length << '\n';
      totalLength += component.length;
      if (component.gapAfter > 0) {
        components << "scaffold-" << scaffoldIndex
                   << "\tgap\t-\t" << component.gapAfter << '\n';
        totalLength += component.gapAfter;
      }
    }
    lengths << "scaffold-" << scaffoldIndex << "\t" << totalLength << '\n';
    components << '\n';
    flush_if_full(components, componentFile, false);
    flush_if_full(lengths, lengthFile, false);
  }
  flush_if_full(components, componentFile, true);
  flush_if_full(lengths, lengthFile, true);
}
