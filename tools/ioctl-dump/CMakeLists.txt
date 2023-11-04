# rm2fb uses Linux APIs extensively
if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  return()
endif()

project(ioctl-dump)

add_library(${PROJECT_NAME} SHARED main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_link_libraries(${PROJECT_NAME} PRIVATE linux::mxcfb)

add_executable(fb-dump dump.cpp)

