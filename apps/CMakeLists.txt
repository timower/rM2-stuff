add_subdirectory(tilem)
add_subdirectory(rocket)
add_subdirectory(yaft)

add_custom_target(apps)
add_dependencies(apps tilem yaft rocket)
