# Only buildable once vendor/libghostty has a prefix (see GHOSTTY_VT_PREFIX).
if(NOT TARGET ghostty-vt)
  return()
endif()

project(ghostty-vt-smoketest)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ghostty-vt)

install(
  TARGETS ghostty-vt-smoketest
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)
