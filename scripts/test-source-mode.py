#!/usr/bin/env python3
"""Compile and exercise real rule-source decisions in isolated host fixtures."""
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory(prefix='uidfake-source-test-') as tmp:
    tmp = Path(tmp)
    code = r'''
#include "paths.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
using namespace uidfake;
int main(int argc, char **argv) {
  const auto dir = std::filesystem::path{argv[1]};
  const auto mode = dir / "source-mode";
  const RuleSource manual{dir / "manual.json", Tool::Native};
  const RuleSource legacy{dir / "legacy.json", Tool::Hma};
  const RuleSource oss{dir / "oss.json", Tool::HmaOss};
  std::vector<RuleSource> sources{manual, legacy, oss};
  assert(read_rule_mode(mode) == RuleMode::External);
  std::ofstream(manual.pattern()) << "{}";
  assert(!RuleSource::active(sources, RuleMode::External));
  assert(RuleSource::active(sources, RuleMode::Manual)->tool() == Tool::Native);
  std::ofstream(legacy.pattern()) << "{}";
  assert(RuleSource::active(sources, RuleMode::External)->tool() == Tool::Hma);
  std::ofstream(oss.pattern()) << "{}";
  assert(RuleSource::active(sources, RuleMode::External)->tool() == Tool::HmaOss);
  assert(RuleSource::active(sources, RuleMode::Manual)->tool() == Tool::Native);
  assert(!RuleSource::active(sources, RuleMode::Invalid));
  std::filesystem::remove(manual.pattern());
  assert(!RuleSource::active(sources, RuleMode::Manual));
  assert(RuleSource::active(sources, RuleMode::External)->tool() == Tool::HmaOss);
  std::ofstream(mode) << "manual\n";
  assert(read_rule_mode(mode) == RuleMode::Manual);
  std::ofstream(mode) << "external\n";
  assert(read_rule_mode(mode) == RuleMode::External);
  std::ofstream(mode) << "manual injected\n";
  assert(read_rule_mode(mode) == RuleMode::Invalid);
  std::ofstream(mode) << "";
  assert(read_rule_mode(mode) == RuleMode::Invalid);
}
'''
    (tmp / 'test.cpp').write_text(code)
    subprocess.run(['g++', '-std=c++23', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-Isrc/tools', str(tmp / 'test.cpp'),
                    'src/tools/paths.cpp', '-o', str(tmp / 'test')], check=True)
    subprocess.run([str(tmp / 'test'), str(tmp)], check=True)
    subprocess.run(['g++', '-std=c++23', '-Isrc/tools',
                    'scripts/rule_sources_test.cpp', 'src/tools/paths.cpp',
                    '-o', str(tmp / 'legacy-test')], check=True)
    subprocess.run([str(tmp / 'legacy-test'), str(tmp / 'legacy-fixture')], check=True)

sync = Path('src/tools/sync.cpp').read_text()
assert 'sync_now("installed packages changed")' in sync
assert 'if (!policy_published_ ||' in sync
assert 'netlink_.push({})' in sync
assert 'if (rule_mode() == RuleMode::Manual)' in sync
assert '"rule_mode"' in sync and '"active_config"' in sync
watcher = Path('src/tools/watcher.cpp').read_text()
assert '.path = kPackagesXml' in watcher
assert 'source-mode' in Path('src/tools/paths.cpp').read_text()
print('OK: explicit source, stale/empty/deleted manual config, external preference, invalid preference, first empty push and package resync')
