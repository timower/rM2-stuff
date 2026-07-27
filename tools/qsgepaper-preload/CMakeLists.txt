cmake_minimum_required(VERSION 3.10)
project(qsgepaper-preload)

add_executable(qsgepaper-test main.cpp swtcon.cpp native_init.cpp native_update.cpp native_display.cpp ab_capture.cpp)
target_link_libraries(qsgepaper-test dl)

# libqsgepaper.so was built against the C++11 std::string SSO ABI
# (_GLIBCXX_USE_CXX11_ABI=1): in qsgepaper_init the ModeEntry name string has a
# 0x18-byte (24) layout and FUN_0004535c reads ModeEntry::luts at offset 0x18.
# That only lines up if our std::string is also 24 bytes (new ABI). This is the
# modern default, but pin it so a toolchain default change can't silently break
# the shared ModeEntry/vector/list layouts.
target_compile_definitions(qsgepaper-test PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

# A/B verification tool for render_update_kernel (still-library, Phase 6) -
# see render_kernel_verify.cpp's header comment.
add_executable(render-kernel-verify render_kernel_verify.cpp native_init.cpp)
target_link_libraries(render-kernel-verify dl)
target_compile_definitions(render-kernel-verify PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

# Empirical input->output addressing mapper for render_update_kernel (Phase 6
# porting work) - see render_kernel_addr_map.cpp's header comment.
add_executable(render-kernel-addr-map render_kernel_addr_map.cpp native_init.cpp)
target_link_libraries(render-kernel-addr-map dl)
target_compile_definitions(render-kernel-addr-map PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

# Empirical probe for dispatch_processed_regions (still-library, the last
# remaining by-address call in the native port) - see
# dispatch_processed_regions_probe.cpp's header comment.
add_executable(dispatch-processed-regions-probe dispatch_processed_regions_probe.cpp native_init.cpp native_update.cpp)
target_link_libraries(dispatch-processed-regions-probe dl)
target_compile_definitions(dispatch-processed-regions-probe PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

# A/B regression harness: runs a matrix of update sequences and captures the
# transient drive stream so a native-vs-library (SWTCON_LIBDISPATCH) diff can
# catch pipeline regressions the settled buffers miss - see ab_harness.cpp.
add_executable(swtcon-ab-test ab_harness.cpp swtcon.cpp native_init.cpp native_update.cpp native_display.cpp ab_capture.cpp)
target_link_libraries(swtcon-ab-test dl)
target_compile_definitions(swtcon-ab-test PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

# Empirical probe for the two worker-side playback kernels (still-library,
# the last remaining by-address calls in the pipeline) - see
# playback_kernel_probe.cpp's header comment.
add_executable(playback-kernel-probe playback_kernel_probe.cpp native_init.cpp native_update.cpp)
target_link_libraries(playback-kernel-probe dl)
target_compile_definitions(playback-kernel-probe PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

install(
  TARGETS qsgepaper-test
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)
