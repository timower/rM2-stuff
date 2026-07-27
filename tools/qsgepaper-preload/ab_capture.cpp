#include "ab_capture.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "native_init.h"
#include "native_update.h"

extern void* g_pStateBufferNative;
extern int g_nFbSizeXNative;

namespace {

constexpr size_t kStateBufferBytes = 0x503580;

std::mutex g_mtx;
std::vector<std::string> g_lines;

const char*
capture_path() {
  static const char* p = getenv("SWTCON_AB_CAPTURE");
  return p;
}

uint32_t
fnv1a(const void* data, size_t len) {
  uint32_t h = 2166136261u;
  const uint8_t* p = (const uint8_t*)data;
  for (size_t i = 0; i < len; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

void
emit(const std::string& line) {
  std::lock_guard<std::mutex> lock(g_mtx);
  g_lines.push_back(line);
}

std::string
fmt(const char* f, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return std::string(buf);
}

} // namespace

bool
ab_capture_enabled() {
  static const bool on = (capture_path() != nullptr);
  return on;
}

void
ab_capture_dispatch(ListHead* sub_list) {
  if (!ab_capture_enabled())
    return;
  auto* node = (WorkItemNode*)sub_list->next;
  while ((void*)node != (void*)sub_list) {
    const WorkItem& it = node->item;
    auto* rr = (const RegionRows*)it.sp3.ptr;

    uint32_t sp3hash = 0;
    long sdp_off = -1; // u16 offset of stateDataPtr from sp3.dataPtr
    int sp3_y0 = -1, sp3_y1 = -1, sp3_x0 = -1, sp3_x1 = -1, sp3_stride = -1;
    if (rr && rr->dataPtr) {
      sp3_y0 = rr->y0;
      sp3_y1 = rr->y1;
      sp3_x0 = rr->x0;
      sp3_x1 = rr->x1;
      sp3_stride = rr->stride;
      int cols = rr->x1 - rr->x0 + 1;
      size_t len = (size_t)rr->stride * (cols > 0 ? cols : 0) * sizeof(uint16_t);
      sp3hash = fnv1a(rr->dataPtr, len);
      if (it.stateDataPtr)
        sdp_off = ((const uint16_t*)it.stateDataPtr) - (const uint16_t*)rr->dataPtr;
    }

    // Key each record by seqId so sorting keeps the same item's records
    // together and the comparison is order-independent.
    emit(fmt("DISP seq=%d rect=%d,%d,%d,%d sp3=%d,%d,%d,%d,%d sdpoff=%ld "
             "sp3hash=%08x sync=%d fr=%d mode=%d pm=%d phase=%d lw=%d",
             it.seqId, it.rectY0, it.rectY1, it.rectX0, it.rectX1, sp3_y0, sp3_y1, sp3_x0,
             sp3_x1, sp3_stride, sdp_off, sp3hash, (int)it.sync, (int)it.fullRefresh,
             (int)it.mode, it.pixelMode, (int)it.phase, (int)it.lutWidthMinus1));

    node = node->next;
  }
}

void
ab_capture_playback(const WorkItem* item, int start_frame, int end_frame) {
  if (!ab_capture_enabled())
    return;
  for (int f = start_frame; f != end_frame; f++) {
    const void* slot = native_frame_buffer_addr(f % 16);
    uint32_t h = fnv1a(slot, (size_t)g_nFbSizeXNative);
    emit(fmt("PLAY seq=%d f=%d slot=%d hash=%08x", item->seqId, f, f % 16, h));
  }
}

void
ab_capture_kernel(const WorkItem* item, const char* kernel, int frame_count, int chunk_count) {
  if (!ab_capture_enabled())
    return;
  emit(fmt("KERN seq=%d kernel=%s frames=%d chunks=%d phase=%d lw=%d rect=%d,%d,%d,%d",
           item->seqId, kernel, frame_count, chunk_count, (int)item->phase,
           (int)item->lutWidthMinus1, item->rectY0, item->rectY1, item->rectX0, item->rectX1));
}

void
ab_capture_settled(const char* tag) {
  if (!ab_capture_enabled())
    return;
  // Only the state buffer is a stable settled sentinel: nothing writes it after
  // a batch settles. The 17-slot framebuffer ring is deliberately NOT hashed
  // here - the live worker thread keeps panning/rewriting it, so a post-hoc
  // snapshot is racy (non-deterministic even native-vs-native). The transient
  // drive data is captured race-free at write time by ab_capture_playback
  // instead; that, not a settled fb snapshot, is the reliable "what drove the
  // panel" signal.
  uint32_t statehash = g_pStateBufferNative ? fnv1a(g_pStateBufferNative, kStateBufferBytes) : 0;
  emit(fmt("STATE tag=%s statehash=%08x", tag, statehash));
}

void
ab_capture_flush() {
  if (!ab_capture_enabled())
    return;
  const char* path = capture_path();
  std::lock_guard<std::mutex> lock(g_mtx);
  std::sort(g_lines.begin(), g_lines.end());
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    fprintf(stderr, "ab_capture_flush: cannot write %s\n", path);
    return;
  }
  for (const auto& l : g_lines)
    out << l << "\n";
  fprintf(stderr, "ab_capture_flush: wrote %zu records to %s\n", g_lines.size(), path);
}

int
ab_capture_compare(const char* path_a, const char* path_b) {
  auto load = [](const char* p, std::vector<std::string>& out) -> bool {
    std::ifstream in(p);
    if (!in)
      return false;
    std::string line;
    while (std::getline(in, line))
      out.push_back(line);
    std::sort(out.begin(), out.end()); // defensive: files are already sorted
    return true;
  };

  std::vector<std::string> a, b;
  if (!load(path_a, a)) {
    fprintf(stderr, "compare: cannot read %s\n", path_a);
    return 2;
  }
  if (!load(path_b, b)) {
    fprintf(stderr, "compare: cannot read %s\n", path_b);
    return 2;
  }

  // Sorted set difference both ways: lines only in A, lines only in B.
  std::vector<std::string> only_a, only_b;
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(only_a));
  std::set_difference(b.begin(), b.end(), a.begin(), a.end(), std::back_inserter(only_b));

  if (only_a.empty() && only_b.empty()) {
    printf("A/B MATCH: %zu records identical (%s vs %s)\n", a.size(), path_a, path_b);
    return 0;
  }

  printf("A/B MISMATCH: %zu record(s) only in A, %zu only in B\n", only_a.size(), only_b.size());
  // Group the diff by record kind + key so the first divergence is obvious.
  auto dump = [](const char* label, const std::vector<std::string>& v) {
    size_t n = std::min<size_t>(v.size(), 20);
    for (size_t i = 0; i < n; i++)
      printf("  %s %s\n", label, v[i].c_str());
    if (v.size() > n)
      printf("  %s ... (%zu more)\n", label, v.size() - n);
  };
  dump("A-only:", only_a);
  dump("B-only:", only_b);
  return 1;
}
