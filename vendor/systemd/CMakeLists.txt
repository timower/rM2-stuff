find_package(PkgConfig)
pkg_check_modules(SYSTEMD libsystemd REQUIRED)

add_library(Systemd::Lib INTERFACE IMPORTED GLOBAL)
set_target_properties(
  Systemd::Lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${SYSTEMD_INCLUDE_DIRS}
                          INTERFACE_LINK_LIBRARIES ${SYSTEMD_LIBRARIES})
