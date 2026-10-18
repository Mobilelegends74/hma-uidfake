// SPDX-License-Identifier: GPL-2.0
#include "rules.hpp"

#include "oss_presets.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <ranges>
#include <utility>

namespace uidfake {
namespace {

/* HMA-OSS reads a preset as exactPackageNames U packageNames and exports only
 * the second half to its cache, so the written-in half is checked here as well:
 * the two halves together are what the app hides. */
[[nodiscard]] bool static_preset_contains(std::string_view preset,
                                          std::string_view target) {
  for (const auto &entry : oss_presets::kStatic) {
    if (entry.name != preset)
      continue;
    for (std::size_t i = 0; i < entry.count; ++i)
      if (entry.packages[i] == target)
        return true;
  }
  return false;
}

[[nodiscard]] std::size_t static_preset_size(std::string_view preset) {
  for (const auto &entry : oss_presets::kStatic)
    if (entry.name == preset)
      return entry.count;
  return 0;
}

/* A preset and how many packages it holds: the written-in names plus what the
 * cache carried, so the number can be read next to the app's own preset view. A
 * '?' marks a name the cache did not have. */
[[nodiscard]] std::string preset_label(std::string_view preset,
                                       const Presets &presets) {
  const auto dynamic = presets.find(std::string{preset});
  const std::size_t statics = static_preset_size(preset);
  const std::size_t total =
      (dynamic == presets.end() ? 0 : dynamic->second.size()) + statics;
  return dynamic == presets.end() ? std::format("{}?({})", preset, total)
                                  : std::format("{}({})", preset, total);
}

/* One entry per pair: the kernel table is a set, and so is the count. */
void dedupe(Pairs &pairs) {
  std::ranges::sort(pairs);
  pairs.erase(std::ranges::unique(pairs).begin(), pairs.end());
}

/* HMA's own list (xj.a in the app): the packages it always knows about. They
 * are not hidden from anybody, and nothing is hidden from them; when a caller
 * hides everything else they join the set, which is what keeps them visible
 * there. */
constexpr std::array kHmaKnownPackages{
    std::string_view{"android"},
    std::string_view{"com.android.shell"},
    std::string_view{"com.android.systemui"},
    std::string_view{"com.android.permissioncontroller"},
    std::string_view{"com.android.providers.downloads"},
    std::string_view{"com.android.providers.downloads.ui"},
    std::string_view{"com.android.providers.media"},
    std::string_view{"com.android.providers.media.module"},
    std::string_view{"com.android.providers.settings"},
    std::string_view{"com.google.android.webview"},
    std::string_view{"com.google.android.providers.media.module"},
};

/* Constants.packagesShouldNotHide in HMA-OSS. A different list from HMA's; a
 * caller or a target on it is never hidden. */
/* Names HMA-OSS knows by itself, and the callers for which a preset hit still
 * leaves a GMS-connected app visible (Constants.gmsPackages/riskyPackages). */
constexpr std::string_view kHmaOssManager = "org.frknkrc44.hma_oss";
constexpr std::string_view kHmaOssGms = "com.google.android.gms";
constexpr std::string_view kHmaOssVending = "com.android.vending";
constexpr std::array kHmaOssGmsCallers{
    std::string_view{"com.android.vending"},
    std::string_view{"com.google.android.gms"},
    std::string_view{"com.google.android.gsf"},
};

constexpr std::array kHmaOssReservedPackages{
    std::string_view{"android"},
    std::string_view{"android.media"},
    std::string_view{"android.uid.system"},
    std::string_view{"android.uid.shell"},
    std::string_view{"android.uid.systemui"},
    std::string_view{"com.android.permissioncontroller"},
    std::string_view{"com.android.providers.downloads"},
    std::string_view{"com.android.providers.downloads.ui"},
    std::string_view{"com.android.providers.media"},
    std::string_view{"com.android.providers.media.module"},
    std::string_view{"com.android.providers.settings"},
    std::string_view{"com.google.android.providers.media.module"},
    std::string_view{"com.google.android.permissioncontroller"},
    std::string_view{"com.miui.securitycenter"},
};

} // namespace

std::optional<nlohmann::json>
Rules::read_json(const std::filesystem::path &path) {
  try {
    std::ifstream in(path);
    if (!in)
      return std::nullopt;
    nlohmann::json config;
    in >> config;
    return config;
  } catch (const std::exception &e) {
    Log::warn("cannot parse {}: {}", path.string(), e.what());
    return std::nullopt;
  }
}

Rules::Rules(Tool tool, nlohmann::json config, std::filesystem::path path)
    : config_(std::move(config)), path_(std::move(path)), tool_(tool) {
  if (const auto it = config_.find("scope");
      it != config_.end() && it->is_object())
    scope_ = &*it;
  if (const auto it = config_.find("templates");
      it != config_.end() && it->is_object())
    templates_ = &*it;
}

const nlohmann::json *Rules::caller_entry(std::string_view caller) const {
  if (scope_ == nullptr)
    return nullptr;
  const auto it = scope_->find(std::string{caller});
  return it == scope_->end() ? nullptr : &*it;
}

const nlohmann::json *Rules::template_entry(std::string_view name) const {
  if (templates_ == nullptr)
    return nullptr;
  const auto it = templates_->find(std::string{name});
  return it == templates_->end() ? nullptr : &*it;
}

const nlohmann::json *Rules::find_array(const nlohmann::json &entry,
                                        std::string_view key) {
  const auto it = entry.find(key);
  if (it == entry.end() || !it->is_array())
    return nullptr;
  return &*it;
}

bool Rules::in_list(const nlohmann::json &array, std::string_view name) {
  return std::ranges::any_of(array, [&](const nlohmann::json &item) {
    return item.is_string() && item.get_ref<const std::string &>() == name;
  });
}

void Rules::log_summary(std::size_t callers, const Pairs &pairs) const {
  Log::info("{} ({}): {} caller(s), {} pair(s)", path_.string(),
            tool_name(tool_), callers, pairs.size());
}

std::unique_ptr<HmaRules> HmaRules::load(const std::filesystem::path &path) {
  auto config = read_json(path);
  if (!config)
    return nullptr;
  return std::make_unique<HmaRules>(std::move(*config), path);
}

/* HMA: the extra list plus the appList of every template the caller applies.
 * See defpackage/m10.java in the app. */
bool HmaRules::on_list(const nlohmann::json &entry,
                       std::string_view target) const {
  if (const auto *extra = find_array(entry, "extraAppList");
      extra != nullptr && in_list(*extra, target))
    return true;

  const auto *applied = find_array(entry, "applyTemplates");
  if (applied == nullptr)
    return false;
  for (const auto &name : *applied) {
    if (!name.is_string())
      continue;
    const auto *tpl = template_entry(name.get_ref<const std::string &>());
    if (tpl == nullptr)
      continue;
    const auto *list = find_array(*tpl, "appList");
    if (list != nullptr && in_list(*list, target))
      return true;
  }
  return false;
}

bool HmaRules::hides_target(const nlohmann::json &entry,
                            std::string_view target,
                            bool target_is_system) const {
  /* The caller's own list never hides a package HMA always knows about; in
   * whitelist mode those join the set instead, which keeps them visible. */
  const bool whitelist = entry.value("useWhitelist", false);
  const bool known = std::ranges::contains(kHmaKnownPackages, target);
  if (!whitelist && known)
    return false;
  /* System apps stay visible in whitelist mode when that is switched on. */
  if (whitelist && entry.value("excludeSystemApps", false) && target_is_system)
    return false;

  const bool on_the_list = on_list(entry, target) || (whitelist && known);
  return whitelist ? !on_the_list : on_the_list;
}

bool HmaRules::hides(std::string_view caller, std::string_view target,
                     bool target_is_system, const Presets &) const {
  if (std::ranges::contains(kHmaKnownPackages, caller))
    return false;
  const auto *entry = caller_entry(caller);
  if (entry == nullptr)
    return false;
  return hides_target(*entry, target, target_is_system);
}

Pairs HmaRules::expand(const PackageDb &packages, const Presets &) const {
  Pairs pairs;
  std::size_t callers = 0;

  if (scope_ != nullptr)
    for (const auto &[caller, entry] : scope_->items()) {
      append_pairs(
          pairs, packages, caller,
          [&](std::string_view target, std::uint32_t, bool target_is_system) {
            return hides_target(entry, target, target_is_system);
          });
      ++callers;
    }

  dedupe(pairs);
  log_summary(callers, pairs);
  return pairs;
}

HmaOssRules::HmaOssRules(nlohmann::json config, std::filesystem::path path)
    : Rules(Tool::HmaOss, std::move(config), std::move(path)) {
  const auto *ignored = find_array(config_, "ignoredPackagesForPresets");
  if (ignored == nullptr)
    return;
  for (const auto &item : *ignored)
    if (item.is_string())
      presets_skip_.insert(item.get_ref<const std::string &>());
}

std::unique_ptr<HmaOssRules>
HmaOssRules::load(const std::filesystem::path &path) {
  auto config = read_json(path);
  if (!config)
    return nullptr;
  return std::make_unique<HmaOssRules>(std::move(*config), path);
}

bool HmaOssRules::presets_skip(std::string_view target) const {
  return presets_skip_.contains(target);
}

/* HMA-OSS: HMAService.shouldHide, in its order. */
bool HmaOssRules::hides_target(std::string_view caller,
                               const nlohmann::json &entry,
                               std::string_view target, bool target_is_system,
                               const Presets &presets) const {
  const bool whitelist = entry.value("useWhitelist", false);
  if (const auto *extra = find_array(entry, "extraAppList");
      extra != nullptr && in_list(*extra, target))
    return !whitelist;
  if (const auto *opposite = find_array(entry, "extraOppositeAppList");
      opposite != nullptr && in_list(*opposite, target))
    return whitelist;

  if (const auto *applied = find_array(entry, "applyTemplates");
      applied != nullptr) {
    for (const auto &name : *applied) {
      if (!name.is_string())
        continue;
      const auto *tpl = template_entry(name.get_ref<const std::string &>());
      if (tpl == nullptr)
        continue;
      const auto *list = find_array(*tpl, "appList");
      if (list != nullptr && in_list(*list, target))
        return !whitelist;
    }
  }

  if (!presets_skip(target)) {
    if (const auto *applied = find_array(entry, "applyPresets");
        applied != nullptr) {
      for (const auto &name : *applied) {
        if (!name.is_string())
          continue;
        const auto &preset_name = name.get_ref<const std::string &>();
        const auto preset = presets.find(std::string{preset_name});
        /* HMA-OSS reads a preset as exactPackageNames U packageNames, and its
         * cache carries only the second half: the written-in half is checked
         * here too, so a cache that is missing or trimmed hides the same set
         * the app does. */
        const auto scanned = facts_.scanned.find(std::string{preset_name});
        const bool in_preset =
            (preset != presets.end() && preset->second.contains(target)) ||
            (scanned != facts_.scanned.end() &&
             scanned->second.contains(target)) ||
            static_preset_contains(preset_name, target);
        if (in_preset) {
          /* The Play Store asks as GMS itself. */
          const auto overridden =
              caller == kHmaOssVending ? std::string_view{kHmaOssGms} : caller;
          return !gms_ignored(overridden, target);
        }
      }
    }
  }

  if (whitelist && entry.value("excludeSystemApps", false) && target_is_system)
    return false;
  return whitelist;
}

/* HMA-OSS: HMAService.shouldHide, in its order, including the checks that come
 * before any list is looked at. */
bool HmaOssRules::hides(std::string_view caller, std::string_view target,
                        bool target_is_system, const Presets &presets) const {
  if (caller == kHmaOssManager)
    return false;
  if (std::ranges::contains(kHmaOssReservedPackages, caller) ||
      std::ranges::contains(kHmaOssReservedPackages, target))
    return false;
  /* A caller never hides itself. */
  if (caller == target)
    return false;
  if (config_.value("webViewProtection", false)) {
    if (facts_.webview == caller || facts_.webview == target)
      return false;
    if (!facts_.browser.empty() &&
        (facts_.browser == caller || facts_.browser == target))
      return false;
  }
  const auto *entry = caller_entry(caller);
  if (entry == nullptr)
    return false;
  return hides_target(caller, *entry, target, target_is_system, presets);
}

/* True when the caller asks as a GMS package and the target has a GMS
 * connection: HMA-OSS leaves those visible, presets included. */
bool HmaOssRules::gms_ignored(std::string_view caller,
                              std::string_view target) const {
  return std::ranges::contains(kHmaOssGmsCallers, caller) &&
         facts_.gms_connected.contains(target);
}

/* The names the config applies, each once, in a stable order. */
std::vector<std::string> HmaOssRules::presets_in_use() const {
  std::vector<std::string> names;

  if (scope_ == nullptr)
    return names;
  for (const auto &[caller, entry] : scope_->items()) {
    const auto *applied = find_array(entry, "applyPresets");

    if (applied == nullptr)
      continue;
    for (const auto &item : *applied) {
      if (!item.is_string())
        continue;
      auto name = item.get<std::string>();
      if (!std::ranges::contains(names, name))
        names.push_back(std::move(name));
    }
  }
  std::ranges::sort(names);
  return names;
}

/* The names the config applies, split by whether the cache had them. */
void HmaOssRules::report_presets(const PackageDb &packages,
                                 const Presets &presets,
                                 const Pairs &pairs) const {
  if (scope_ == nullptr)
    return;

  std::vector<std::string> found;
  std::vector<std::string> missing;
  for (const auto &[caller, entry] : scope_->items()) {
    const auto *applied = find_array(entry, "applyPresets");
    if (applied == nullptr)
      continue;
    for (const auto &item : *applied) {
      if (!item.is_string())
        continue;
      auto name = item.get<std::string>();
      if (std::ranges::contains(found, name) ||
          std::ranges::contains(missing, name))
        continue;
      (presets.contains(name) ? found : missing).push_back(std::move(name));
    }
  }
  if (found.empty() && missing.empty())
    return;

  /* The names on one line, for the log. */
  const auto name_list = [](const std::vector<std::string> &list) {
    std::string joined;
    for (const auto &name : list) {
      if (!joined.empty())
        joined += ", ";
      joined += name;
    }
    return joined;
  };

  /* How much they are worth: the same callers asked again without them. */
  Pairs plain;
  for (const auto &[caller, entry] : scope_->items())
    append_pairs(
        plain, packages, caller,
        [&](std::string_view target, std::uint32_t, bool target_is_system) {
          return hides(caller, target, target_is_system, {});
        });
  std::size_t extra = 0;
  for (const auto &pair : pairs) {
    const bool in_plain = std::ranges::any_of(plain, [&](const Pair &other) {
      return other.caller == pair.caller && other.target == pair.target;
    });
    if (!in_plain)
      ++extra;
  }

  if (!found.empty())
    Log::info("presets: {} ({} pair(s) come only from them)", name_list(found),
              extra);
  /*
   * A name the app's cache did not have is not a name that hides nothing: the
   * expansion also reads this module's own scan and its built-in list, so the
   * report says which of the two applies instead of writing them all off.
   */
  std::vector<std::string> unknown;
  for (const auto &name : missing) {
    const bool covered =
        facts_.scanned.contains(name) ||
        std::ranges::any_of(oss_presets::kStatic, [&](const auto &entry) {
          return entry.name == name;
        });
    if (!covered)
      unknown.push_back(name);
  }
  if (!missing.empty()) {
    if (unknown.empty())
      Log::info(
          "presets: {} (not in the app's cache; this module's own scan and "
          "its built-in list stand in for them)",
          name_list(missing));
    else
      Log::info(
          "! presets: {} (not in the app's cache and unknown to this module "
          "either, so they hide nothing)",
          name_list(unknown));
  }
}

/* One caller's entry, as this file read it: the mode, how many entries each
 * list has, the names of the templates and presets it applies (a name the cache
 * does not carry gets a '?'), and the system-app switch. */
std::string HmaOssRules::describe(const nlohmann::json &entry,
                                  const Presets &presets) const {
  const auto list_of = [&](std::string_view key) -> const nlohmann::json * {
    return find_array(entry, key);
  };
  const auto count_of = [&](std::string_view key) {
    const auto *list = list_of(key);
    return list == nullptr ? std::size_t{0} : list->size();
  };
  const auto names_of = [&](std::string_view key) {
    std::string joined;
    const auto *list = list_of(key);
    if (list == nullptr)
      return joined;
    for (const auto &item : *list) {
      if (!item.is_string())
        continue;
      if (!joined.empty())
        joined += ",";
      joined += item.get_ref<const std::string &>();
    }
    return joined;
  };

  const auto with_names = [&](std::string_view key) {
    const auto joined = names_of(key);
    return std::format("{} {}", count_of(key),
                       joined.empty() ? "[none]" : "[" + joined + "]");
  };

  std::string out = entry.value("useWhitelist", false) ? "whitelist" : "normal";
  out += std::format(", extra {}, opposite {}", with_names("extraAppList"),
                     with_names("extraOppositeAppList"));
  const auto templates = names_of("applyTemplates");
  if (!templates.empty())
    out += std::format(", templates [{}]", templates);
  const auto *applied = list_of("applyPresets");
  if (applied != nullptr) {
    std::string preset_names;
    for (const auto &item : *applied) {
      if (!item.is_string())
        continue;
      if (!preset_names.empty())
        preset_names += ",";
      /* The count is both halves of the preset, so it can be read next to the
       * app's own preset view; a '?' marks a name the cache did not have. */
      preset_names +=
          preset_label(item.get_ref<const std::string &>(), presets);
    }
    if (!preset_names.empty())
      out += std::format(", presets [{}]", preset_names);
  }
  if (entry.value("excludeSystemApps", false))
    out += ", excludeSystemApps";
  return out;
}

Pairs HmaOssRules::expand(const PackageDb &packages,
                          const Presets &presets) const {
  Pairs pairs;
  std::size_t callers = 0;

  if (scope_ != nullptr)
    for (const auto &[caller, entry] : scope_->items()) {
      const std::size_t before = pairs.size();
      append_pairs(
          pairs, packages, caller,
          [&](std::string_view target, std::uint32_t, bool target_is_system) {
            /* The pairs and the single-pair answer come from one gate. */
            return hides(caller, target, target_is_system, presets);
          });
      ++callers;
      /* One line per caller: a parse that did not happen shows up in the log
       * alone. */
      Log::info("  {}: {} pair(s) ({})", caller, pairs.size() - before,
                describe(entry, presets));
    }

  dedupe(pairs);
  log_summary(callers, pairs);
  report_presets(packages, presets, pairs);
  return pairs;
}

NativeRules::NativeRules(nlohmann::json config, std::filesystem::path path)
    : Rules(Tool::Native, std::move(config), std::move(path)) {
  if (const auto it = config_.find("apps");
      it != config_.end() && it->is_object())
    apps_ = &*it;
  mode_ = config_.value("mode", std::string{"blacklist"});
  hide_system_ = config_.value("hide_system", false);
}

std::unique_ptr<NativeRules>
NativeRules::load(const std::filesystem::path &path) {
  auto config = read_json(path);
  if (!config || !config->is_object()) {
    if (config)
      Log::warn("{} is not a JSON object", path.string());
    return nullptr;
  }
  /*
   * A format this build does not know is refused and not guessed at: a config
   * written for a newer layout would otherwise hide a different set than the
   * one its author sees, and a hide list that is off by a field is the failure
   * this whole thing exists to avoid.
   */
  const int version = config->value("version", 2);
  if (version > 2) {
    Log::warn("{}: format version {} is newer than this tool knows ({})",
              path.string(), version, 2);
    return nullptr;
  }
  return std::make_unique<NativeRules>(std::move(*config), path);
}

bool NativeRules::on_list(const nlohmann::json &entry,
                          std::string_view target) const {
  if (const auto *hide = find_array(entry, "hide");
      hide != nullptr && in_list(*hide, target))
    return true;

  const auto *applied = find_array(entry, "templates");
  if (applied == nullptr)
    return false;
  for (const auto &name : *applied) {
    if (!name.is_string())
      continue;
    const auto *tpl = template_entry(name.get_ref<const std::string &>());
    /* A native template is the array itself, not an object with an appList. */
    if (tpl != nullptr && tpl->is_array() && in_list(*tpl, target))
      return true;
  }
  return false;
}

bool NativeRules::hides_target(const nlohmann::json &entry,
                               std::string_view target,
                               bool target_is_system) const {
  const bool hide_system = entry.value("hide_system", hide_system_);
  /*
   * A system app is left visible unless the config asks otherwise. Hiding the
   * framework's own packages makes the device look broken long before it makes
   * an app look absent, and the pair is almost never what a user wants.
   */
  if (target_is_system && !hide_system)
    return false;

  const std::string mode = entry.value("mode", mode_);
  const bool whitelist = mode == "whitelist";
  if (!whitelist && entry.value("hide_all", false))
    return true;

  const bool listed = on_list(entry, target);
  return whitelist ? !listed : listed;
}

const nlohmann::json *NativeRules::entry(std::string_view caller) const {
  if (apps_ == nullptr)
    return nullptr;
  const auto it = apps_->find(std::string{caller});
  return it == apps_->end() ? nullptr : &*it;
}

bool NativeRules::hides(std::string_view caller, std::string_view target,
                        bool target_is_system, const Presets &) const {
  /* A caller never hides itself, whatever a config says: the kernel's own
   * self-check and the meaning of a pair both assume it. */
  if (caller == target)
    return false;
  const auto *caller_rules = entry(caller);
  if (caller_rules == nullptr)
    return false;
  return hides_target(*caller_rules, target, target_is_system);
}

Pairs NativeRules::expand(const PackageDb &packages, const Presets &) const {
  Pairs pairs;
  std::size_t callers = 0;

  if (apps_ != nullptr)
    for (const auto &[caller, caller_rules] : apps_->items()) {
      append_pairs(
          pairs, packages, caller,
          [&](std::string_view target, std::uint32_t, bool target_is_system) {
            return hides_target(caller_rules, target, target_is_system);
          });
      ++callers;
    }

  dedupe(pairs);
  log_summary(callers, pairs);
  return pairs;
}

} // namespace uidfake
