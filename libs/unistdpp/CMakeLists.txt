project(unistdpp)

add_library(${PROJECT_NAME} STATIC
  unistdpp.cpp
  file.cpp
  pipe.cpp
  poll.cpp
  socket.cpp
  mmap.cpp)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC tl::expected)
