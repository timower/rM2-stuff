add_library(systemdpp STATIC sdbus.cpp)
target_include_directories(systemdpp PUBLIC include/)

target_link_libraries(systemdpp PUBLIC unistdpp)

if(NOT APPLE)
  target_link_libraries(systemdpp PUBLIC Systemd::Lib)
endif()
