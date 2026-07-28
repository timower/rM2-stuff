// Empirical probe for the two worker-side playback kernels FUN_0004a140
// ("plain") and FUN_0004a234 (called "overlap-aware" at the time this probe
// was written - later corrected to "aligned", see native_display.cpp's
// native_dispatch_aligned_kernel: it fires on an 8-phase-aligned boundary
// with no live overlap dependency, the opposite of what "overlap-aware"
// suggests) - the last still-library by-address calls anywhere in the
// pipeline (AGENTS.md next step). Same
// "load the real library, call the real function directly, inspect what
// happened" approach as render_kernel_addr_map.cpp/
// dispatch_processed_regions_probe.cpp - deliberately does NOT try to
// hand-decode the NEON/jump-table body first.
//
// Two independent unknowns stand between "call it" and "know what it does":
//
//   1. Destination addressing: frameSlots[] are raw 0x165800-byte buffers in
//      the hardware panel's own packed wire format (260 x 1408 x 32bpp -
//      nothing like the plain per-pixel dataBuffer/backBuffer
//      render_update_kernel used). Unknown, and not worth hand-decoding -
//      the same binary-search trick from render_kernel_addr_map.cpp maps it
//      without needing to.
//
//   2. LUT lookup indices: swtcon_architecture.md's current text guesses the
//      kernel indexes the LUT "the same way read_lut_packed_pixel does" but
//      leaves *which* (row,col) it passes as open. Working hypothesis this
//      probe is built to test: item.transitionDataPtr (+0x44) holds, per pixel,
//      the SAME packed `(oldState<<5)|newState` transition value that
//      dispatch_processed_regions_probe.cpp already confirmed for pixelTransitions - and
//      LUTEntry::mode_width (16 or 32, see native_load_waveform) is exactly
//      the bit-width of that packed field's oldState/newState halves. That
//      strongly suggests the LUT is a classic waveform table indexed
//      [oldState][newState][phase] -> a bit_depth-wide drive code, NOT a
//      screen-position dither block like g_pGammaTable - i.e. row=oldState,
//      col=newState, not screen row/col at all. Experiment 3 below tests
//      this directly.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <dlfcn.h>
#include <new>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "native_init.h"
#include "native_update.h"

#define INSTANCE_ADDR 0x35de0
#define PLAIN_PLAYBACK_KERNEL_ADDR 0x4a140
#define ALIGNED_PLAYBACK_KERNEL_ADDR 0x4a234

// One full hardware frame slot, bytes - bitsPerPixel(0x20) * xres(0x104) *
// yres(0x580) / 8, matching FbInitParams in swtcon.cpp and native_init_lut's
// own 0x165800-byte LUT blob (same size, not a coincidence: the LUT gets
// memcpy'd straight into a frame slot by native_upload_lut_to_frame_slot).
constexpr size_t kFrameSlotBytes = 0x165800;

// Signature shared by both kernels (confirmed, swtcon_architecture.md
// §6.4): (frameSlots[8], item, frameCount, chunkIndex, chunkCount).
typedef void (*PlaybackKernelFn)(void**, WorkItem*, int, int, int);

extern void* g_pStateBufferNative;
extern void* g_pGammaTableNative;

static uintptr_t g_runtime_offset = 0;
uintptr_t
swtcon_runtime_offset() {
  return g_runtime_offset;
}

// This probe doesn't link swtcon.cpp (see CMakeLists.txt), which normally
// owns these - it never uses SWTCON_LIBIMPL library mode, so a
// permanently-false/null stub is enough to satisfy native_update.cpp's link
// requirements.
bool
swtcon_lib_impl_enabled() {
  return false;
}
void (*qsgepaper_lock)() = nullptr;
void (*qsgepaper_update)(update_data*) = nullptr;
void (*qsgepaper_unlock_post)() = nullptr;
void (*qsgepaper_wait)() = nullptr;

static uintptr_t
load_library() {
  void* handle = dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    handle = dlopen("./libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
      fprintf(stderr, "Failed to load libqsgepaper.so: %s\n", dlerror());
      exit(1);
    }
  }
  void* instance_func = dlsym(handle, "_ZN13EPFramebuffer8instanceEv");
  if (!instance_func) {
    fprintf(stderr, "Failed to find known export symbol: %s\n", dlerror());
    exit(1);
  }
  return (uintptr_t)instance_func - INSTANCE_ADDR;
}

static void
segv_handler(int sig, siginfo_t* si, void* ucv) {
  auto* uc = (ucontext_t*)ucv;
  unsigned long pc = uc->uc_mcontext.arm_pc;
  unsigned long lr = uc->uc_mcontext.arm_lr;
  fprintf(stderr, "\n*** SIGSEGV: fault_addr=%p pc=0x%lx lr=0x%lx\n", si->si_addr, pc, lr);
  fprintf(stderr, "*** ghidra: pc=0x%lx lr=0x%lx (runtime_offset=0x%lx)\n", pc - g_runtime_offset,
          lr - g_runtime_offset, (unsigned long)g_runtime_offset);
  fflush(stderr);
  signal(sig, SIG_DFL);
  raise(sig);
}

// Allocates exactly `size` usable bytes, right-aligned against a PROT_NONE
// guard page that immediately follows (byte-exact, not page-rounded: the
// returned pointer + size == the guard page's start), plus a full PROT_NONE
// guard page before the mapping. Any out-of-bounds write past the intended
// end faults immediately at the true boundary instead of silently corrupting
// an adjacent heap allocation (which is what plain malloc'd buffers did on
// the first run of this probe - a "double free or corruption" surfaced only
// later, at a subsequent free(), with no indication of where the real
// overrun happened). A gross underrun (more than one page before the start)
// is also caught; a small underrun into the same page as valid-but-unused
// slack is not - not worth chasing given writes are far more likely given
// this kernel's forward-processing NEON loops.
// Guard region width, in pages, on each side - much wider than the classic
// single-page redzone. A first run of this probe with a 1-page guard still
// silently corrupted memory ("free(): invalid pointer" at an unrelated later
// free(), no SIGSEGV) - consistent with a computed address using a stride
// far too large for this tiny synthetic buffer (e.g. a full-screen-width
// stride instead of the item's own narrow one) jumping clean over a single
// 4KB guard page into the next mmap region. 64 pages (256KB) makes that kind
// of overshoot much harder to miss.
constexpr long kGuardPages = 64;

static void*
alloc_guarded(size_t size) {
  long page = sysconf(_SC_PAGESIZE);
  size_t rounded = ((size + page - 1) / page) * page;
  if (rounded == 0)
    rounded = (size_t)page;
  size_t guard = (size_t)kGuardPages * (size_t)page;
  size_t total = rounded + 2 * guard;
  uint8_t* base = (uint8_t*)mmap(nullptr, total, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  if (mprotect(base, guard, PROT_NONE) != 0 ||
      mprotect(base + guard + rounded, guard, PROT_NONE) != 0) {
    perror("mprotect");
    exit(1);
  }
  return base + guard + (rounded - size);
}

// The read-side inverse of native_read_lut_packed_pixel (already native,
// confirmed - native_init.cpp): plants a bit_depth-wide value at (row, col,
// phase) in a LUTEntry's packed data, leaving every other packed pixel in
// the same word untouched.
static void
write_lut_packed_pixel(const LUTEntry* lut, int row, int col, int phase, unsigned value) {
  int pixels_per_word = 16 / lut->bit_depth;
  int word_idx = phase / pixels_per_word;
  int bit_off = phase - pixels_per_word * word_idx;
  int mw = lut->mode_width;
  int data_idx = mw * row + col + mw * mw * word_idx + word_idx;
  uint16_t* dest = (uint16_t*)lut->data;
  unsigned mask = ((1u << lut->bit_depth) - 1);
  uint16_t word = dest[data_idx];
  word = (uint16_t)(word & ~(mask << (lut->bit_depth * bit_off)));
  word = (uint16_t)(word | ((value & mask) << (lut->bit_depth * bit_off)));
  dest[data_idx] = word;
}

// Number of 16-bit words a LUTEntry needs to hold mode_width x mode_width
// packed pixels for phases 0..lutWidth-1, given native_read_lut_packed_pixel's
// addressing (data_idx = mw*row + col + mw*mw*word_idx + word_idx).
static size_t
lut_data_words(int mode_width, int bit_depth, int lut_width) {
  int pixels_per_word = 16 / bit_depth;
  int max_word_idx = (lut_width - 1) / pixels_per_word;
  return (size_t)(mode_width * mode_width + 1) * (max_word_idx + 1);
}

struct TestItem {
  WorkItemNode* node;
  RegionRows transitionsRr;
  uint16_t* transitionBuf;  // one u16 per pixel, column-major: stride*(col-x0)+(row-y0)
  LUTEntry lut;
};

// Builds a WorkItem covering [y0,y1]x[x0,x1] (inclusive), with a state
// buffer (filled to `fill_transition`, packed as (old<<5)|new like
// dispatch_processed_regions's pixelTransitions) and a synthetic LUT of the given shape.
// No rebasing: transitionsRr's origin equals the item's rect origin, so
// transitionDataPtr == the buffer base (offset 0) - keeps the first experiments
// free of a second unverified variable.
static void
build_item(TestItem* ti, int y0, int x0, int y1, int x1, uint16_t fill_transition, int phase,
           int mode_width, int bit_depth, int lut_width) {
  // Guard-paged, not malloc'd: two runs of this probe already silently
  // corrupted heap state somewhere after a "clean" kernel return, even with
  // every OTHER buffer (state/LUT/frameSlots) guard-paged - the remaining
  // candidate is a write past `item` itself (WorkItem is 0x5c bytes,
  // embedded at node+8, so node's malloc'd 100-byte chunk ends exactly at
  // item's last byte with zero slack - an off-by-a-little write past
  // pixelMode at +0x58 would land directly in the next heap chunk's header,
  // matching the "double free or corruption"/"invalid pointer" glibc errors
  // seen so far exactly). Guarding it turns that into an immediate,
  // localizing SIGSEGV instead.
  // Placement-new (value-init), not memset: WorkItem now has non-trivial
  // shared_ptr members that must actually be constructed, not just
  // zero-filled, before being assigned into below.
  ti->node = new (alloc_guarded(sizeof(WorkItemNode))) WorkItemNode();
  WorkItem* item = &ti->node->item;

  int rows = y1 - y0 + 1;
  int cols = x1 - x0 + 1;
  int strideRows = (rows + 15) & ~15;  // same rounding RegionRows uses elsewhere
  size_t transitionBytes = (size_t)strideRows * cols * sizeof(uint16_t);
  ti->transitionBuf = (uint16_t*)alloc_guarded(transitionBytes);
  for (int i = 0; i < strideRows * cols; i++)
    ti->transitionBuf[i] = fill_transition;

  ti->transitionsRr.dataPtr = (uint8_t*)ti->transitionBuf;
  ti->transitionsRr.y0 = y0;
  ti->transitionsRr.x0 = x0;
  ti->transitionsRr.y1 = y1;
  ti->transitionsRr.x1 = x1;
  ti->transitionsRr.stride = strideRows;
  ti->transitionsRr.size = strideRows * cols * (int)sizeof(uint16_t);

  size_t lut_words = lut_data_words(mode_width, bit_depth, lut_width);
  ti->lut.size_kb = lut_width;  // matches native_load_waveform: size_kb doubles as phase count
  ti->lut.mode_width = mode_width;
  ti->lut.temperature = 0;
  ti->lut.bit_depth = bit_depth;
  size_t lutBytes = lut_words * sizeof(uint16_t);
  ti->lut.data = alloc_guarded(lutBytes);
  memset(ti->lut.data, 0, lutBytes);

  item->pixelDataPtr = 0;
  item->rectY0 = y0;
  item->rectX0 = x0;
  item->rectY1 = y1;
  item->rectX1 = x1;
  item->seqId = 0;
  item->frameCursor = 0;
  item->frameAnchor = 0;
  item->phase = (int16_t)phase;
  item->lutWidthMinus1 = (int16_t)(lut_width - 1);
  item->lut = non_owning_sp(&ti->lut);
  item->mode = 0;
  item->temperature = 0;
  item->pixelTransitions = non_owning_sp(&ti->transitionsRr);
  item->transitionDataPtr = ti->transitionBuf;  // no rebase: transitionsRr origin == rect origin
  item->sync = 0;
  item->fastDraw = 0;
  item->pixelMode = 0;
}

static void
free_item(TestItem* ti) {
  // node/transitionBuf/lut.data are all guard-paged mmap regions now, not
  // malloc'd - leaking them for the lifetime of this short-lived probe
  // process is fine (a handful of experiments, not a long-running server).
  //
  // THE ACTUAL BUG this probe kept hitting: LUTEntry (native_init.h) has a
  // destructor - `~LUTEntry() { if (data) free(data); }` - and TestItem
  // embeds `LUTEntry lut` BY VALUE, so that destructor runs automatically
  // when `ti` goes out of scope at the end of each experiment block, calling
  // libc free() on our guard-paged mmap pointer. That's an invalid free
  // completely unrelated to anything the kernel itself does - it's what
  // produced "free(): invalid pointer"/"double free or corruption" on every
  // run so far, always right after "returned without crashing" (the next
  // heap operation after the kernel call, which is this destructor). Null it
  // out here so the destructor's `if (data)` guard skips the free.
  ti->lut.data = nullptr;
}

// Scans a frame slot for nonzero bytes, printing up to `max_print` (offset,
// value) pairs. Returns the total nonzero count.
static size_t
scan_nonzero(const char* label, const uint8_t* buf, size_t n, int max_print) {
  size_t count = 0;
  for (size_t i = 0; i < n; i++) {
    if (buf[i] != 0) {
      if ((int)count < max_print)
        printf("    %s nonzero @0x%zx = 0x%02x\n", label, i, buf[i]);
      count++;
    }
  }
  return count;
}

int
main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = segv_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, nullptr);

  g_runtime_offset = load_library();
  printf("Loaded library, runtime_offset=0x%lx\n", (unsigned long)g_runtime_offset);

  // The first run of this probe (item.transitionDataPtr/lut only, no global
  // bridging) silently corrupted memory even with a 256KB guard on every
  // buffer - i.e. not a moderate stride overshoot off one of our own
  // buffers. dispatch_processed_regions_probe.cpp hit an analogous issue:
  // its kernels (FUN_0004f8f0/FUN_0004e680) read/write the LIBRARY's own
  // g_pStateBuffer global directly, not a parameter - left unbridged, it's
  // whatever a freshly dlopen'd library defaults to (very likely null).
  // Bridge the same globals defensively here in case FUN_0004a140 turns out
  // to read per-pixel state from g_pStateBuffer (absolute screen row/col)
  // rather than item.transitionDataPtr as swtcon_architecture.md's "[derived]"
  // (i.e. unconfirmed) description currently guesses.
  if (native_init_statebuffer() != 0) {
    fprintf(stderr, "native_init_statebuffer failed\n");
    return 1;
  }
  statebuffer_globals()->pStatebuffer = g_pStateBufferNative;
  statebuffer_globals()->pGammaTable = g_pGammaTableNative;
  printf("native_init_statebuffer done, bridged pStatebuffer/pGammaTable\n");

  auto plain_kernel = (PlaybackKernelFn)(g_runtime_offset + PLAIN_PLAYBACK_KERNEL_ADDR);
  auto aligned_kernel = (PlaybackKernelFn)(g_runtime_offset + ALIGNED_PLAYBACK_KERNEL_ADDR);

  // The frameSlots[8] pointer array itself is guard-paged too, not just the
  // buffers it points to - if the kernel ever indexes past slot 7 (a wrong
  // assumption about the calling convention, or genuinely reads beyond 8),
  // that must fault immediately rather than silently clobbering whatever
  // else was on the stack (which is exactly what happened before this: the
  // kernel call "returned without crashing" but corrupted something that
  // only surfaced as "free(): invalid pointer" afterwards).
  void** frame_slots = (void**)alloc_guarded(8 * sizeof(void*));
  for (int i = 0; i < 8; i++)
    frame_slots[i] = alloc_guarded(kFrameSlotBytes);

  auto clear_slots = [&] {
    for (int i = 0; i < 8; i++)
      memset(frame_slots[i], 0, kFrameSlotBytes);
  };

  // --- Experiment 1: sanity. Ghidra decompile of the frameCount==1 case
  // (via mcp__ghidra__decompile_function on libqsgepaper_3.23.0.54.so)
  // confirmed: Y is block-quantized by 8 (local_54 = ((rectY1-rectY0)+1)>>3;
  // the whole write loop is skipped if that's 0), X is not. A 1x1 rect
  // (the very first thing this probe tried) silently skips the write loop
  // entirely for exactly that reason - not a bug in the call, just a
  // degenerate rect. Single column, 8-row span is the minimal rect that
  // actually exercises the write path.
  // Transition (0,0), mode_width=32/bit_depth=2/lutWidth=8 (typical
  // GC16-shaped LUT), LUT cell (row=0,col=0,phase=0) set to the max 2-bit
  // value (3) - the decompile also confirmed the LUT index formula matches
  // native_read_lut_packed_pixel exactly, using the raw transitionDataPtr u16
  // value directly as `mw*row+col` (not a >>5/&0x1f split - numerically
  // identical only because mode_width=32 here).
  {
    printf("\n=== Experiment 1: sanity, frameCount=1, 8-row x 1-col rect ===\n");
    const int Y0 = 0, Y1 = 7, X0 = 500;
    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X0, /*fill_transition=*/0, /*phase=*/0, /*mode_width=*/32,
               /*bit_depth=*/2, /*lut_width=*/8);
    write_lut_packed_pixel(&ti.lut, 0, 0, 0, 3);
    clear_slots();

    printf("  node=%p transitionBuf=%p lut.data=%p frame_slots(array)=%p frame_slots[0..7]=",
           (void*)ti.node, (void*)ti.transitionBuf, ti.lut.data, (void*)frame_slots);
    for (int i = 0; i < 8; i++)
      printf("%p ", frame_slots[i]);
    printf("\n");
    printf("  calling FUN_0004a140...\n");
    plain_kernel(frame_slots, &ti.node->item, /*frameCount=*/1, /*chunkIndex=*/0, /*chunkCount=*/1);
    printf("  returned without crashing\n");

    for (int i = 0; i < 8; i++) {
      char label[32];
      snprintf(label, sizeof(label), "slot%d", i);
      size_t n = scan_nonzero(label, (const uint8_t*)frame_slots[i], kFrameSlotBytes, 20);
      if (n)
        printf("  slot%d: %zu nonzero bytes\n", i, n);
    }
    free_item(&ti);
  }

  // --- Experiment 2: does the kernel touch a slot at all when the LUT
  // value for this pixel's transition is 0 (no drive)? Sanity check that
  // "all zero" isn't itself already a marked state some other way.
  {
    printf("\n=== Experiment 2: all-zero LUT, expect no output ===\n");
    const int Y0 = 0, Y1 = 7, X0 = 500;
    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X0, /*fill_transition=*/0, /*phase=*/0, /*mode_width=*/32,
               /*bit_depth=*/2, /*lut_width=*/8);
    // LUT left all-zero (calloc'd) - no write_lut_packed_pixel call.
    clear_slots();

    printf("  calling FUN_0004a140...\n");
    plain_kernel(frame_slots, &ti.node->item, /*frameCount=*/1, /*chunkIndex=*/0, /*chunkCount=*/1);
    printf("  returned without crashing\n");

    size_t total = 0;
    for (int i = 0; i < 8; i++)
      total += scan_nonzero("slot", (const uint8_t*)frame_slots[i], kFrameSlotBytes, 5);
    printf("  total nonzero bytes across all 8 slots: %zu\n", total);
    free_item(&ti);
  }

  // --- Experiment 3: LUT-index hypothesis test. Two pixels with DIFFERENT
  // transitions (oldState,newState) = (1,2) and (5,7); mark ONLY LUT cell
  // (row=1,col=2,phase=0) with value 3, leave (row=5,col=7,phase=0) and
  // everything else at 0. If the kernel indexes the LUT by
  // (oldState,newState) as hypothesized, only the pixel carrying transition
  // (1,2) should produce nonzero output; if it instead indexes by screen
  // row/col (dither-block style, mod mode_width), the result will depend on
  // the pixels' screen coordinates instead and this experiment's pixel
  // separation (adjacent columns) plus the marked cell choice should show a
  // mismatch against the transition-based prediction.
  {
    printf("\n=== Experiment 3: LUT index hypothesis (oldState,newState) vs (row,col) ===\n");
    const int Y0 = 0, Y1 = 7, X0 = 500, X1 = 501;  // two adjacent columns, 8-row span
    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X1, /*fill_transition=*/0, /*phase=*/0, /*mode_width=*/32,
               /*bit_depth=*/2, /*lut_width=*/8);
    // pixel at (Y0,X0): transition (oldState=1,newState=2) -> packed (1<<5)|2 = 0x22
    // pixel at (Y0,X1): transition (oldState=5,newState=7) -> packed (5<<5)|7 = 0xa7
    auto cell = [&](int col, int row) -> uint16_t& {
      return ti.transitionBuf[(size_t)ti.transitionsRr.stride * (col - ti.transitionsRr.x0) + (row - ti.transitionsRr.y0)];
    };
    cell(X0, Y0) = (1 << 5) | 2;
    cell(X1, Y0) = (5 << 5) | 7;
    write_lut_packed_pixel(&ti.lut, /*row=*/1, /*col=*/2, /*phase=*/0, 3);
    clear_slots();

    printf("  calling FUN_0004a140...\n");
    plain_kernel(frame_slots, &ti.node->item, /*frameCount=*/1, /*chunkIndex=*/0, /*chunkCount=*/1);
    printf("  returned without crashing\n");

    for (int i = 0; i < 8; i++) {
      size_t n = scan_nonzero("slot", (const uint8_t*)frame_slots[i], kFrameSlotBytes, 20);
      if (n)
        printf("  slot%d: %zu nonzero bytes (pixel X0=%d transition=(1,2)=marked, X1=%d "
               "transition=(5,7)=unmarked)\n",
               i, n, X0, X1);
    }
    free_item(&ti);
  }

  // --- Experiment 4: per-row bit-position mapping. Experiment 3's single
  // active row (absolute row 0, the first row of the 8-row group) landed in
  // the HIGH bits of the HIGH byte (0x7fbd9, bits 7:6) - i.e. bits[15:14] of
  // the little-endian 16-bit pair, not bits[1:0] as a naive lane0->shift0
  // reading of the NEON sequence would suggest. That's consistent with a
  // reversed row order (row 0 -> top bits, row 7 -> bottom bits), matching
  // the 180-degree-rotation convention already confirmed elsewhere in this
  // codebase (render_update_kernel's addressing). Isolate each of the 8 rows
  // one at a time (fresh item per row, all other rows left at an
  // always-zero-drive transition) to map every row unambiguously.
  {
    printf("\n=== Experiment 4: per-row bit-position mapping (one row active at a time) ===\n");
    const int X0 = 500;
    for (int r = 0; r < 8; r++) {
      TestItem ti;
      build_item(&ti, 0, X0, 7, X0, /*fill_transition=*/0, /*phase=*/0, /*mode_width=*/32,
                 /*bit_depth=*/2, /*lut_width=*/8);
      // Row r gets transition (oldState=1,newState=0) = 32; every other row
      // keeps the default fill transition 0, whose LUT cell is left
      // unmarked (0) - only row r should produce nonzero output.
      auto cell = [&](int col, int row) -> uint16_t& {
        return ti.transitionBuf[(size_t)ti.transitionsRr.stride * (col - ti.transitionsRr.x0) + (row - ti.transitionsRr.y0)];
      };
      cell(X0, r) = (1 << 5) | 0;
      write_lut_packed_pixel(&ti.lut, /*row=*/1, /*col=*/0, /*phase=*/0, 3);
      clear_slots();

      plain_kernel(frame_slots, &ti.node->item, /*frameCount=*/1, /*chunkIndex=*/0, /*chunkCount=*/1);

      bool found = false;
      for (int i = 0; i < 8 && !found; i++) {
        const uint8_t* buf = (const uint8_t*)frame_slots[i];
        for (size_t off = 0; off < kFrameSlotBytes; off++) {
          if (buf[off] != 0) {
            printf("  row=%d -> slot%d byte@0x%zx = 0x%02x\n", r, i, off, buf[off]);
            found = true;
            break;
          }
        }
      }
      if (!found)
        printf("  row=%d -> NO nonzero output found\n", r);
      free_item(&ti);
    }
  }

  // --- Experiment 5: multi-group rect (16 rows = 2 groups of 8, single
  // column). Ghidra decompile trace (frameCount==1 case) predicts the two
  // groups land in CONTIGUOUS 4-byte slots at the same column's base address
  // (group g at base+g*4), each holding its own 8-row packed word with the
  // same reversed row->bit mapping as experiment 4. All 16 rows given the
  // same always-drive-3 transition, so both group words should read back as
  // 0xffff if the prediction holds.
  {
    printf("\n=== Experiment 5: multi-group rect (16 rows, predicts group1 at base+4) ===\n");
    const int X0 = 500;
    TestItem ti;
    build_item(&ti, 0, X0, 15, X0, /*fill_transition=*/(1 << 5) | 0, /*phase=*/0, /*mode_width=*/32,
               /*bit_depth=*/2, /*lut_width=*/8);
    write_lut_packed_pixel(&ti.lut, /*row=*/1, /*col=*/0, /*phase=*/0, 3);
    clear_slots();

    plain_kernel(frame_slots, &ti.node->item, /*frameCount=*/1, /*chunkIndex=*/0, /*chunkCount=*/1);

    size_t n = scan_nonzero("slot0", (const uint8_t*)frame_slots[0], kFrameSlotBytes, 20);
    printf("  slot0: %zu nonzero bytes total (predicted: 4, at 0x7fbd8-0x7fbd9 and 0x7fbdc-0x7fbdd)\n",
           n);
    free_item(&ti);
  }

  // --- Experiment 6: frameCount>1 generalization. Ghidra decompile of case
  // 2 shows it does NOT run case 1's LUT lookup twice at phase and phase+1 -
  // it computes an 8-entry shared buffer ONCE per column/row-group (one
  // entry per possible sub-phase 0-7 within the current LUT word, i.e. all 8
  // bit-offsets extractable from the single fetched packed-LUT word), then
  // frameSlots[k] (k=0..frameCount-1) just reads out shared-buffer entry
  // (phase&7)+k - never a second independent LUT fetch. This works because
  // native_advance_work_item_frames's frame_count is always chosen so
  // phase&7 + frameCount never crosses an 8-phase LUT-word boundary, i.e.
  // frameCount<=8-phase%8. Verify: one active row (row=0) with a LUT entry
  // whose phase=0 slot (bit-offset 0) = 1 and phase=1 slot (bit-offset 1,
  // SAME LUT word since word_idx=phase/8=0 for both) = 2 - two DIFFERENT
  // values baked into the one word case 1 alone could never distinguish.
  // frameCount=2 at phase=0 should split them cleanly across frameSlots[0]
  // (value 1, at bits[15:14] since row=0) and frameSlots[1] (value 2, same
  // bit position, same destination address - independent target buffers).
  {
    printf("\n=== Experiment 6: frameCount=2 shared-buffer hypothesis ===\n");
    const int X0 = 500;
    TestItem ti;
    build_item(&ti, 0, X0, 7, X0, /*fill_transition=*/0, /*phase=*/0, /*mode_width=*/32,
               /*bit_depth=*/2, /*lut_width=*/16);
    auto cell = [&](int col, int row) -> uint16_t& {
      return ti.transitionBuf[(size_t)ti.transitionsRr.stride * (col - ti.transitionsRr.x0) + (row - ti.transitionsRr.y0)];
    };
    cell(X0, 0) = (1 << 5) | 0;  // row 0 only: transition (oldState=1,newState=0) = 32
    write_lut_packed_pixel(&ti.lut, /*row=*/1, /*col=*/0, /*phase=*/0, 1);  // bit-offset 0
    write_lut_packed_pixel(&ti.lut, /*row=*/1, /*col=*/0, /*phase=*/1, 2);  // bit-offset 1, same word
    clear_slots();

    plain_kernel(frame_slots, &ti.node->item, /*frameCount=*/2, /*chunkIndex=*/0, /*chunkCount=*/1);

    for (int i = 0; i < 8; i++) {
      size_t n = scan_nonzero("slot", (const uint8_t*)frame_slots[i], kFrameSlotBytes, 10);
      if (n)
        printf("  slot%d: %zu nonzero bytes\n", i, n);
    }
    printf("  predicted: slot0 byte@0x7fbd9=0x40 (value 1<<6), slot1 byte@0x7fbd9=0x80 (value 2<<6)\n");
    free_item(&ti);
  }

  // --- Experiment 7: are FUN_0004a234's cases 4-8 computationally
  // IDENTICAL to FUN_0004a140, not just similarly-shaped? A Ghidra decompile
  // pass on case 8 (94/117 real aligned-kernel calls in one ab-test run,
  // vs. 23 for case 5 and 0 for cases 1-4/6-7 - see AGENTS.md) found it
  // touches the exact same WorkItem fields (0xc/0x10/0x14/0x18/0x28/0x2c/
  // 0x3c/0x44), the exact same LUT-index formula, the exact same
  // destination address formula, and the exact same row->bit packing order
  // as FUN_0004a140 - no new field, no global, no intList access, just a
  // different NEON unrolling strategy (justified by phase&7==0 always
  // holding when the aligned kernel is selected, letting it extract all 8
  // sub-phases from one lane-shifted vector instead of doing the (phase&7)+k
  // slice FUN_0004a140 does). If true, native_playback_kernel_plain should
  // already produce byte-identical output to the real FUN_0004a234 - this
  // decisive test calls BOTH kernels on the IDENTICAL WorkItem/state/LUT
  // (into separate frame-slot buffers) and diffs every byte, rather than
  // trusting the decompile reading alone. Uses phase=0 (8-aligned, required
  // for the aligned-kernel path to ever be selected in real usage) and
  // frameCount=8 so every one of the 8 packed sub-phases gets exercised in
  // one call - the richest single-call test available. A varied
  // pseudo-random transition/LUT fill (not a uniform value) avoids a
  // trivial coincidental match.
  {
    printf("\n=== Experiment 7: FUN_0004a234 vs FUN_0004a140 identical-input diff ===\n");
    // X0..X1 spans 128 columns (not just 4) - a Ghidra decompile of
    // FUN_0004a3f8 (case 1's delegate) showed it does alignment-based
    // column-chunking and an 8x4 transpose/prefetch bulk loop, structurally
    // very different from FUN_0004a140's simple one-column-at-a-time loop -
    // a narrow rect risks only ever exercising the "remainder" tail path,
    // never the bulk vectorized one, which would make an "identical output"
    // finding much weaker evidence than it looks.
    const int Y0 = 0, Y1 = 15, X0 = 500, X1 = 627;
    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X1, /*fill_transition=*/0, /*phase=*/0, /*mode_width=*/32,
               /*bit_depth=*/2, /*lut_width=*/8);
    auto cell = [&](int col, int row) -> uint16_t& {
      return ti.transitionBuf[(size_t)ti.transitionsRr.stride * (col - ti.transitionsRr.x0) + (row - ti.transitionsRr.y0)];
    };
    // Deterministic pseudo-random transition per pixel (mw=32, so oldState/
    // newState each 0-31 - keep both in range so the transition value stays
    // a valid mw*row+col index).
    unsigned seed = 12345;
    auto next_rand = [&]() {
      seed = seed * 1103515245u + 12345u;
      return (seed >> 16) & 0x7fff;
    };
    for (int col = X0; col <= X1; col++)
      for (int row = Y0; row <= Y1; row++)
        cell(col, row) = (uint16_t)((next_rand() % 32) * 32 + (next_rand() % 32));
    // Fill every (oldState,newState) LUT cell for phase 0-7 (word_idx=0,
    // exactly matching frameCount=8's full sub-phase range) with a varied
    // pseudo-random 2-bit value.
    for (int r = 0; r < 32; r++)
      for (int c = 0; c < 32; c++)
        for (int p = 0; p < 8; p++)
          write_lut_packed_pixel(&ti.lut, r, c, p, next_rand() & 3);

    void** frame_slots_b = (void**)alloc_guarded(8 * sizeof(void*));
    for (int i = 0; i < 8; i++)
      frame_slots_b[i] = alloc_guarded(kFrameSlotBytes);
    auto clear_slots_b = [&] {
      for (int i = 0; i < 8; i++)
        memset(frame_slots_b[i], 0, kFrameSlotBytes);
    };

    for (int frame_count : { 8, 5, 4, 6, 7, 1, 2, 3 }) {
      clear_slots();
      plain_kernel(frame_slots, &ti.node->item, frame_count, /*chunkIndex=*/0, /*chunkCount=*/1);
      clear_slots_b();
      aligned_kernel(frame_slots_b, &ti.node->item, frame_count, /*chunkIndex=*/0, /*chunkCount=*/1);

      size_t mismatches = 0;
      for (int i = 0; i < 8 && mismatches < 20; i++) {
        const uint8_t* a = (const uint8_t*)frame_slots[i];
        const uint8_t* b = (const uint8_t*)frame_slots_b[i];
        for (size_t off = 0; off < kFrameSlotBytes && mismatches < 20; off++) {
          if (a[off] != b[off]) {
            printf("  frameCount=%d MISMATCH slot%d @0x%zx: plain=0x%02x aligned=0x%02x\n",
                   frame_count, i, off, a[off], b[off]);
            mismatches++;
          }
        }
      }
      if (mismatches == 0)
        printf("  frameCount=%d: IDENTICAL byte-for-byte\n", frame_count);
      else
        printf("  frameCount=%d: %zu+ mismatches found - NOT identical\n", frame_count, mismatches);
    }

    // Same check again but chunkCount=2 (chunkIndex 0 then 1, sequentially -
    // ranges are disjoint so accumulating into the same buffers is fine):
    // FUN_0004a3f8 computes its own chunk-boundary split independently from
    // FUN_0004a140/FUN_0004a234's shared boundary formula, so this exercises
    // a genuinely different code path in the delegate, not just a wider
    // single-chunk call.
    for (int frame_count : { 8, 5, 1, 2, 3 }) {
      clear_slots();
      plain_kernel(frame_slots, &ti.node->item, frame_count, 0, 2);
      plain_kernel(frame_slots, &ti.node->item, frame_count, 1, 2);
      clear_slots_b();
      aligned_kernel(frame_slots_b, &ti.node->item, frame_count, 0, 2);
      aligned_kernel(frame_slots_b, &ti.node->item, frame_count, 1, 2);

      size_t mismatches = 0;
      for (int i = 0; i < 8 && mismatches < 20; i++) {
        const uint8_t* a = (const uint8_t*)frame_slots[i];
        const uint8_t* b = (const uint8_t*)frame_slots_b[i];
        for (size_t off = 0; off < kFrameSlotBytes && mismatches < 20; off++) {
          if (a[off] != b[off]) {
            printf("  chunkCount=2 frameCount=%d MISMATCH slot%d @0x%zx: plain=0x%02x aligned=0x%02x\n",
                   frame_count, i, off, a[off], b[off]);
            mismatches++;
          }
        }
      }
      if (mismatches == 0)
        printf("  chunkCount=2 frameCount=%d: IDENTICAL byte-for-byte\n", frame_count);
      else
        printf("  chunkCount=2 frameCount=%d: %zu+ mismatches found - NOT identical\n", frame_count,
               mismatches);
    }
    free_item(&ti);
  }

  // frame_slots are guard-paged mmap regions, not malloc'd - leaked
  // intentionally, same as build_item's buffers (see free_item).
  return 0;
}
