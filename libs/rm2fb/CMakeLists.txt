project(rm2fb)

add_library(rm2fb_headers OBJECT ControlSocket.cpp SharedBuffer.cpp)
target_include_directories(rm2fb_headers PUBLIC ./include)
target_link_libraries(rm2fb_headers PUBLIC unistdpp)

# rm2fb uses Linux APIs extensively
if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  return()
endif()

find_package(PkgConfig)
pkg_check_modules(LIBEVDEV libevdev REQUIRED)

# Library used for the (socket) testing tool
add_library(
  rm2fb_lib STATIC
  InputDevice.cpp
  PreloadHooks.cpp
  Versions/Version.cpp
  Versions/Version2.15.cpp
  Versions/Version3.5.cpp
  Versions/Version3.8.cpp
  Versions/Version3.20.cpp)

target_include_directories(
  rm2fb_lib
  PRIVATE ${LIBEVDEV_INCLUDE_DIRS}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(rm2fb_lib PUBLIC linux::mxcfb rt dl unistdpp
                                       rm2fb_headers)

add_library(rm2fb_server SHARED ServerLib.cpp Server.cpp ImageHook.cpp)
target_link_libraries(rm2fb_server PRIVATE rm2fb_lib Systemd::Lib
                                           Evdev::Dynamic udev)

add_executable(rm2fb_server_exe ServerExe.cpp Server.cpp ImageHook.cpp)
set_property(TARGET rm2fb_server_exe PROPERTY OUTPUT_NAME rm2fb_server)

target_link_libraries(rm2fb_server_exe PRIVATE rm2fb_lib Systemd::Lib pthread)
target_link_libraries(rm2fb_server_exe PRIVATE Evdev::Static udev)

target_compile_definitions(rm2fb_server_exe PRIVATE USE_QSGEPAPER)

# rm2fb server backed directly by the native swtcon reimplementation instead
# of by-address hooking into a version-specific libqsgepaper.so (compare
# ServerExe.cpp/rm2fb_server_exe above) - drives the display straight off
# swtcon_*, reading/writing rm2fb's own shared framebuffer. Only builds where
# swtcon itself does (32-bit ARM target - see libs/swtcon/CMakeLists.txt).
if(TARGET swtcon)
  add_executable(rm2fb_server_swtcon ServerSwtcon.cpp Server.cpp ImageHook.cpp)
  target_link_libraries(rm2fb_server_swtcon PRIVATE swtcon rm2fb_lib
                                                     Evdev::Static udev Systemd::Lib pthread)
endif()

add_library(rm2fb_client SHARED Client.cpp ClientSocket.cpp IOCTL.cpp
                                ImageHook.cpp AddressHooking.cpp)

target_link_libraries(rm2fb_client PRIVATE Frida::Gum rm2fb_lib)

set_target_properties(rm2fb_client PROPERTIES VERSION "1.1.0" SOVERSION "1")

add_library(rm2fb_client_no_hook SHARED Client.cpp ClientSocket.cpp IOCTL.cpp
                                        ImageHook.cpp)
target_link_libraries(rm2fb_client_no_hook PRIVATE rm2fb_lib)
target_compile_definitions(rm2fb_client_no_hook PRIVATE NO_HOOKING)

# rm2fb client, xochitl-coexistence variant (see ClientSwtcon.cpp) - lets
# xochitl run its own statically-linked swtcon completely untouched instead
# of the by-address per-xochitl-version hooking rm2fb_client above uses.
# Deliberately not linked against rm2fb_lib, which bundles Versions/*.cpp
# and evdev - PreloadHooks.cpp (the only piece of rm2fb_lib this needs, for
# the malloc/calloc redirect) is compiled directly instead, so this target
# has zero Versions.cpp/Frida dependency. Only builds where swtcon does
# (32-bit ARM target - see libs/swtcon/CMakeLists.txt), since it's meant to
# pair with rm2fb_server_swtcon.
if(TARGET swtcon)
  add_library(rm2fb_client_swtcon SHARED ClientSwtcon.cpp ClientSocket.cpp
                                         IOCTL.cpp ImageHook.cpp
                                         PreloadHooks.cpp)
  target_link_libraries(rm2fb_client_swtcon PRIVATE rm2fb_headers linux::mxcfb)
  set_target_properties(rm2fb_client_swtcon PROPERTIES VERSION "1.0.0"
                                                        SOVERSION "1")
endif()

# Utility target that builds both the client and server libs.
add_custom_target(rm2fb DEPENDS rm2fb_client rm2fb_server)

add_executable(rm2fbctl rm2fbctl.cpp)
target_link_libraries(rm2fbctl PRIVATE rm2fb_headers)

install(
  TARGETS rm2fbctl
  COMPONENT rm2display
  DESTINATION ${OPT_PREFIX}bin)

install(
  TARGETS rm2fb_server_exe
  COMPONENT rm2display
  DESTINATION ${OPT_PREFIX}bin)

install(
  TARGETS rm2fb_client rm2fb_server rm2fb_client_no_hook
  COMPONENT rm2display
  DESTINATION ${OPT_PREFIX}lib)

if(ADD_OPT_PREFIX)
  # Make a symlink in /usr/lib to librm2fb_client.so.1
  execute_process(COMMAND ln -sf /opt/lib/librm2fb_client.so.1
                          "${CMAKE_BINARY_DIR}/librm2fb_client.so.1")
  install(
    FILES "${CMAKE_BINARY_DIR}/librm2fb_client.so.1"
    DESTINATION usr/lib
    COMPONENT rm2display)
endif()

# Not built (and so not installed) on non-32-bit targets - see swtcon's own
# guard in libs/swtcon/CMakeLists.txt.
if(TARGET rm2fb_server_swtcon)
  install(
    TARGETS rm2fb_server_swtcon
    COMPONENT rm2display
    DESTINATION ${OPT_PREFIX}bin)
endif()

if(TARGET rm2fb_client_swtcon)
  install(
    TARGETS rm2fb_client_swtcon
    COMPONENT rm2display
    DESTINATION ${OPT_PREFIX}lib)
endif()

install(
  FILES rm2fb.service rm2fb.socket
  COMPONENT rm2display
  DESTINATION lib/systemd/system)

install(
  FILES opkg/rm2fb-preload.env
  COMPONENT rm2display
  DESTINATION ${OPT_PREFIX}etc/xochitl.env.d)

add_deploy(rm2fb_client)
add_deploy(rm2fb_server)
