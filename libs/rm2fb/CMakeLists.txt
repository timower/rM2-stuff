project(rm2fb)

set(COMMON_SRC
  Address.cpp
  SharedBuffer.cpp
  Socket.cpp
  ImageHook.cpp)


# Library used for the (socket) testing tool
add_library(rm2fb_lib OBJECT ${COMMON_SRC})
target_compile_features(rm2fb_lib PUBLIC cxx_std_17)
target_include_directories(rm2fb_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rm2fb_lib PUBLIC rt dl)

if (${EMULATE})
  return()
endif()

add_library(rm2fb_server SHARED Server.cpp ${COMMON_SRC})
target_compile_features(rm2fb_server PUBLIC cxx_std_17)
target_link_libraries(rm2fb_server PRIVATE rt dl)

add_library(rm2fb_client SHARED
  Client.cpp IOCTL.cpp AddressHooking.cpp ${COMMON_SRC})
target_compile_features(rm2fb_client PUBLIC cxx_std_17)
target_link_libraries(rm2fb_client PRIVATE linux::mxcfb Frida::Gum rt dl)

# Utility target that builds both the client and server libs.
add_custom_target(rm2fb DEPENDS rm2fb_client rm2fb_server)

install(TARGETS rm2fb_client rm2fb_server DESTINATION opt/lib)
install(FILES rm2fb.service DESTINATION lib/systemd/system)
