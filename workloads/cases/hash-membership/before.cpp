#include "directory.hpp"

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// BEFORE: scan the linked list even though the hash-map insertion below
// performs the same duplicate check.
PERFBANK_NOINLINE bool add_file_before(
    PakDirectory& directory,
    const std::string& name) {
  for (const PakFile& file : directory.files) {
    if (same_name(file.name, name)) {
      return false;
    }
  }

  directory.files.emplace_front(name);
  PakFile* file = &directory.files.front();
  const bool inserted = directory.filesMap.emplace(lowercase(name), file).second;
  if (!inserted) {
    directory.files.pop_front();
  }
  return inserted;
}
