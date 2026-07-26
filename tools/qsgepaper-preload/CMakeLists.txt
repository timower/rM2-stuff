cmake_minimum_required(VERSION 3.10)
project(qsgepaper-preload)

add_executable(qsgepaper-test main.cpp swtcon.cpp native_init.cpp native_update.cpp native_display.cpp)
target_link_libraries(qsgepaper-test dl)

# libqsgepaper.so was built against the C++11 std::string SSO ABI
# (_GLIBCXX_USE_CXX11_ABI=1): in qsgepaper_init the ModeEntry name string has a
# 0x18-byte (24) layout and FUN_0004535c reads ModeEntry::luts at offset 0x18.
# That only lines up if our std::string is also 24 bytes (new ABI). This is the
# modern default, but pin it so a toolchain default change can't silently break
# the shared ModeEntry/vector/list layouts.
target_compile_definitions(qsgepaper-test PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

install(
  TARGETS qsgepaper-test
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)
