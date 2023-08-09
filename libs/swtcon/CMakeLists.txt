if (NOT "${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  return()
endif()

project(swtcon)

add_library(${PROJECT_NAME} STATIC
  SwtconState.cpp
  swtcon.cpp
  fb.cpp
  Waveforms.cpp
  Vsync.cpp)

set_property(TARGET ${PROJECT_NAME}
  PROPERTY
    POSITION_INDEPENDENT_CODE ON)


target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME} pthread)
