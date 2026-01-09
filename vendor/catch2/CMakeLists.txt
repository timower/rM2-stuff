include(FetchContent)

FetchContent_Declare(
  Catch2
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz
      FIND_PACKAGE_ARGS CONFIG)
FetchContent_MakeAvailable(Catch2)
