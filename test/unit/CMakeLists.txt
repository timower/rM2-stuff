project(unit-tests)

add_executable(
  ${PROJECT_NAME} TestUnistdpp.cpp TestRMLib.cpp TestTilem.cpp TestYaft.cpp
                  TestRocket.cpp TestRm2fb.cpp)

find_program(CAT_EXE cat)

target_compile_definitions(
  ${PROJECT_NAME} PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets"
                          CAT_EXE="${CAT_EXE}")

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE Catch2::Catch2WithMain
          unistdpp
          rMlib
          tilem::lib
          Yaft::app_lib
          rocket::lib
          rm2fb_headers)

# Server (Server.cpp/ServerInternal.h) only builds/is meaningful on Linux -
# see libs/rm2fb/CMakeLists.txt's own guard.
if(TARGET rm2fb_lib)
  target_sources(
    ${PROJECT_NAME}
    PRIVATE TestRm2fbServer.cpp ${CMAKE_SOURCE_DIR}/libs/rm2fb/Server.cpp
            ${CMAKE_SOURCE_DIR}/libs/rm2fb/ImageHook.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE rm2fb_lib Systemd::Lib
                                                Evdev::Static udev)
endif()

# swtcon builds on host too (KERNEL_MODE_C fallback) specifically for this.
if(TARGET swtcon)
  target_sources(${PROJECT_NAME} PRIVATE TestSwtcon.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE swtcon)

  # MockFb.cpp intercepts open()/ioctl() via linker symbol wrapping (GNU
  # ld/lld-only, and only meaningful where swtcon's own <linux/fb.h>-based
  # ioctls apply) to fake a working /dev/fb0 - see MockFb.h's own comment for
  # why. Scoped to just this executable target, not swtcon itself.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(${PROJECT_NAME} PRIVATE MockFb.cpp)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--wrap=open -Wl,--wrap=ioctl)
  endif()

  # assets/test.wbf.tar.xz is a real waveform (pulled from RemEmu) checked
  # in xz-compressed (~13x smaller) - extract it at build time instead of
  # storing the 280KB original. `cmake -E tar` uses CMake's own bundled
  # libarchive, so this needs no external xz/tar binary.
  set(SWTCON_TEST_WBF_DIR ${CMAKE_CURRENT_BINARY_DIR}/wbf)
  add_custom_command(
    OUTPUT ${SWTCON_TEST_WBF_DIR}/test.wbf
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SWTCON_TEST_WBF_DIR}
    COMMAND ${CMAKE_COMMAND} -E tar xf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/test.wbf.tar.xz
    WORKING_DIRECTORY ${SWTCON_TEST_WBF_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/test.wbf.tar.xz
    COMMENT "Extracting swtcon test waveform")
  add_custom_target(swtcon-test-wbf DEPENDS ${SWTCON_TEST_WBF_DIR}/test.wbf)
  add_dependencies(${PROJECT_NAME} swtcon-test-wbf)
  target_compile_definitions(
    ${PROJECT_NAME} PRIVATE SWTCON_TEST_WBF_DIR="${SWTCON_TEST_WBF_DIR}")
endif()

# From:
# https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md#catchcmake-and-catchaddtestscmake
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

catch_discover_tests(${PROJECT_NAME} EXTRA_ARGS "--allow-running-no-tests")
