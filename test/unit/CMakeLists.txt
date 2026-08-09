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

# AddressSanitizer intercepts dlopen(), which breaks how sdl2-compat's
# libSDL2.so finds its own libSDL3.so.0: normally ld.so resolves that
# dlopen() call's un-pathed filename using libSDL2.so's own RUNPATH, but
# under ASan the call appears (to ld.so) to originate from the sanitizer
# runtime statically linked into this executable, so it searches *our*
# RUNPATH instead - which doesn't contain SDL3's store path, so it's never
# found. Work around it by handing ctest libSDL2.so's own RUNPATH directly.
if(SANITIZE AND EMULATE)
  # find_package's imported targets are directory-scoped - libs/rMlib's own
  # SDL2::SDL2 (created by its own find_package(SDL2) call) isn't visible in
  # this sibling directory, so look it up again (cheap - SDL2_DIR is cached).
  find_package(SDL2 QUIET)
endif()
if(SANITIZE AND TARGET SDL2::SDL2)
  get_target_property(SDL2_LOCATION SDL2::SDL2 LOCATION)
  execute_process(
    COMMAND readelf -d ${SDL2_LOCATION}
    OUTPUT_VARIABLE SDL2_ELF_DYNAMIC_SECTION
    ERROR_QUIET)
  string(REGEX MATCH "\\(RUNPATH\\)[^\n]*\\[([^]]*)\\]"
               _ "${SDL2_ELF_DYNAMIC_SECTION}")
  if(CMAKE_MATCH_1)
    string(REPLACE ":" ";" SDL2_RUNPATH "${CMAKE_MATCH_1}")
  endif()
endif()

catch_discover_tests(${PROJECT_NAME} EXTRA_ARGS "--allow-running-no-tests"
                      DL_PATHS ${SDL2_RUNPATH})
