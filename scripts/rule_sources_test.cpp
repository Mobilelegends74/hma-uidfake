// SPDX-License-Identifier: GPL-2.0
// Host test for the rule places: what a source points at, which source is the one
// in use, and what is watched for it.
#include "paths.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using uidfake::RuleSource;
using uidfake::Tool;

namespace
{

void must(bool ok, const char *what)
{
	if (ok)
		return;
	std::fprintf(stderr, "rule sources: FAILED (%s)\n", what);
	std::exit(1);
}

void write_config(const fs::path &path)
{
	fs::create_directories(path.parent_path());
	std::ofstream out{ path };
	out << "{}";
}

bool watched(const std::vector<RuleSource::Watch> &watches,
	     const fs::path &path, std::string_view name)
{
	return std::ranges::any_of(watches, [&](const RuleSource::Watch &w) {
		if (w.path != path)
			return false;
		/* A file watch has no name filter: it sees events without one. */
		return name.empty() ? w.names.empty() :
				      std::ranges::contains(w.names, name);
	});
}

} // namespace

int main(int argc, char **argv)
{
	const fs::path root = argc > 1 ? argv[1] :
					 fs::path{ "build/rule_sources_test" };
	fs::remove_all(root);

	/* The two apps keep their config in different places, which is what says which
   * of them a file belongs to. */
	const RuleSource hma{ root / "user0" / "files" / "config.json",
			      Tool::Hma };
	const RuleSource oss{ root / "misc" / "hide_my_applist_*" /
				      "config.json",
			      Tool::HmaOss };
	const std::vector<RuleSource> sources{ hma, oss };

	/* Neither is installed: nothing to read, and what gets watched is where one
   * would appear. */
	must(!hma.config(), "HMA's file is not there");
	must(!RuleSource::active(sources), "no source is in use");
	{
		const auto watches = hma.watches();
		must(watches.size() == 1 &&
			     watched(watches, hma.pattern().parent_path(),
				     "config.json"),
		     "waiting watches the directory, not a file");
		must(watched(oss.watches(), root / "misc", "hide_my_applist_*"),
		     "waiting watches where the data directory will appear");
	}

	/* Only HMA: its file is the source, and its file and directory are watched. */
	const fs::path hma_file = hma.pattern();
	write_config(hma_file);
	must(hma.config() == hma_file, "HMA's file is found");
	must(RuleSource::active(sources).has_value() &&
		     RuleSource::active(sources)->pattern() ==
			     sources.front().pattern(),
	     "the first source that exists is the one in use");
	{
		const auto watches = hma.watches();
		must(watches.size() == 2 &&
			     watched(watches, hma_file.parent_path(),
				     "config.json") &&
			     watched(watches, hma_file, ""),
		     "its file and that file's directory, and nothing else");
	}

	/* HMA-OSS as well: external mode prefers OSS over legacy HMA. */
	const fs::path oss_file =
		root / "misc" / "hide_my_applist_Ab3xY" / "config.json";
	write_config(oss_file);
	must(oss.config() == oss_file,
	     "the config under the random-suffixed directory is found");
	must(RuleSource::active(sources).has_value() &&
		     RuleSource::active(sources)->pattern() ==
			     sources[1].pattern(),
	     "HMA-OSS takes priority over legacy HMA in external mode");
	{
		const auto watches = oss.watches();
		/*
		 * Its directory and the file itself, plus the place a second data
		 * directory would appear in -- watched for that name alone, so no run
		 * can start reading a different config.
		 */
		must(watches.size() == 3 &&
			     watched(watches, oss_file.parent_path(),
				     "config.json") &&
			     watched(watches, oss_file, "") &&
			     watched(watches,
				     oss_file.parent_path().parent_path(),
				     oss.pattern()
					     .parent_path()
					     .filename()
					     .string()),
		     "its directory, its file, and the place a data directory appears");
	}

	/* Without HMA, HMA-OSS is the one in use. */
	fs::remove(hma_file);
	must(RuleSource::active(sources).has_value() &&
		     RuleSource::active(sources)->pattern() ==
			     sources[1].pattern(),
	     "HMA-OSS takes over when HMA is not installed");

	must(uidfake::leaf_matches("config.json", "config.json"), "exact name");
	must(uidfake::leaf_matches("hide_my_applist_x", "hide_my_applist_*"),
	     "prefix match");
	must(!uidfake::leaf_matches("hide_other", "hide_my_applist_*"),
	     "no match");

	/* The places this program looks in by default, in their order. */
	const auto known = RuleSource::known();
	must(known.size() == 4 && known[0].tool() == Tool::Native &&
		     known[1].tool() == Tool::Hma &&
		     known[2].tool() == Tool::HmaOss &&
		     known[2].pattern().string().find('*') != std::string::npos,
	     "saved manual config, HMA and OSS candidates; explicit mode decides");
	must(uidfake::tool_name(Tool::Hma) == "hma" &&
		     uidfake::tool_name(Tool::HmaOss) == "hma-oss",
	     "the tools have names");

	fs::remove_all(root);
	std::printf("rule sources: PASS\n");
	return 0;
}
