set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_SYSROOT "/opt/codex/rm11x/3.1.2/sysroots/cortexa7hf-neon-remarkable-linux-gnueabi")

set(host_root "/opt/codex/rm11x/3.1.2/sysroots/x86_64-codexsdk-linux")
set(triple "arm-remarkable-linux-gnueabi")
set(tools "${host_root}/usr/bin/${triple}")
set(prefix "${tools}/${triple}-")

set(CMAKE_C_COMPILER "${prefix}gcc")
set(CMAKE_C_COMPILER_AR "${prefix}gcc-ar")
set(CMAKE_C_COMPILER_RANLIB "${prefix}gcc-ranlib")
# TODO: remove sysroot?
set(CMAKE_C_COMPILER_ARG1 "-mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=${sysroot}")


set(CMAKE_CXX_COMPILER "${prefix}g++")
set(CMAKE_CXX_COMPILER_AR "${prefix}gcc-ar")
set(CMAKE_CXX_COMPILER_RANLIB "${prefix}gcc-ranlib")
# TODO: remove sysroot?
set(CMAKE_CXX_COMPILER_ARG1 "-mfpu=neon -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=${sysroot}")

set(CMAKE_ADDR2LINE "${prefix}addr2line")
set(CMAKE_AR "${prefix}ar")
set(CMAKE_NM "${prefix}nm")
set(CMAKE_OBJCOPY "${prefix}objcopy")
set(CMAKE_OBJDUMP "${prefix}objdump")
set(CMAKE_RANLIB "${prefix}ranlib")
set(CMAKE_READELF "${prefix}readelf")
set(CMAKE_STRIP "${prefix}strip")


set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
