// SPDX-License-Identifier: GPL-2.0
#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace uidfake::kmi {
struct Module {
  std::map<std::string, std::string> info;
  std::vector<unsigned char> build_id;
  bool versioned = false;
};
struct Result {
  std::string selected, code, reason, evidence;
};
Module inspect(const std::filesystem::path &path);
Result select(const std::filesystem::path &directory,
              const std::filesystem::path &root, const std::string &release,
              const std::string &machine);
} // namespace uidfake::kmi
