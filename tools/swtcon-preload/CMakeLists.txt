if (NOT TARGET swtcon)
  return()
endif()

project(swtcon-preload)

add_library(${PROJECT_NAME} SHARED
  main.cpp)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    swtcon
    dl)
