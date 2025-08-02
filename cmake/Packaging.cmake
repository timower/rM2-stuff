# Configure CPack to generate ipk files. Must be called after all install
# targets (which define components) has been added.
function(setup_cpack_ipk)

  set(CPACK_GENERATOR "DEB")
  set(CPACK_POST_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake/GenIPK.cmake")

  get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
  list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

  configure_file("${PROJECT_SOURCE_DIR}/cmake/CPackOptions.cmake.in"
                 "${PROJECT_BINARY_DIR}/CPackOptions.cmake" @ONLY)

  set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CPackOptions.cmake")

  include(CPack)

endfunction()

option(ADD_OPT_PREFIX
       "Add /opt prefix to relevant install targets. Used for toltec" OFF)
if(ADD_OPT_PREFIX)
  set(OPT_PREFIX "opt/")
else()
  set(OPT_PREFIX "")
endif()
