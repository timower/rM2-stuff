project(rm2fb)

add_library(rm2fb_headers INTERFACE)
target_include_directories(rm2fb_headers
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# rm2fb uses Linux APIs extensively
if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  return()
endif()

find_package(PkgConfig)
pkg_check_modules(LIBEVDEV libevdev REQUIRED)

set(COMMON_SRC
  Address.cpp
  SharedBuffer.cpp
  ControlSocket.cpp
  ImageHook.cpp
  uinput.cpp)

# Library used for the (socket) testing tool
add_library(rm2fb_lib STATIC ${COMMON_SRC})

target_include_directories(rm2fb_lib
  PRIVATE ${LIBEVDEV_INCLUDE_DIRS}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(rm2fb_lib
  PUBLIC
    rt
    dl
    unistdpp
    ${LIBEVDEV_LIBRARIES})

add_library(rm2fb_server SHARED Server.cpp)
target_link_libraries(rm2fb_server PRIVATE rm2fb_lib)

add_library(rm2fb_client SHARED
  Client.cpp
  IOCTL.cpp
  AddressHooking.cpp)

target_link_libraries(rm2fb_client
  PRIVATE
    linux::mxcfb
    Frida::Gum
    rm2fb_lib)

set_target_properties(rm2fb_client PROPERTIES
  VERSION "1.1.0"
  SOVERSION "1")

# Utility target that builds both the client and server libs.
add_custom_target(rm2fb DEPENDS rm2fb_client rm2fb_server)

install(TARGETS rm2fb_client rm2fb_server
  COMPONENT rm2fb
  DESTINATION opt/lib)
install(FILES rm2fb.service
  COMPONENT rm2fb
  DESTINATION lib/systemd/system)

add_deploy(rm2fb_client)
add_deploy(rm2fb_server)
