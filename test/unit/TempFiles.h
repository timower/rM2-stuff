#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

class TemporaryDirectory {
public:
  TemporaryDirectory() : dir("/tmp/unit-tests") {
    REQUIRE_NOTHROW(std::filesystem::create_directory(dir));
  }
  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory(TemporaryDirectory&&) = default;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(TemporaryDirectory&&) = default;

  ~TemporaryDirectory() {
    if (!dir.empty() && std::filesystem::is_directory(dir)) {
      REQUIRE_NOTHROW(std::filesystem::remove_all(dir));
    }
  }

  std::filesystem::path dir;
};
