#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <unistd.h>

class TemporaryDirectory {
public:
  TemporaryDirectory(bool cwd = false) : dir("/tmp/unit-tests") {
    std::string templateName = "rm2-unit-test-XXXXXX";
    dir = std::filesystem::temp_directory_path() / mkdtemp(templateName.data());
    REQUIRE_NOTHROW(std::filesystem::create_directory(dir));

    if (cwd) {
      oldCwd = std::filesystem::current_path();
      std::filesystem::current_path(dir);
    }
  }
  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory(TemporaryDirectory&&) = default;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(TemporaryDirectory&&) = default;

  ~TemporaryDirectory() {
    if (oldCwd.has_value()) {
      std::filesystem::current_path(*oldCwd);
    }

    if (!dir.empty() && std::filesystem::is_directory(dir)) {
      REQUIRE_NOTHROW(std::filesystem::remove_all(dir));
    }
  }

  std::filesystem::path dir;
  std::optional<std::filesystem::path> oldCwd;
};
