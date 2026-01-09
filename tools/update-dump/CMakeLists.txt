project(update-dump)

add_library(${PROJECT_NAME} SHARED main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_link_libraries(${PROJECT_NAME} PRIVATE Frida::Gum)
