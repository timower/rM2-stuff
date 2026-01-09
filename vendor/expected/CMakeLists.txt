FetchContent_Declare(
  expected
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.tar.gz)

set(EXPECTED_BUILD_TESTS OFF)
FetchContent_MakeAvailable(expected)
