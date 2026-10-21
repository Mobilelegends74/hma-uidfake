// SPDX-License-Identifier: GPL-2.0
#include "kmi.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
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
    result.crcs.emplace(name, crc);
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
    if (crcs.size() % sizeof(uint32_t))
      throw std::runtime_error("malformed extended CRC array");
    std::size_t i = 0;
    for (std::size_t n = 0; n < crcs.size() / sizeof(uint32_t); ++n) {
      auto name = zstring(ext, i);
      i += name.size() + 1;
      record(name, at<uint32_t>(crcs, n * 4));
    }
    // C string concatenation may leave one implicit final NUL in addition to
    // the per-symbol explicit terminator. Never count it as another symbol.
    if (ext.size() - i > 1 ||
        std::any_of(ext.begin() + i, ext.end(),
                    [](unsigned char c) { return c != 0; }))
      throw std::runtime_error("malformed extended symbol versions");
  }
  result.versioned = symtab && versioned.count("module_layout");
  for (const auto &name : imports)
    result.versioned = result.versioned && versioned.count(name);
  return result;
}

namespace {
// A loaded vendor module is not ABI proof on a kernel that ignores CRC errors.
bool crc_bypass_observed(const std::filesystem::path &root) {
  const int fd =
      open((root / "dev/kmsg").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    return false;
  std::string pending;
  bool bypass = false;
  char block[65536];
  for (unsigned i = 0; i < 256; ++i) {
    const auto size = read(fd, block, sizeof(block));
    if (size < 0 && (errno == EINTR || errno == EPIPE))
      continue;
    if (size <= 0)
      break;
    pending.append(block, size);
    if (pending.find("disagrees about version of symbol") !=
            std::string::npos &&
        pending.find("but ignore") != std::string::npos) {
      bypass = true;
      break;
    }
    if (pending.size() > 131072)
      pending.erase(0, pending.size() - 65536);
  }
  close(fd);
  return bypass;
}

std::optional<Result> image_profile(const std::filesystem::path &directory,
                                    const std::filesystem::path &root,
                                    const std::string &banner,
                                    const std::string &version) {
  std::error_code ec;
  auto current_banner = banner;
  while (!current_banner.empty() &&
         (current_banner.back() == '\n' || current_banner.back() == '\r'))
    current_banner.pop_back();
  unsigned count = 0;
  for (const auto &file :
       std::filesystem::directory_iterator(directory / "kernel-profiles", ec)) {
    if (++count > 64)
      throw std::runtime_error("too many kernel ABI profiles");
    if (file.path().extension() != ".abi")
      continue;
    std::istringstream input(text(file.path()));
    std::string line, id, kmi, expected_banner;
    std::map<std::string, uint64_t> crcs;
    while (std::getline(input, line)) {
      if (line.rfind("banner ", 0) == 0)
        expected_banner = line.substr(7);
      else if (line.rfind("build_id ", 0) == 0)
        id = line.substr(9);
      else if (line.rfind("kmi ", 0) == 0)
        kmi = line.substr(4);
      else if (line.rfind("crc ", 0) == 0) {
        std::istringstream record(line.substr(4));
        std::string name, hex;
        record >> name >> hex;
        if (!std::regex_match(name, std::regex("[a-zA-Z0-9_]+")) ||
            !std::regex_match(hex, std::regex("[0-9a-f]{8}")) ||
            !crcs.emplace(name, std::stoull(hex, nullptr, 16)).second)
          throw std::runtime_error("invalid kernel CRC profile");
      }
    }
    if (expected_banner != current_banner)
      continue;
    const std::string evidence =
        "kernel image profile: " + id +
        "; complete import comparison (not vendor CRCs)\n";
    if (!std::regex_match(id, std::regex("[0-9a-f]{40}")) ||
        !std::regex_match(kmi, std::regex("android[0-9]+-" + version)) ||
        !crcs.count("module_layout"))
      return Result{
          {}, "PROFILE_INVALID", "Invalid kernel-image ABI profile", evidence};
    std::vector<unsigned char> needle{4, 0, 0, 0, 20,  0,   0,   0,
                                      3, 0, 0, 0, 'G', 'N', 'U', 0};
    for (std::size_t i = 0; i < id.size(); i += 2)
      needle.push_back(std::stoul(id.substr(i, 2), nullptr, 16));
    const auto notes = bytes(root / "sys/kernel/notes", 65536);
    if (std::search(notes.begin(), notes.end(), needle.begin(), needle.end()) ==
        notes.end())
      return Result{
          {},
          "PROFILE_ID_UNVERIFIED",
          "Kernel banner matches a known CRC-bypass image, but live kernel "
          "build ID is unavailable/different; refusing vendor inference",
          evidence};
    const auto module =
        inspect(directory / "ko" / (kmi + "_arm64_hma_uidfake.ko"));
    if (!module.versioned || module.info.at("name") != "hma_uidfake" ||
        module.info.at("uidfake_kmi") != kmi ||
        module.info.at("vermagic").rfind(version + ".", 0) != 0 ||
        !std::regex_search(module.info.at("vermagic"),
                           std::regex("(^| )modversions( |$)")))
      return Result{{},
                    "MODULE_METADATA",
                    "Invalid candidate for image profile",
                    evidence};
    unsigned missing = 0, mismatch = 0;
    std::string details = evidence;
    for (const auto &[name, crc] : module.crcs) {
      const auto found = crcs.find(name);
      if (found == crcs.end()) {
        ++missing;
        details += "uncovered import=" + name + "\n";
      } else if (found->second != crc) {
        ++mismatch;
        details += "kernel CRC mismatch=" + name + "\n";
      }
    }
    details += "imports=" + std::to_string(module.crcs.size()) +
               " missing=" + std::to_string(missing) +
               " mismatched=" + std::to_string(mismatch) + "\n";
    if (missing || mismatch)
      return Result{{},
                    "ABI_KERNEL_MISMATCH",
                    "Packaged module does not match CRCs extracted from the "
                    "exact kernel image; a matching source-built LKM is "
                    "required (no force/CRC rewriting)",
                    details};
    return Result{kmi, "KMI_SELECTED",
                  "Live kernel build ID and all module import CRCs match "
                  "kernel-image profile; runtime hook testing still required",
                  details};
  }
  return std::nullopt;
}
} // namespace

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
  std::map<std::string, uint64_t> runtime_crcs;
  bool crc_conflict = false;
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
  if (const auto profile = image_profile(directory, root, banner, version))
    return *profile;
  if (crc_bypass_observed(root))
    return {{},
            "ABI_CHECKS_BYPASSED",
            "Kernel ignores module CRC mismatches; loaded vendor modules "
            "cannot establish kernel ABI. Need verified kernel-image profile "
            "and matching source-built LKM",
            {}};
  std::smatch banner_release;
  if (std::regex_search(banner, banner_release,
                        std::regex("^Linux version ([^ \t\n]+)")))
    merge(banner_release[1], "kernel /proc/version release");
  if (found.empty()) {
    // Trust on-disk vendor module provenance only when its build-id note
    // byte-matches a currently loaded kernel module, not just a filename or
    // a stale vendor partition. No module is loaded during this discovery.
    unsigned inspected = 0;
    unsigned verified = 0, crc_modules = 0, unreadable = 0;
    std::set<std::string> seen;
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
          if (!seen.insert(name).second)
            continue;
          ++verified;
          merge(magic, "loaded build-id verified " + name);
          // A force-loaded reference cannot attest normal kernel admission.
          // Failure to read taint is not evidence of an untainted module.
          std::ifstream taint_file(root / "sys/module" / name / "taint");
          if (!taint_file || !m.versioned ||
              cfg.find("CONFIG_MODVERSIONS=y\n") == std::string::npos)
            continue;
          std::string taint{std::istreambuf_iterator<char>(taint_file), {}};
          if (taint.find('F') != std::string::npos)
            continue;
          ++crc_modules;
          for (const auto &[symbol, crc] : m.crcs) {
            const auto entry = runtime_crcs.emplace(symbol, crc);
            if (!entry.second && entry.first->second != crc)
              crc_conflict = true;
          }
        } catch (const std::exception &) {
          // Unreadable/invalid files do not establish a runtime identity.
          ++unreadable;
        }
      }
    }
    evidence += "system scan: entries=" + std::to_string(inspected) +
                " build_id_verified=" + std::to_string(verified) +
                " crc_reference_modules=" + std::to_string(crc_modules) +
                " crc_symbols=" + std::to_string(runtime_crcs.size()) +
                " unreadable_or_invalid=" + std::to_string(unreadable) + "\n";
  }
  if (found.empty() && crc_conflict)
    return {{},
            "ABI_CONFLICT",
            "Verified loaded modules disagree on symbol CRCs; refusing "
            "inconsistent evidence",
            evidence};
  if (found.empty() && !runtime_crcs.empty()) {
    // This is candidate identification, not a claim of complete ABI coverage.
    // Require module_layout AND multiple other CRCs, zero observed mismatches,
    // and a unique candidate. All unseen imports are still checked by the
    // kernel's unchanged finit_module(flags=0), before module initialization.
    for (const auto &entry :
         std::filesystem::directory_iterator(directory / "ko")) {
      const auto file = entry.path().filename().string();
      const std::string suffix = "_arm64_hma_uidfake.ko";
      if (file.size() <= suffix.size() ||
          file.substr(file.size() - suffix.size()) != suffix)
        continue;
      const auto id = file.substr(0, file.size() - suffix.size());
      if (!std::regex_match(id, std::regex("android[0-9]+-[0-9]+[.][0-9]+")) ||
          id.substr(id.find('-') + 1) != version)
        continue;
      Module candidate;
      try {
        candidate = inspect(entry.path());
        const auto ids = identities(candidate.info["vermagic"], version);
        if (candidate.info["name"] != "hma_uidfake" ||
            candidate.info["uidfake_kmi"] != id || !candidate.versioned ||
            candidate.info["vermagic"].rfind(version + ".", 0) != 0 ||
            (!ids.empty() && ids != std::set<std::string>{id}) ||
            !std::regex_search(candidate.info["vermagic"],
                               std::regex("(^| )modversions( |$)")))
          return {{},
                  "MODULE_METADATA",
                  "Invalid packaged ABI candidate " + id,
                  evidence};
      } catch (const std::exception &error) {
        return {{}, "MODULE_ELF", id + ": " + error.what(), evidence};
      }
      unsigned common = 0, mismatch = 0;
      std::string differences;
      for (const auto &[symbol, crc] : candidate.crcs) {
        const auto ref = runtime_crcs.find(symbol);
        if (ref == runtime_crcs.end())
          continue;
        ++common;
        if (ref->second != crc) {
          ++mismatch;
          if (mismatch <= 8)
            differences += symbol + " ";
        }
      }
      const auto layout = runtime_crcs.find("module_layout");
      const bool layout_match =
          layout != runtime_crcs.end() &&
          candidate.crcs.at("module_layout") == layout->second;
      evidence +=
          "ABI candidate=" + id + " common=" + std::to_string(common) +
          " mismatched=" + std::to_string(mismatch) +
          " uncovered=" + std::to_string(candidate.crcs.size() - common) +
          " module_layout=" + (layout_match ? "match" : "missing/mismatch") +
          (differences.empty() ? "" : " differing=" + differences) + "\n";
      if (layout_match && common >= 8 && !mismatch)
        found.insert(id);
    }
    if (found.empty())
      return {{},
              "ABI_UNCONFIRMED",
              "No unique supported candidate established by verified "
              "loaded-module CRCs",
              evidence};
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
