#include "io_case.hpp"

#include <ostream>

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// BEFORE: std::endl flushes each small record to the output stream.
PERFBANK_NOINLINE void write_scaffolds_before(
    const ScaffoldSet& scaffolds,
    std::ostream& lengthFile,
    std::ostream& componentFile) {
  for (std::size_t scaffoldIndex = 0; scaffoldIndex < scaffolds.size(); ++scaffoldIndex) {
    int totalLength = 0;
    for (const ScaffoldComponent& component : scaffolds[scaffoldIndex]) {
      componentFile << "scaffold-" << scaffoldIndex
                    << "\tcontig-" << component.contig
                    << "\t" << component.strand
                    << "\t" << component.length << std::endl;
      totalLength += component.length;
      if (component.gapAfter > 0) {
        componentFile << "scaffold-" << scaffoldIndex
                      << "\tgap\t-\t" << component.gapAfter << std::endl;
        totalLength += component.gapAfter;
      }
    }
    lengthFile << "scaffold-" << scaffoldIndex << "\t" << totalLength << std::endl;
    componentFile << std::endl;
  }
}
