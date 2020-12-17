#pragma once

namespace swtcon {

constexpr int
normPhase(int i) {
  return i < 0 ? -(-i & 0xf) : i & 0xf;
}

} // namespace swtcon
