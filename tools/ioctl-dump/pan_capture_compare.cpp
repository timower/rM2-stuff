// Diffs two SWTCON_PAN_CAPTURE dumps (see main.cpp's capture_pan) - the
// black-box, ioctl-level A/B check: run the same qsgepaper-test input once
// native and once with SWTCON_LIBIMPL=1 (each writing its own capture file),
// then compare here.
//
// Comparison is ORDERED: hash at position i in capture A is compared against
// the hash at position i in capture B, in sequence order. This assumes both
// captures come from a single, isolated test case (see
// tools/swtcon-test/ab_compare.sh, which runs and diffs one test case
// at a time by default) - real-time frame pacing and cross-test interleaving
// can still shift how many transient waveform frames a *whole-suite* run
// produces (documented previously as 677 vs. 355 deduped records for an
// identical full-suite run, native vs. library), which would make a
// positional compare noisy garbage for that case. Per isolated test case,
// the two runs are expected to produce the same ordered sequence of
// displayed content.
//
// Exit code: 0 = identical ordered sequence, 1 = a length or positional
// mismatch, 2 = I/O error.
//
// idx==16 (kInitFrameSlotIndex, libs/swtcon/qsgepaper_globals.h) entries -
// worker_thread_func's per-tick keep-alive pan, always showing the same
// static blank/white reference content, never real playback - are excluded
// upstream, in main.cpp's capture_display, not here. They used to be
// dropped in this file instead, but that was too late: capture_display's
// own consecutive-hash dedup runs first and tracks "last shown hash" across
// every pan including idle keep-alive ticks, so if an idle tick happened to
// fire (real-time-scheduling-dependent, differs between native and
// SWTCON_LIBIMPL=1 runs) moments before a genuine content transition that
// happens to hash identically to that static reference (e.g. a waveform's
// own white flash phase - not hypothetical, confirmed via
// SWTCON_PAN_CAPTURE_ALL showing byte-identical undeduped content on a run
// where the deduped comparison mismatched), the real transition got masked
// as "no change" and silently dropped from the very capture file this tool
// reads - no amount of filtering here could recover it. See main.cpp's
// capture_display for the actual fix.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<uint32_t>
read_hashes(const char* path) {
  std::ifstream f(path);
  std::vector<uint32_t> hashes;
  std::string line;
  while (std::getline(f, line)) {
    auto pos = line.find("hash=");
    if (pos == std::string::npos)
      continue;
    uint32_t h = 0;
    std::istringstream(line.substr(pos + 5)) >> std::hex >> h;
    if (hashes.empty() || hashes.back() != h)
      hashes.push_back(h);
  }
  return hashes;
}

} // namespace

int
main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <capture_a> <capture_b>\n";
    return 2;
  }

  std::ifstream check_a(argv[1]), check_b(argv[2]);
  if (!check_a.good() || !check_b.good()) {
    std::cerr << "error: could not open one or both capture files\n";
    return 2;
  }

  std::vector<uint32_t> a = read_hashes(argv[1]);
  std::vector<uint32_t> b = read_hashes(argv[2]);

  std::cout << argv[1] << ": " << a.size() << " frame hashes\n";
  std::cout << argv[2] << ": " << b.size() << " frame hashes\n";

  size_t n = std::min(a.size(), b.size());
  size_t mismatches = 0;
  size_t shown = 0;
  for (size_t i = 0; i < n; i++) {
    if (a[i] == b[i])
      continue;
    mismatches++;
    if (shown++ < 20) {
      std::cout << "  seq=" << i << ": hash=" << std::hex << std::setfill('0')
                << std::setw(8) << a[i] << " vs hash=" << std::setw(8) << b[i]
                << std::dec << "\n";
    }
  }
  if (shown < mismatches)
    std::cout << "  ... (" << mismatches - shown << " more mismatches)\n";

  bool length_mismatch = a.size() != b.size();
  if (length_mismatch) {
    std::cout << "length mismatch: " << argv[1] << " has " << a.size() << ", "
              << argv[2] << " has " << b.size() << "\n";
  }

  if (mismatches == 0 && !length_mismatch) {
    std::cout << "MATCH: identical ordered sequence (" << a.size()
              << " hashes)\n";
    return 0;
  }

  std::cout << "MISMATCH: " << mismatches << " of " << n
            << " compared positions differ"
            << (length_mismatch ? "; plus a length mismatch" : "") << "\n";
  return 1;
}
