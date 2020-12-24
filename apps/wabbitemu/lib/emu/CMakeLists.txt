project(tiemu)

set(CORE_SRC calcs.c z80.c state.c rom.c flash.c link.c keypad.c lcd.c
	cert.c md5.c timers.c monolcd.c graylcd.c grayimage.c graycolor.c memory.c)

set(x7_SRC  x7/x7_init.c  x7/x7_io.c x7/x7_memory.c  x7/x7_subcore.c)
set(x1_SRC  x1/x1_init.c  x1/x1_io.c x1/x1_memory.c  x1/x1_subcore.c)
set(x2_SRC  x2/x2_init.c  x2/x2_io.c x2/x2_memory.c  x2/x2_subcore.c)
set(x3_SRC  x3/x3_init.c  x3/x3_io.c x3/x3_memory.c  x3/x3_subcore.c)
set(xp_SRC  xp/xp_init.c  xp/xp_io.c xp/xp_memory.c  xp/xp_subcore.c)
set(xs_SRC  xs/xs_init.c  xs/xs_io.c xs/xs_memory.c  xs/xs_subcore.c)
set(x4_SRC  x4/x4_init.c  x4/x4_io.c x4/x4_memory.c  x4/x4_subcore.c)
set(xz_SRC  xz/xz_init.c  xz/xz_io.c xz/xz_memory.c  xz/xz_subcore.c)
set(xn_SRC  xn/xn_init.c  xn/xn_io.c xn/xn_memory.c  xn/xn_subcore.c)
set(x5_SRC  x5/x5_init.c  x5/x5_io.c x5/x5_memory.c  x5/x5_subcore.c)
set(x6_SRC  x6/x6_init.c  x6/x6_io.c x6/x6_memory.c  x6/x6_subcore.c)

add_library(${PROJECT_NAME} STATIC
  ${CORE_SRC}
  ${x7_SRC}
  ${x1_SRC}
  ${x2_SRC}
  ${x3_SRC}
  ${xp_SRC}
  ${xs_SRC}
  ${x4_SRC}
  ${xz_SRC}
  ${xn_SRC}
  ${x5_SRC}
  ${x6_SRC})

target_compile_definitions(${PROJECT_NAME}
  PUBLIC PACKAGE_VERSION="2.0")


target_include_directories(${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
