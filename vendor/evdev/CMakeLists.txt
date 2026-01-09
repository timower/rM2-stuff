find_package(PkgConfig)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

add_library(Evdev::Dynamic INTERFACE IMPORTED GLOBAL)
set_target_properties(
  Evdev::Dynamic
  PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${LIBEVDEV_INCLUDE_DIRS}
             INTERFACE_LINK_LIBRARIES ${LIBEVDEV_LIBRARIES})

add_library(Evdev::Static INTERFACE IMPORTED GLOBAL)
set_target_properties(
  Evdev::Static
  PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${LIBEVDEV_INCLUDE_DIRS}
             INTERFACE_LINK_LIBRARIES
             "-Wl,-Bstatic;${LIBEVDEV_LIBRARIES};-Wl,-Bdynamic")
