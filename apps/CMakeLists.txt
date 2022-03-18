add_subdirectory(tilem)
add_subdirectory(rocket)
add_subdirectory(yaft)
add_subdirectory(yaft2)

add_custom_target(apps)
add_dependencies(apps tilem yaft rocket yaft2)
