add_library(tomlpp INTERFACE)
add_library(tomlplusplus::tomlplusplus ALIAS tomlpp)
target_include_directories(tomlpp SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

