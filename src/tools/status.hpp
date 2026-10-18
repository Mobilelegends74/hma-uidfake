// SPDX-License-Identifier: GPL-2.0
#pragma once

#include <optional>
#include <string_view>

namespace uidfake {

/*
 * KernelSU/Magisk description is only a live status: Working / Not working,
 * localized using the Android system locale saved by the module installer.
 * Detailed hook/rule/error counts remain available through sync-tool --status.
 * No operation takes place when module.prop does not exist next to the binary.
 */
void report_status(std::string_view text);

/*
 * The running kernel's config, as far as this tool needs it. GKI kernels ship
 * /proc/config.gz (CONFIG_IKCONFIG_PROC), and the module reports the geometry
 * it was built for, so the two can be compared where a user will see the
 * answer. Empty when the config is not readable, which is the only case that
 * says nothing.
 */
struct DeviceGeometry {
  std::optional<unsigned int> va_bits;
  std::optional<unsigned int> page_shift;
};

[[nodiscard]] DeviceGeometry read_device_config();

} // namespace uidfake
