add_library(linux_mxcfb INTERFACE)
add_library(linux::mxcfb ALIAS linux_mxcfb)
target_include_directories(linux_mxcfb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
