add_library(thick STATIC thick.cpp)
target_include_directories(thick PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(thick PRIVATE rMlib)
