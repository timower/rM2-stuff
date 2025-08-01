project(rMlib)

option(EMULATE "Emulate a rM tablet using SDL" OFF)
option(EMULATE_UINPUT "Emulate input devices using uinput" OFF)

set(RMLIB_SOURCES Device.cpp Canvas.cpp)

if(EMULATE)
  list(APPEND RMLIB_SOURCES EmulatedFramebuffer.cpp)
else()
  list(APPEND RMLIB_SOURCES FrameBuffer.cpp)
endif()

if(EMULATE AND NOT EMULATE_UINPUT)
  list(APPEND RMLIB_SOURCES EmulatedInput.cpp)
else()
  list(APPEND RMLIB_SOURCES Input.cpp)
endif()

add_library(${PROJECT_NAME} STATIC ${RMLIB_SOURCES})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE thick stb
  PUBLIC utf8cpp tl::expected unistdpp)

# If not emulating, or emulating uinput, link in udev, evdev and linux headers.
if(NOT EMULATE OR EMULATE_UINPUT)
  find_package(PkgConfig)
  pkg_check_modules(LIBEVDEV REQUIRED libevdev)
  message(STATUS "Found evdevv: ${LIBEVDEV_INCLUDE_DIRS}")

  target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE # Make sure to link libevdev statically
            -Wl,-Bstatic ${LIBEVDEV_LIBRARIES} -Wl,-Bdynamic udev linux::mxcfb)

  target_include_directories(${PROJECT_NAME} PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if(EMULATE)
  target_link_libraries(${PROJECT_NAME} PRIVATE NotoFont::font)

  find_package(SDL2 REQUIRED)
  if(TARGET SDL2::SDL2)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)
  else()
    target_link_libraries(${PROJECT_NAME} PUBLIC ${SDL2_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${SDL2_INCLUDE_DIRS})
  endif()
  target_link_libraries(${PROJECT_NAME} PUBLIC pthread)

  target_compile_definitions(${PROJECT_NAME} PUBLIC EMULATE)
  target_compile_definitions(${PROJECT_NAME} PUBLIC EMULATE_SCALE=2)

  if(EMULATE_UINPUT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EMULATE_UINPUT)
  endif()
endif()

install(
  TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/lib)
install(
  DIRECTORY include/
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/include/rmlib)
