option(COVERAGE "Generate coverage data in debug builds" OFF)
option(SANITIZE "Use asan in debug builds" OFF)

# Set fPIC, even for shared libraries..
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(BASE_OPTS
  -fdiagnostics-color=always
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-gnu-statement-expression
  -Wno-gnu-statement-expression-from-macro-expansion
  -Wno-psabi)

set(COVER_OPTS
  -fprofile-instr-generate
  -fcoverage-mapping)

if (COVERAGE)
  list(APPEND BASE_OPTS ${COVER_OPTS})
endif()

set(ASAN_OPTS
  -fsanitize=address
  -fno-omit-frame-pointer
  -fsanitize=undefined)

if (SANITIZE)
  list(APPEND BASE_OPTS ${ASAN_OPTS})
endif()

list(JOIN BASE_OPTS " " BASE_OPTS)

set(CMAKE_C_FLAGS_DEBUG
  "${BASE_OPTS} -g"
  CACHE STRING ""
  FORCE)
set(CMAKE_CXX_FLAGS_DEBUG
  "${BASE_OPTS} -g -Wno-c++20-designator"
  CACHE STRING ""
  FORCE)
