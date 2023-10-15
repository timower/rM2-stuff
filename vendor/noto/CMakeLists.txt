find_program(XXD xxd REQUIRED)

set(BUILD_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
set(FONT_HEADER "${BUILD_INCLUDE_DIR}/noto-sans-mono.h")
add_custom_command(
  OUTPUT "${FONT_HEADER}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BUILD_INCLUDE_DIR}"
  COMMAND
    ${XXD} -n "noto_sans_mono_font"
           -i "${CMAKE_CURRENT_SOURCE_DIR}/NotoSansMono-Regular.ttf"
              "${FONT_HEADER}")

add_library(NotoFont INTERFACE "${FONT_HEADER}")
target_include_directories(NotoFont INTERFACE "${BUILD_INCLUDE_DIR}")
