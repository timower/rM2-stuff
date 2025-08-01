include(FetchContent)

FetchContent_Declare(
  catch2
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz)
FetchContent_MakeAvailable(catch2)
