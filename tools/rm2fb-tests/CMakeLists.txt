add_executable(perf perf.cpp)
target_link_libraries(perf PRIVATE rm2fb_lib)

if (NOT TARGET rm2fb_lib)
  return()
endif()

project(rm2fb-tests)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE rm2fb_lib)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE rm2fb_lib)

