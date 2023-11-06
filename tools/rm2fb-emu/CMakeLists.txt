find_package(PkgConfig)
pkg_check_modules(LIBEVDEV libevdev)

if (EMULATE)
  project(rm2fb-emu)
  add_executable(${PROJECT_NAME} rm2fb-emu.cpp)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE unistdpp rMlib)

  project(rm2fb-test)
  add_executable(${PROJECT_NAME} rm2fb-test.cpp)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      unistdpp
      rMlib
      tomlplusplus::tomlplusplus)

endif()

if(LIBEVDEV_FOUND)
  project(rm2fb-forward)

  add_executable(${PROJECT_NAME} rm2fb-forward.cpp)

  target_link_libraries(${PROJECT_NAME}
    PRIVATE unistdpp rm2fb_lib ${LIBEVDEV_LIBRARIES})
  target_include_directories(${PROJECT_NAME}
    PRIVATE ${LIBEVDEV_INCLUDE_DIRS})

  install(TARGETS ${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
    DESTINATION opt/bin)

  add_deploy(rm2fb-forward)
endif()

