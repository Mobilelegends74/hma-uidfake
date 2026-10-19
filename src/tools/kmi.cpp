// SPDX-License-Identifier: GPL-2.0
#include "kmi.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <elf.h>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <zlib.h>

namespace uidfake::kmi {
namespace {
constexpr std::size_t limit = 32 * 1024 * 1024;
std::vector<unsigned char> bytes(const std::filesystem::path &path,
                                 std::size_t maximum = limit) {
  std::ifstream in(path, std::ios::binary);
  std::vector<unsigned char> out;
  char block[4096];
  while (in.read(block, sizeof(block)) || in.gcount()) {
    if (out.size() + in.gcount() > maximum)
      throw std::runtime_error("file exceeds inspection limit");
    out.insert(out.end(), block, block + in.gcount());
  }
  return out;
}
std::string text(const std::filesystem::path &path) {
  auto b = bytes(path, 2 * 1024 * 1024);
  return {b.begin(), b.end()};
}
template <typename T>
T at(const std::vector<unsigned char> &b, std::size_t off) {
  if (off > b.size() || sizeof(T) > b.size() - off)
    throw std::runtime_error("truncated ELF structure");
  T value;
  std::memcpy(&value, b.data() + off, sizeof(value));
  return value;
}
std::string zstring(const std::vector<unsigned char> &b, std::size_t off) {
  if (off >= b.size())
    throw std::runtime_error("ELF string offset outside section");
  auto end = std::find(b.begin() + off, b.end(), 0);
  if (end == b.end())
    throw std::runtime_error("unterminated ELF string");
  return {b.begin() + off, end};
}
std::set<std::string> identities(const std::string &value,
                                 const std::string &version) {
  const std::regex pattern(
      "(^|[^a-zA-Z0-9])android([0-9]+)(-[0-9]+[.][0-9]+)?(?=[^a-zA-Z0-9.]|$)");
  std::set<std::string> out;
  for (auto i = std::sregex_iterator(value.begin(), value.end(), pattern);
       i != std::sregex_iterator(); ++i) {
    if ((*i)[3].matched && (*i)[3].str() != "-" + version)
      throw std::runtime_error(
          "conflicting Android KMI/Linux version evidence");
    out.insert("android" + (*i)[2].str() + "-" + version);
  }
  return out;
}
std::string config(const std::filesystem::path &root) {
  gzFile in = gzopen((root / "proc/config.gz").c_str(), "rb");
  if (!in)
    return {};
  std::string out;
  char block[8192];
  int count;
  while ((count = gzread(in, block, sizeof(block))) > 0) {
    if (out.size() + count > 2 * 1024 * 1024) {
      gzclose(in);
      throw std::runtime_error("kernel config exceeds inspection limit");
    }
    out.append(block, count);
  }
  gzclose(in);
  if (count < 0)
    throw std::runtime_error("cannot decode /proc/config.gz");
  return out;
}
} // namespace

Module inspect(const std::filesystem::path &path) {
  const auto b = bytes(path);
  const auto h = at<Elf64_Ehdr>(b, 0);
  if (std::memcmp(h.e_ident, ELFMAG, SELFMAG) ||
      h.e_ident[EI_CLASS] != ELFCLASS64 || h.e_ident[EI_DATA] != ELFDATA2LSB ||
      h.e_ident[EI_VERSION] != EV_CURRENT || h.e_version != EV_CURRENT ||
      h.e_machine != EM_AARCH64 || h.e_type != ET_REL ||
      h.e_ehsize != sizeof(h) || h.e_shentsize != sizeof(Elf64_Shdr) ||
      !h.e_shnum || h.e_shnum > 4096 || h.e_shstrndx >= h.e_shnum ||
      h.e_shoff > b.size() ||
      h.e_shnum > (b.size() - h.e_shoff) / sizeof(Elf64_Shdr))
    throw std::runtime_error("not a valid ARM64 relocatable kernel module");
  std::map<std::string, std::vector<unsigned char>> sections;
  auto slice = [&](const Elf64_Shdr &s) {
    if (s.sh_offset > b.size() || s.sh_size > b.size() - s.sh_offset)
      throw std::runtime_error("ELF section outside file");
    return std::vector<unsigned char>(b.begin() + s.sh_offset,
                                      b.begin() + s.sh_offset + s.sh_size);
  };
  const auto names =
      slice(at<Elf64_Shdr>(b, h.e_shoff + h.e_shstrndx * sizeof(Elf64_Shdr)));
  std::set<std::string> imports, versioned;
  bool symtab = false;
  for (unsigned i = 1; i < h.e_shnum; ++i) {
    auto s = at<Elf64_Shdr>(b, h.e_shoff + i * sizeof(Elf64_Shdr));
    if (s.sh_type == SHT_NOBITS)
      continue;
    const auto name = zstring(names, s.sh_name);
    if (s.sh_type == SHT_SYMTAB) {
      if (symtab || s.sh_link >= h.e_shnum ||
          s.sh_entsize != sizeof(Elf64_Sym) || s.sh_size % sizeof(Elf64_Sym))
        throw std::runtime_error("invalid module symbol table");
      symtab = true;
      const auto symbols = slice(s);
      const auto strings =
          slice(at<Elf64_Shdr>(b, h.e_shoff + s.sh_link * sizeof(Elf64_Shdr)));
      for (std::size_t n = sizeof(Elf64_Sym); n < symbols.size();
           n += sizeof(Elf64_Sym)) {
        const auto symbol = at<Elf64_Sym>(symbols, n);
        if (symbol.st_shndx == SHN_UNDEF &&
            ELF64_ST_BIND(symbol.st_info) == STB_GLOBAL)
          imports.insert(zstring(strings, symbol.st_name));
      }
    }
    if (name == ".modinfo" || name == ".note.gnu.build-id" ||
        name == "__versions" || name == "__version_ext_names" ||
        name == "__version_ext_crcs") {
      if (!sections.emplace(name, slice(s)).second)
        throw std::runtime_error("duplicate ELF metadata section");
    }
  }
  Module result;
  const auto &info = sections[".modinfo"];
  for (std::size_t i = 0; i < info.size();) {
    const auto item = zstring(info, i);
    i += item.size() + 1;
    const auto equal = item.find('=');
    if (equal != std::string::npos) {
      auto key = item.substr(0, equal), value = item.substr(equal + 1);
      if (key == "name" || key == "vermagic" || key == "srcversion" ||
          key == "uidfake_kmi") {
        if (!result.info.emplace(key, value).second)
          throw std::runtime_error("duplicate module identity field");
      }
    }
  }
  result.build_id = sections[".note.gnu.build-id"];
  // A real version table, including module_layout, is mandatory. No version
  // bytes or undefined symbols are removed/rewritten by this tool.
  auto &versions = sections["__versions"];
  auto record = [&](const std::string &name, uint64_t crc) {
    if (name.empty() || !crc || !versioned.insert(name).second)
      throw std::runtime_error("invalid/duplicate CRC version record");
  };
  if (!versions.empty()) {
    if (versions.size() % 64)
      throw std::runtime_error("malformed module symbol versions");
    for (std::size_t i = 0; i < versions.size(); i += 64) {
      const auto name = zstring(versions, i + 8);
      if (name.size() >= 56)
        throw std::runtime_error("oversized versioned symbol name");
      record(name, at<uint64_t>(versions, i));
    }
  } else {
    auto &ext = sections["__version_ext_names"];
    auto &crcs = sections["__version_ext_crcs"];
    std::size_t n = 0;
    for (std::size_t i = 0; i < ext.size(); ++n) {
      auto name = zstring(ext, i);
      i += name.size() + 1;
      record(name, at<uint32_t>(crcs, n * 4));
    }
    if (crcs.size() != n * 4)
      throw std::runtime_error("malformed extended symbol versions");
  }
  result.versioned = symtab && versioned.count("module_layout");
  for (const auto &name : imports)
    result.versioned = result.versioned && versioned.count(name);
  return result;
}

Result select(const std::filesystem::path &directory,
              const std::filesystem::path &root, const std::string &release,
              const std::string &machine) {
  if (machine != "aarch64")
    return {{}, "ARCH", "Only an ARM64 running kernel is supported", {}};
  std::smatch match;
  if (!std::regex_search(release, match, std::regex("^([0-9]+[.][0-9]+)[.]")))
    return {{}, "RELEASE", "Cannot parse running kernel release", {}};
  const std::string version = match[1];
  std::set<std::string> found;
  std::string evidence;
  auto merge = [&](const std::string &value, const std::string &source) {
    const auto ids = identities(value, version);
    if (!ids.empty()) {
      found.insert(ids.begin(), ids.end());
      evidence += source + ": " + value + "\n";
    }
  };
  merge(release, "uname");
  const auto cfg = config(root);
  if (cfg.find("# CONFIG_MODULES is not set") != std::string::npos ||
      cfg.find("# CONFIG_MODVERSIONS is not set") != std::string::npos)
    return {{},
            "MODVERSIONS",
            "Kernel disables modules or symbol CRC validation",
            {}};
  // Only explicit kernel metadata is evidence. Android SDK, fingerprint,
  // device names, source BRANCH and a lone Linux major.minor are not.
  std::istringstream lines(cfg);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.rfind("CONFIG_LOCALVERSION=", 0) == 0)
      merge(line.substr(20), "kernel CONFIG_LOCALVERSION");
  }
  // Only its release token is kernel identity. Compiler strings elsewhere in
  // /proc/version can mention a different Android toolchain branch.
  const auto banner = text(root / "proc/version");
  std::smatch banner_release;
  if (std::regex_search(banner, banner_release,
                        std::regex("^Linux version ([^ \\t\\n]+)")))
    merge(banner_release[1], "kernel /proc/version release");
  if (found.empty()) {
    // Trust on-disk vendor module provenance only when its build-id note
    // byte-matches a currently loaded kernel module, not just a filename or
    // a stale vendor partition. No module is loaded during this discovery.
    unsigned inspected = 0;
    std::uintmax_t budget = 256 * 1024 * 1024;
    for (const char *dir :
         {"vendor_dlkm/lib/modules", "system_dlkm/lib/modules",
          "vendor/lib/modules", "odm/lib/modules", "system/lib/modules"}) {
      std::error_code ec;
      std::filesystem::recursive_directory_iterator it(
          root / dir,
          std::filesystem::directory_options::skip_permission_denied, ec),
          end;
      for (; !ec && it != end; it.increment(ec)) {
        if (++inspected > 4096)
          return {{},
                  "SCAN_LIMIT",
                  "System module discovery limit exceeded; refusing incomplete "
                  "identity scan",
                  evidence};
        if (it.depth() > 8) {
          it.disable_recursion_pending();
          continue;
        }
        if (it->path().extension() != ".ko" || !it->is_regular_file(ec))
          continue;
        auto stem = it->path().stem().string();
        std::replace(stem.begin(), stem.end(), '-', '_');
        if (!std::filesystem::is_directory(root / "sys/module" / stem, ec))
          continue;
        const auto size = it->file_size(ec);
        if (ec || size > budget)
          return {{},
                  "SCAN_LIMIT",
                  "Cannot complete bounded system module inspection",
                  evidence};
        budget -= size;
        try {
          auto m = inspect(it->path());
          const auto name = m.info["name"];
          if (!std::regex_match(name, std::regex("[a-zA-Z0-9_]+")) ||
              m.build_id.size() < 16)
            continue;
          auto live = bytes(
              root / "sys/module" / name / "notes/.note.gnu.build-id", 4096);
          if (live != m.build_id)
            continue;
          const auto magic = m.info["vermagic"];
          if (magic.rfind(version + ".", 0) != 0)
            continue;
          merge(magic, "loaded build-id verified " + name);
        } catch (const std::exception &) {
          // Unreadable/invalid files do not establish a runtime identity.
        }
      }
    }
  }
  if (found.size() != 1)
    return {{},
            found.empty() ? "KMI_UNKNOWN" : "KMI_AMBIGUOUS",
            found.empty()
                ? "No verified Android KMI identity; refusing "
                  "Linux-version-only selection"
                : "Conflicting kernel KMI evidence; refusing cross-KMI load",
            evidence};
  const auto chosen = *found.begin();
  const auto path = directory / "ko" / (chosen + "_arm64_hma_uidfake.ko");
  if (!std::filesystem::is_regular_file(path))
    return {{}, "KMI_MISSING", "Package has no module for " + chosen, evidence};
  try {
    auto m = inspect(path);
    auto ids = identities(m.info["vermagic"], version);
    const auto embedded = m.info.find("uidfake_kmi");
    const bool identity_ok =
        embedded == m.info.end()
            ? ids == found
            : embedded->second == chosen && (ids.empty() || ids == found);
    if (m.info["name"] != "hma_uidfake" || !identity_ok || !m.versioned ||
        m.info["vermagic"].rfind(version + ".", 0) != 0 ||
        !std::regex_search(m.info["vermagic"],
                           std::regex("(^| )modversions( |$)")))
      return {{},
              "MODULE_METADATA",
              "Candidate identity or symbol-version metadata disagrees with "
              "kernel KMI",
              evidence};
  } catch (const std::exception &error) {
    return {{}, "MODULE_ELF", error.what(), evidence};
  }
  return {chosen, "KMI_SELECTED",
          "Identity selected; kernel must still validate unchanged LKM CRCs, "
          "vermagic and signature",
          evidence};
}
} // namespace uidfake::kmi
