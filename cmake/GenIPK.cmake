# Cmake generates ar archives instead of tar.gz.
# So this post archive script re-packages the .deb using tar.

foreach(PACKAGE_FILE IN LISTS CPACK_PACKAGE_FILES)
  get_filename_component(PARENT_DIR "${PACKAGE_FILE}" DIRECTORY)

  set(TEMP_DIR "${PARENT_DIR}/tmp")
  file(MAKE_DIRECTORY "${TEMP_DIR}")

  execute_process(
    COMMAND ar x "${PACKAGE_FILE}"
    WORKING_DIRECTORY "${TEMP_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)

  execute_process(
    COMMAND tar -czf "${PACKAGE_FILE}" "."
    WORKING_DIRECTORY "${TEMP_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)

  message(STATUS "Created ${PACKAGE_FILE} as IPK")
endforeach()
