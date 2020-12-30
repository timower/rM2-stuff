project(rMlib)

add_library(${PROJECT_NAME} STATIC
  Input.cpp
  Device.cpp
  FrameBuffer.cpp
  Canvas.cpp)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
