// SPDX-License-Identifier: GPL-2.0
#include <cstdio>
#include <exception>

#include "sync.hpp"

int main(int argc, char **argv) {
  /* The daemon has to be quiet about it: an exception that escapes main would
   * take it down with a message nobody reads, after it has been supervisoring
   * the policy for days. */
  try {
    const auto config = uidfake::parse_args(argc, argv);
    if (!config)
      return 2;

    uidfake::Syncer syncer(*config);
    if (config->make_template) {
      syncer.template_for(*config->make_template, config->write_config);
      return 0;
    }
    if (config->list) {
      syncer.list_targets(*config->list);
      return 0;
    }
    if (config->explain) {
      syncer.explain(config->explain->first, config->explain->second);
      return 0;
    }
    /* The WebUI's commands print one JSON document and exit. */
    if (config->status) {
      syncer.print_status();
      return 0;
    }
    if (config->list_packages) {
      syncer.print_packages();
      return 0;
    }
    if (config->get_config) {
      syncer.print_config();
      return 0;
    }
    if (config->set_config)
      return syncer.set_config() ? 0 : 1;
    return syncer.run() ? 0 : 1;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "sync-tool: %s\n", e.what());
    return 1;
  }
}
