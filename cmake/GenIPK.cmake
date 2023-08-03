# Cmake generates ar archives instead of tar.gz.
# So this post archive script re-packages the .deb using tar.

get_filename_component(PARENT_DIR "${CPACK_PACKAGE_FILES}" DIRECTORY)
set(TEMP_DIR "${PARENT_DIR}/tmp")
file(MAKE_DIRECTORY "${TEMP_DIR}")

execute_process(
  COMMAND ar x "${CPACK_PACKAGE_FILES}"
  WORKING_DIRECTORY "${TEMP_DIR}"
  COMMAND_ERROR_IS_FATAL ANY)

execute_process(
  COMMAND tar -czvf "${CPACK_PACKAGE_FILES}" "."
  WORKING_DIRECTORY "${TEMP_DIR}"
  COMMAND_ERROR_IS_FATAL ANY)

