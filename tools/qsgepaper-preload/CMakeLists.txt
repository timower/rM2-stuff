cmake_minimum_required(VERSION 3.10)
project(qsgepaper-preload)

add_executable(qsgepaper-test main.cpp)
target_link_libraries(qsgepaper-test dl)

install(
  TARGETS qsgepaper-test
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)
