set(WABBIT_INCLUDE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/core
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware
  ${CMAKE_CURRENT_SOURCE_DIR}/interface
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities
  PARENT_SCOPE)


set(WABBIT_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/core/core.c
  ${CMAKE_CURRENT_SOURCE_DIR}/interface/calc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/interface/state.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/81hw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/83hw.c
	${CMAKE_CURRENT_SOURCE_DIR}/hardware/83phw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/83psehw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/86hw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/lcd.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/link.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/colorlcd.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hardware/keys.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/indexcb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/alu.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/device.c
  ${CMAKE_CURRENT_SOURCE_DIR}/core/control.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/var.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/savestate.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/sendfile.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/label.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/gif.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/linksendvar.c
  # ${CMAKE_CURRENT_SOURCE_DIR}/utilities/screenshothandle.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/exportvar.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/sound.c
  ${CMAKE_CURRENT_SOURCE_DIR}/utilities/breakpoint.c
  # ${CMAKE_CURRENT_SOURCE_DIR}/utilities/zpipe.c
	${CMAKE_CURRENT_SOURCE_DIR}/interface.c
    PARENT_SCOPE)
