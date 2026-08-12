#include "directory.hpp"

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// AFTER: let the existing O(1)-expected hash insertion reject duplicates.
PERFBANK_NOINLINE bool add_file_after(
    PakDirectory& directory,
    const std::string& name) {
  directory.files.emplace_front(name);
  PakFile* file = &directory.files.front();
  const bool inserted = directory.filesMap.emplace(lowercase(name), file).second;
  if (!inserted) {
    directory.files.pop_front();
  }
  return inserted;
}
