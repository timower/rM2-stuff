// Diffs two SWTCON_PAN_CAPTURE dumps (see main.cpp's capture_pan) - the
// black-box, ioctl-level A/B check: run the same qsgepaper-test input once
// native and once with SWTCON_LIBIMPL=1 (each writing its own capture file),
// then compare here.
//
// Comparison is by SET of distinct content hashes, not position or count.
// A positional/count-based diff was tried first and doesn't work: the panel
// pans continuously at a fixed real-time cadence regardless of content (see
// main.cpp's capture_pan comment), so even after deduping idle repeats, the
// *number* of transient waveform frames a run produces varies with how fast
// that run's process happens to run relative to the panel's frame-pacing
// clock (confirmed empirically: 677 vs. 355 deduped records for an
// identical test, native vs. library) - this project's own AGENTS.md already
// documents this class of real-time jitter for the internal PLAY capture.
// Slot index isn't a stable sync key either - multiple ring slots
// legitimately carry identical content during priming/blank fills. What
// should hold regardless of timing is: every distinct frame content that
// got displayed in one run also got displayed somewhere in the other run.
//
// Exit code: 0 = same set of distinct hashes, 1 = a hash present in one run
// never appeared in the other, 2 = I/O error.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>

namespace {

std::set<uint32_t>
read_hashes(const char* path) {
  std::ifstream f(path);
  std::set<uint32_t> hashes;
  std::string line;
  while (std::getline(f, line)) {
    auto pos = line.find("hash=");
    if (pos == std::string::npos)
      continue;
    uint32_t h = 0;
    std::istringstream(line.substr(pos + 5)) >> std::hex >> h;
    hashes.insert(h);
  }
  return hashes;
}

void
print_missing(const char* label, const std::set<uint32_t>& missing) {
  size_t shown = 0;
  for (uint32_t h : missing) {
    if (shown++ >= 20) {
      std::cout << "  ... (" << missing.size() - 20 << " more)\n";
      break;
    }
    std::cout << "  " << label << ": hash=" << std::hex << h << std::dec << "\n";
  }
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

  std::set<uint32_t> a = read_hashes(argv[1]);
  std::set<uint32_t> b = read_hashes(argv[2]);

  std::set<uint32_t> only_a, only_b;
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                       std::inserter(only_a, only_a.end()));
  std::set_difference(b.begin(), b.end(), a.begin(), a.end(),
                       std::inserter(only_b, only_b.end()));

  std::cout << argv[1] << ": " << a.size() << " distinct frame hashes\n";
  std::cout << argv[2] << ": " << b.size() << " distinct frame hashes\n";

  if (only_a.empty() && only_b.empty()) {
    std::cout << "MATCH: same set of distinct frame content (" << a.size()
              << " hashes)\n";
    return 0;
  }

  if (!only_a.empty()) {
    std::cout << only_a.size() << " hash(es) only in " << argv[1] << ":\n";
    print_missing("only_a", only_a);
  }
  if (!only_b.empty()) {
    std::cout << only_b.size() << " hash(es) only in " << argv[2] << ":\n";
    print_missing("only_b", only_b);
  }
  return 1;
}
