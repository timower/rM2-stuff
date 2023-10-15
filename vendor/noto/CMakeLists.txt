find_program(XXD xxd)

if ("${XXD}" STREQUAL "XXD-NOTFOUND")
  message(WARNING "XXD not found, skipping font include")
  return()
endif()

set(BUILD_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
set(FONT_HEADER "${BUILD_INCLUDE_DIR}/noto-sans-mono.h")
add_custom_command(
  OUTPUT "${FONT_HEADER}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BUILD_INCLUDE_DIR}"
  COMMAND
    ${XXD} -i "NotoSansMono-Regular.ttf"
              "${FONT_HEADER}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(NotoFont INTERFACE "${FONT_HEADER}")
target_include_directories(NotoFont INTERFACE "${BUILD_INCLUDE_DIR}")

add_library(NotoFont::font ALIAS NotoFont)
