add_subdirectory(test)
add_subdirectory(swtcon-preload)
add_subdirectory(input-test)
add_subdirectory(ui-tests)
add_subdirectory(rm2fb-tests)

add_custom_target(tools)
add_dependencies(tools misc-test swtcon-preload input-test ui-tests)
