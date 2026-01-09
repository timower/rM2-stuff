set(SSH_HOST
    "RemEmu"
    CACHE STRING "SSH host to deploy to")

set(RUN_PREFIX
    "LD_PRELOAD=/home/root/librm2fb_client.so"
    CACHE STRING "Prefix when running over SSH")

find_program(SCP scp)
find_program(SSH ssh)

if("${SCP}" STREQUAL "SCP-NOTFOUND")
  message(WARNING "scp not found, skipping deploy")
endif()
if("${SSH}" STREQUAL "SSH-NOTFOUND")
  message(WARNING "scp not found, skipping deploy")
endif()

function(add_deploy target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "add_deploy called on non-existent target: ${target}")
  endif()

  if("${SCP}" STREQUAL "SCP-NOTFOUND")
    return()
  endif()

  add_custom_target(
    deploy_${target}
    COMMAND ${SCP} $<TARGET_FILE:${target}> "${SSH_HOST}:"
    DEPENDS ${target})

  if("${SSH}" STREQUAL "SSH-NOTFOUND")
    return()
  endif()

  add_custom_target(
    run_${target}
    COMMAND ${SSH} -t -t "${SSH_HOST}" ${RUN_PREFIX}
            "./$<TARGET_FILE_NAME:${target}>"
    DEPENDS deploy_${target}
    USES_TERMINAL)
endfunction()

function(add_opkg_targets)
  if("${SCP}" STREQUAL "SCP-NOTFOUND" OR "${SSH}" STREQUAL "SSH-NOTFOUND")
    return()
  endif()

  get_cmake_property(all_comps COMPONENTS)
  list(REMOVE_ITEM all_comps "Unspecified")

  foreach(comp IN LISTS all_comps)
    add_custom_target(
      opkg_${comp}
      COMMAND "${SCP}" "${CMAKE_BINARY_DIR}/${comp}.ipk" "${SSH_HOST}:"
      COMMAND "${SSH}" "${SSH_HOST}" bash -l -c
              "'opkg install --force-reinstall ${comp}.ipk'"
      DEPENDS package)
  endforeach()

endfunction()
