#pragma once

namespace swtcon {

constexpr auto pan_buffer_size = 0x580; // size in y direction == 1408
constexpr auto pan_buffers_count = 0x10;

constexpr auto pan_bits_per_pixel = 4;
constexpr auto pan_line_size = 0x410; // 2080 * 4 / 8 bytes

} // namespace swtcon
