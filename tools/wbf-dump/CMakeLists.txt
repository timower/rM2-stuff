project(wbf-dump)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE swtcon)

install(
  TARGETS wbf-dump
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)
