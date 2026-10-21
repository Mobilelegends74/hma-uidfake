// SPDX-License-Identifier: GPL-2.0
#include "paths.hpp"

#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <system_error>
#include <unistd.h>

namespace uidfake {

RuleMode read_rule_mode(const std::filesystem::path &file) {
  std::error_code ec;
  if (!std::filesystem::exists(file, ec))
    return ec ? RuleMode::Invalid : RuleMode::External;
  std::ifstream in(file);
  std::string value, extra;
  if (!(in >> value) || (in >> extra))
    return RuleMode::Invalid;
  if (value == "external")
    return RuleMode::External;
  if (value == "manual")
    return RuleMode::Manual;
  return RuleMode::Invalid;
}

RuleMode rule_mode() {
  return read_rule_mode(std::filesystem::path{kNativeConfigDir} /
                        "source-mode");
}

std::string_view rule_mode_name(RuleMode mode) {
  if (mode == RuleMode::External)
    return "external";
  if (mode == RuleMode::Manual)
    return "manual";
  return "invalid";
}

bool write_rule_mode(RuleMode mode) {
  if (mode == RuleMode::Invalid)
    return false;
  const auto dir = std::filesystem::path{kNativeConfigDir};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec)
    return false;
  std::string tmp = (dir / ".source-mode-XXXXXX").string();
  const int fd = ::mkstemp(tmp.data());
  if (fd < 0)
    return false;
  const std::string value = std::string{rule_mode_name(mode)} + "\n";
  const bool written = ::write(fd, value.data(), value.size()) ==
                           static_cast<ssize_t>(value.size()) &&
                       ::fsync(fd) == 0;
  ::close(fd);
  if (written)
    std::filesystem::rename(tmp, dir / "source-mode", ec);
  if (!written || ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  return true;
}

namespace {

/* /data/user/0/<pkg> is a bind mount of /data/data/<pkg>; a plain adb shell or
 * an old ROM may only have the second one. */
constexpr std::string_view kDataUserPrefix = "/data/user/0/";
constexpr std::string_view kDataDataPrefix = "/data/data/";

[[nodiscard]] bool has_glob(const std::filesystem::path &path) {
  return path.string().find('*') != std::string::npos;
}

/* The directories a pattern names, sorted so the result is stable. */
[[nodiscard]] std::vector<std::filesystem::path>
match_dirs(const std::filesystem::path &pattern) {
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  for (const auto &entry :
       std::filesystem::directory_iterator{pattern.parent_path(), ec}) {
    if (ec)
      break;
    if (leaf_matches(entry.path().filename().string(),
                     pattern.filename().string()))
      found.push_back(entry.path());
  }
  std::ranges::sort(found);
  return found;
}

[[nodiscard]] std::filesystem::path
resolve_user_prefix(const std::filesystem::path &path) {
  std::error_code ignored;
  if (std::filesystem::exists(path, ignored))
    return path;
  auto text = path.string();
  if (text.starts_with(kDataUserPrefix)) {
    text.replace(0, kDataUserPrefix.size(), kDataDataPrefix);
    const std::filesystem::path alternative{text};
    if (std::filesystem::exists(alternative, ignored))
      return alternative;
  }
  return path;
}

} // namespace

std::string_view tool_name(Tool tool) {
  switch (tool) {
  case Tool::Native:
    return "uidfake";
  case Tool::HmaOss:
    return "hma-oss";
  case Tool::Hma:
    break;
  }
  return "hma";
}

std::vector<RuleSource> RuleSource::known() {
  // Saved manual data is separate from external policy. Never resurrect a
  // deleted primary config through the obsolete module-directory copy.
  return {
      {native_config_file(), Tool::Native},
      {std::filesystem::path{
           "/data/user/0/com.tsng.hidemyapplist/files/config.json"},
       Tool::Hma},
      {std::filesystem::path{"/data/misc/hide_my_applist_*/config.json"},
       Tool::HmaOss},
      {std::filesystem::path{"/data/system/hide_my_applist_*/config.json"},
       Tool::HmaOss},
  };
}

std::optional<RuleSource>
RuleSource::active(const std::vector<RuleSource> &sources, RuleMode mode) {
  if (mode == RuleMode::Invalid)
    return std::nullopt;
  // No file-existence priority: saved/empty manual rules cannot override HMA.
  // External mode prefers OSS, with legacy HMA as a compatibility fallback.
  for (const auto tool : mode == RuleMode::Manual
                             ? std::vector<Tool>{Tool::Native}
                             : std::vector<Tool>{Tool::HmaOss, Tool::Hma})
    for (const auto &source : sources)
      if (source.tool() == tool && source.config())
        return source;
  return std::nullopt;
}

std::optional<std::filesystem::path> RuleSource::config() const {
  std::error_code ignored;
  const auto dir = pattern_.parent_path();
  if (has_glob(dir.filename().string())) {
    for (const auto &match : match_dirs(dir)) {
      auto file = match / pattern_.filename();
      if (std::filesystem::exists(file, ignored))
        return file;
    }
    return std::nullopt;
  }
  auto file = resolve_user_prefix(pattern_);
  if (!std::filesystem::exists(file, ignored))
    return std::nullopt;
  return file;
}

bool leaf_matches(std::string_view name, std::string_view pattern) {
  const auto star = pattern.find('*');
  if (star == std::string_view::npos)
    return name == pattern;
  const auto prefix = pattern.substr(0, star);
  const auto suffix = pattern.substr(star + 1);
  return name.size() >= prefix.size() + suffix.size() &&
         name.compare(0, prefix.size(), prefix) == 0 &&
         name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<RuleSource::Watch> RuleSource::watches() const {
  std::vector<Watch> watches;
  std::error_code ignored;
  const auto dir = pattern_.parent_path();

  /*
   * The data the app keeps beside its config. The presets a caller applies are
   * expanded into that cache, and the cache is written after the config that
   * names them, so a change to it has to wake the sync up on its own.
   */
  const std::vector<std::string> names{
      pattern_.filename().string(), std::string{kPresetCacheNew},
      std::string{kPresetCacheOld}, "source-mode"};

  const auto watch_dir = [&](const std::filesystem::path &watched) {
    watches.push_back(
        Watch{.path = watched, .kind = Watch::Kind::Directory, .names = names});
    for (const auto &name : names) {
      const auto file = watched / name;
      if (std::filesystem::exists(file, ignored))
        watches.push_back(
            Watch{.path = file, .kind = Watch::Kind::File, .names = {}});
    }
  };

  if (!has_glob(dir.filename().string())) {
    watch_dir(resolve_user_prefix(pattern_).parent_path());
    return watches;
  }

  const auto matches = match_dirs(dir);

  watches.push_back(Watch{.path = dir.parent_path(),
                          .kind = Watch::Kind::Directory,
                          .names = {dir.filename().string()}});

  for (const auto &match : matches)
    watch_dir(match);
  return watches;
}

} // namespace uidfake
