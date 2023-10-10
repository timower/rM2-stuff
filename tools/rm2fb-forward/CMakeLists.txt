project(rm2fb-forward)

find_package(PkgConfig)
pkg_check_modules(LIBEVDEV libevdev)

if (NOT LIBEVDEV_FOUND)
  return()
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE rm2fb_lib ${LIBEVDEV_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBEVDEV_INCLUDE_DIRS})

