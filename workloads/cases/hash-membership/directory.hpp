#pragma once

#include <list>
#include <string>
#include <unordered_map>
#include <utility>

struct PakFile {
  explicit PakFile(std::string value) : name(std::move(value)) {}
  std::string name;
};

struct PakDirectory {
  std::list<PakFile> files;
  std::unordered_map<std::string, PakFile*> filesMap;
};

std::string lowercase(std::string value);
bool same_name(const std::string& left, const std::string& right);
bool add_file_before(PakDirectory& directory, const std::string& name);
bool add_file_after(PakDirectory& directory, const std::string& name);
