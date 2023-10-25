project(yaft)

add_executable(${PROJECT_NAME}
  main.cpp
  fb/common.cpp
  keyboard.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib
    libYaft)

if (APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE linux::mxcfb)
endif()

install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/bin)

install(DIRECTORY ${CMAKE_BINARY_DIR}/terminfo
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/share)

install(FILES draft/yaft.draft
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/etc/draft)
install(FILES draft/yaft.png
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/etc/draft/icons)

add_deploy(${PROJECT_NAME})
