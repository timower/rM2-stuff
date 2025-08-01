if(NOT EMULATE)
  return()
endif()

find_package(PkgConfig)
pkg_check_modules(LIBEVDEV libevdev)

project(rm2fb-emu)

add_executable(${PROJECT_NAME} rm2fb-emu.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE unistdpp rMlib rm2fb_headers)

project(rm2fb-test)
add_executable(${PROJECT_NAME} rm2fb-test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE unistdpp rMlib rm2fb_headers)
