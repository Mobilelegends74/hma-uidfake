// SPDX-License-Identifier: GPL-2.0
#include "status.hpp"

#include <algorithm>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <zlib.h>

#include "common.hpp"

namespace uidfake {
namespace {

/* Where this binary was started from: the module directory, the way the boot
 * scripts invoke it. */
std::filesystem::path module_dir() {
  std::error_code ec;
  const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);

  if (ec || self.empty())
    return {};
  return self.parent_path();
}

/* KernelSU module list gets a plain, localized status, not a diagnostic log.
 * The installer's locale.js holds "ru" or "en" and is safe to read as text.
 * No user-created or HMA policy files are inspected for this UI choice. */
std::string card_description(const std::filesystem::path &dir, bool working) {
  std::ifstream language{dir / "webroot" / "locale.js"};
  const std::string data{std::istreambuf_iterator<char>{language},
                         std::istreambuf_iterator<char>{}};
  const bool russian =
      data.find("HMA_UIDFAKE_INSTALL_LOCALE = \"ru\"") != std::string::npos;
  std::string reason;
  if (!working) {
    std::ifstream diagnostics{dir / "state" / "load.reason"};
    std::getline(diagnostics, reason);
    reason.resize(std::min(reason.size(), std::size_t{220}));
    if (reason.empty())
      reason = russian ? "Хуки или политика не активны; см. state/sync.log"
                       : "Hooks or policy inactive; see state/sync.log";
    reason = " — " + reason;
  }
  if (russian)
    return working ? "✅ Работает" : "❌ Не работает" + reason;
  return working ? "✅ Working" : "❌ Not working" + reason;
}

/* One config line's value, or empty. */
std::optional<unsigned int> config_value(const std::string &text,
                                         std::string_view key) {
  const std::string prefix = std::string(key) + "=";

  for (std::size_t at = text.find(prefix); at != std::string::npos;
       at = text.find(prefix, at + 1)) {
    if (at != 0 && text[at - 1] != '\n')
      continue;
    const std::size_t begin = at + prefix.size();
    const std::size_t end = text.find('\n', begin);
    const std::string value = text.substr(begin, end - begin);
    if (!value.empty() &&
        value.find_first_not_of("0123456789") == std::string::npos)
      return static_cast<unsigned int>(std::stoul(value));
  }
  return std::nullopt;
}

} // namespace

DeviceGeometry read_device_config() {
  /* The config does not change while the kernel runs: read it once. */
  static const DeviceGeometry cached = [] {
    DeviceGeometry out;
    gzFile file = gzopen("/proc/config.gz", "rb");
    if (file == nullptr)
      return out;

    std::string text;
    char buffer[16384];
    int got;
    while ((got = gzread(file, buffer,
                         static_cast<unsigned int>(sizeof(buffer)))) > 0)
      text.append(buffer, static_cast<std::size_t>(got));
    gzclose(file);
    if (text.empty())
      return out;

    out.va_bits = config_value(text, "CONFIG_ARM64_VA_BITS");
    if (text.find("CONFIG_ARM64_4K_PAGES=y") != std::string::npos)
      out.page_shift = 12;
    else if (text.find("CONFIG_ARM64_16K_PAGES=y") != std::string::npos)
      out.page_shift = 14;
    else if (text.find("CONFIG_ARM64_64K_PAGES=y") != std::string::npos)
      out.page_shift = 16;
    return out;
  }();
  return cached;
}

void report_status(std::string_view text) {
  const auto dir = module_dir();
  if (dir.empty())
    return;

  const auto prop = dir / "module.prop";
  std::ifstream in(prop);
  if (!in)
    return;

  const std::string wanted = card_description(dir, text == "working");
  std::vector<std::string> lines;
  std::string line;
  bool touched = false;

  while (std::getline(in, line)) {
    if (line.rfind("description=", 0) == 0) {
      const std::string replacement = "description=" + wanted;
      if (line != replacement) {
        line = replacement;
        touched = true;
      }
    }
    lines.push_back(line);
  }
  in.close();
  if (!touched)
    return; /* do not rewrite module.prop when the status did not change */

  const auto tmp = dir / "module.prop.tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out)
      return;
    for (const auto &l : lines)
      out << l << "\n";
  }
  std::error_code ec;
  std::filesystem::rename(tmp, prop, ec);
  if (ec)
    Log::warn("could not update {}: {}", prop.string(), ec.message());
}

} // namespace uidfake
