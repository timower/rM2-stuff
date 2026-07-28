# rm2fb uses Linux APIs extensively
if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  return()
endif()

project(ioctl-dump)

add_library(${PROJECT_NAME} SHARED main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_link_libraries(${PROJECT_NAME} PRIVATE linux::mxcfb)

add_executable(fb-dump dump.cpp)

# Diffs two SWTCON_PAN_CAPTURE dumps produced by libioctl-dump.so's
# capture_pan (main.cpp) - the black-box A/B check for SWTCON_LIBIMPL, run
# the same way as swtcon-ab-test --compare (on-target/emulator, via ssh - see
# CLAUDE.md's Building and Testing section).
add_executable(pan-capture-compare pan_capture_compare.cpp)

install(
  TARGETS ioctl-dump
  COMPONENT ioctl-dump
  DESTINATION ${OPT_PREFIX}lib)

install(
  FILES rm2fb_ioctl_dump.conf
  COMPONENT ioctl-dump
  DESTINATION etc/systemd/system/rm2fb.service.d/)
