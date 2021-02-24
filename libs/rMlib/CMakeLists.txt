project(rMlib)

find_library(LIBEVDEV_LIBRARY
  NAMES libevdev.so
  HINTS "$ENV{SDKTARGETSYSROOT}/usr/lib"
  REQUIRED)

find_path(LIBEVDEV_INCLUDE_DIR libevdev.h
  PATH_SUFFIXES libevdev-1.0/libevdev
  HINTS "$ENV{SDKTARGETSYSROOT}/usr/include"
  REQUIRED)

set(RMLIB_INCLUDES
  include/Canvas.h
  include/Device.h
  include/FrameBuffer.h
  include/Graphics.h
  include/Input.h
  include/MathUtil.h)

add_library(${PROJECT_NAME} STATIC
  Input.cpp
  Device.cpp
  FrameBuffer.cpp
  Canvas.cpp)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBEVDEV_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    ${LIBEVDEV_LIBRARY}
    udev)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(FILES ${RMLIB_INCLUDES} DESTINATION include/rmlib)
