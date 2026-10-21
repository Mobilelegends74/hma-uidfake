// SPDX-License-Identifier: GPL-2.0
#include "kmi.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

int main(int argc, char **argv) {
  try {
    struct utsname uts {};
    if (uname(&uts))
      throw std::runtime_error("uname failed");
    if (argc == 3 && std::string(argv[1]) == "load") {
      const auto file = std::filesystem::path{argv[2]};
      const auto preflight = uidfake::kmi::select(
          file.parent_path().parent_path(), "/", uts.release, uts.machine);
      if (preflight.selected.empty() ||
          file.filename() != preflight.selected + "_arm64_hma_uidfake.ko") {
        std::cerr << "[" << preflight.code << "] " << preflight.reason << "\n"
                  << preflight.evidence;
        return 1;
      }
      // No ksud/lkmloader, symbol rewriting, vermagic replacement, force flags,
      // init_module retry or second candidate. The kernel remains the
      // authority.
      const int fd = open(argv[2], O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
      if (fd < 0)
        throw std::runtime_error(std::strerror(errno));
      const int rc = syscall(SYS_finit_module, fd, "", 0);
      const int error = errno;
      close(fd);
      if (!rc)
        return 0;
      std::cerr << "[KERNEL_REJECTED] finit_module(flags=0): "
                << std::strerror(error) << " (errno=" << error
                << "); no force/rewrite/fallback. Check dmesg for CRC, "
                   "vermagic, symbol, signature or policy rejection.\n";
      return 1;
    }
    uidfake::kmi::Result result;
    if (argc == 3 && std::string(argv[1]) == "select") {
      result = uidfake::kmi::select(argv[2], "/", uts.release, uts.machine);
#ifdef UIDFAKE_KMI_TESTING
    } else if (argc == 6 && std::string(argv[1]) == "fixture") {
      result = uidfake::kmi::select(argv[2], argv[3], argv[4], argv[5]);
#endif
    } else {
      std::cerr
          << "usage: kmi-tool select MODULE_DIRECTORY | load MODULE_FILE\n";
      return 2;
    }
    std::cerr << "[" << result.code << "] " << result.reason << "\n"
              << result.evidence;
    if (result.selected.empty())
      return 1;
    std::cout << result.selected << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "[KMI_ERROR] " << error.what() << "\n";
    return 1;
  }
}
