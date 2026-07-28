// Empirical probe for dispatch_processed_regions (0x50660), the last
// still-library call anywhere in the native swtcon port (AGENTS.md next
// step). Decompilation shows a dense vector-of-vectors bookkeeping
// structure that's easy to mis-read by hand (see swtcon_architecture.md
// §6.2 step 4) - in particular, whether an item with literally zero changed
// pixels ends up degenerate (destroyed) or survives with its full original
// rect is a plain seed/narrow direction detail that's much safer to settle
// by calling the real function than by continuing to eyeball ARM/NEON
// pseudocode. Same "load the real library, call the real function by
// address, inspect what changed" approach as render_kernel_addr_map.cpp.
//
// This does NOT need dispatch_update_regions/render_update_kernel at all:
// dispatch_processed_regions's commit kernels (FUN_0004f8f0/FUN_0004e680)
// read their "new" pixel value straight out of item.pixelDataPtr/item.regionRows (a
// plain byte buffer we control directly) and compare it against
// g_pStateBuffer (the library's own global, bridged to our own allocation
// via statebuffer_globals(), same trick render_kernel_verify.cpp uses for
// the gamma table).
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <new>
#include <ucontext.h>

#include "init.h"
#include "update.h"

#define INSTANCE_ADDR 0x35de0
#define DISPATCH_PROCESSED_REGIONS_ADDR 0x50660

typedef bool (*DispatchProcessedRegionsFn)(ListHead*);

extern void* g_pStateBufferNative;

static uintptr_t g_runtime_offset = 0;
uintptr_t
swtcon_runtime_offset() {
  return g_runtime_offset;
}

// This probe doesn't link swtcon.cpp (see CMakeLists.txt), which normally
// owns these - it never uses SWTCON_LIBIMPL library mode, so a
// permanently-false/null stub is enough to satisfy update.cpp's link
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
  void* handle =
    dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
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

const int SCREEN_W = 1404, SCREEN_H = 1872;

// g_pStateBufferNative is column-major over the FULL screen, stride
// SCREEN_H, 2 bytes/pixel (matches FUN_0004f8f0's own
// `g_pStateBuffer + (col*0x750 + row) * 2` addressing - 0x750 == 1872).
static uint16_t*
state_cell(int abs_col, int abs_row) {
  uint16_t* base = (uint16_t*)g_pStateBufferNative;
  return &base[(size_t)abs_col * SCREEN_H + abs_row];
}

// node is heap-allocated (matching the library's own `operator_new(100)`
// convention for WorkItemNode) rather than embedded/stack, because a
// degenerate item gets `operator_delete`d by dispatch_processed_regions
// itself - freeing a stack address corrupts the heap instead of just
// telling us the item was destroyed.
struct TestItem {
  WorkItemNode* node;
  RegionRows rr;
  uint8_t* pixelBuf; // regionRows payload: byte per pixel, column-major, stride rows_rounded16
};

// Builds one non-degenerate WorkItem covering [y0,y1]x[x0,x1] (inclusive),
// with regionRows/pixelDataPtr pointing at a freshly allocated buffer filled with
// `pixelFill`, and sync/pixelMode as given. Leaves regionRows non-owning and
// lut/pixelTransitions empty (destroying an empty shared_ptr is a safe no-op, exercised
// if this item ends up destroyed).
static void
build_item(TestItem* ti, int y0, int x0, int y1, int x1, uint8_t sync, uint8_t pixelFill) {
  // node is malloc'd (matching the library's own operator_new(100)
  // convention) and placement-new-constructed rather than memset - WorkItem
  // now has non-trivial shared_ptr members that must actually be
  // constructed. The real dispatch_processed_regions frees a destroyed
  // item via a plain (non-virtual) `operator delete`, never running our
  // WorkItemNode destructor - harmless here since regionRows/lut/pixelTransitions are
  // always left non-owning (ctrl==nullptr) in this probe, so there's
  // nothing for a skipped destructor to leak.
  ti->node = new (malloc(sizeof(WorkItemNode))) WorkItemNode();
  WorkItem* item = &ti->node->item;

  // A degenerate rect (y1<y0 or x1<x0, used to test the destroy path) never
  // gets its regionRows/pixelDataPtr buffer read by dispatch_processed_regions - the
  // degenerate check on the rect fields alone short-circuits before any
  // pixel access - so skip the buffer allocation entirely rather than feed
  // malloc a negative-turned-huge size (rows/cols would be <=0).
  bool degenerate = y1 < y0 || x1 < x0;
  if (degenerate) {
    ti->pixelBuf = nullptr;
    memset(&ti->rr, 0, sizeof(ti->rr));
    item->regionRows = non_owning_sp(&ti->rr);
    item->pixelDataPtr = 0;
    item->rectY0 = y0;
    item->rectX0 = x0;
    item->rectY1 = y1;
    item->rectX1 = x1;
    item->sync = sync;
    item->pixelMode = 0;
    return;
  }

  int rows = y1 - y0 + 1;
  int cols = x1 - x0 + 1;
  int strideRows = (rows + 15) & ~15;
  ti->pixelBuf = (uint8_t*)malloc((size_t)strideRows * cols);
  memset(ti->pixelBuf, pixelFill, (size_t)strideRows * cols);

  ti->rr.dataPtr = ti->pixelBuf;
  ti->rr.y0 = y0;
  ti->rr.x0 = x0;
  ti->rr.y1 = y1;
  ti->rr.x1 = x1;
  ti->rr.stride = strideRows;
  ti->rr.size = strideRows * cols;

  item->regionRows = non_owning_sp(&ti->rr);
  item->pixelDataPtr = (int32_t)(intptr_t)ti->pixelBuf;
  item->rectY0 = y0;
  item->rectX0 = x0;
  item->rectY1 = y1;
  item->rectX1 = x1;
  item->sync = sync;
  item->pixelMode = 0;
  // item->deps is already empty from WorkItemNode's own construction above.
}

// Fills the (col,row) cell of the real g_pStateBuffer for every pixel in
// the item's rect to `v` (u16, as FUN_0004f8f0 reads it).
static void
fill_state(int y0, int x0, int y1, int x1, uint16_t v) {
  for (int col = x0; col <= x1; col++)
    for (int row = y0; row <= y1; row++)
      *state_cell(col, row) = v;
}

static void
print_item(const char* label, TestItem* ti, ListHead* subList, bool ret) {
  bool empty = subList->next == subList;
  if (empty) {
    // Destroyed (and operator_delete'd) by dispatch_processed_regions -
    // ti->node is a dangling pointer now, do not dereference it.
    printf("%-28s ret=%d listEmpty=1 (item was destroyed)\n", label, (int)ret);
    return;
  }
  WorkItem* item = &ti->node->item;
  printf("%-28s ret=%d listEmpty=%d rect=[y0=%d x0=%d y1=%d x1=%d] pixelTransitions.ptr=%p pixelTransitions.use_count=%ld\n", label,
         (int)ret, (int)empty, item->rectY0, item->rectX0, item->rectY1, item->rectX1,
         (void*)item->pixelTransitions.get(), (long)item->pixelTransitions.use_count());
  if (item->pixelTransitions) {
    // pixelTransitions aliases into the RegionRows-shaped control block at its dataPtr
    // field - print the embedded rect/stride/size the same way
    // FUN_0004f8f0 itself reads them.
    auto* embedded = item->pixelTransitions.get();
    printf("    pixelTransitions as RegionRows: dataPtr=%p y0=%d x0=%d y1=%d x1=%d stride=%d size=%d\n",
           embedded->dataPtr, embedded->y0, embedded->x0, embedded->y1, embedded->x1,
           embedded->stride, embedded->size);
  }
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

  if (init_statebuffer() != 0) {
    fprintf(stderr, "init_statebuffer failed\n");
    return 1;
  }
  printf("init_statebuffer done\n");
  // Bridge our native g_pStateBufferNative into the library's own global,
  // exactly like render_kernel_verify.cpp bridges the gamma table - the
  // commit kernels FUN_0004f8f0/FUN_0004e680 read/write g_pStateBuffer
  // directly (not via a parameter).
  statebuffer_globals()->pStatebuffer = g_pStateBufferNative;
  printf("bridged pStatebuffer\n");

  auto dispatch_processed_regions =
    (DispatchProcessedRegionsFn)(g_runtime_offset + DISPATCH_PROCESSED_REGIONS_ADDR);

  const int Y0 = 100, X0 = 200, Y1 = 139, X1 = 209; // 40 rows x 10 cols (<29 -> chunkCount=1)

  // --- Experiment 1: sync=0, EVERY pixel already matches state (pixelDataPtr==state
  // as u16) - does the item survive with its FULL original rect, or does it
  // come back degenerate/get destroyed?
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 1;

    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X1, /*sync=*/0, /*pixelFill=*/5);
    fill_state(Y0, X0, Y1, X1, 5); // state == pixelDataPtr everywhere: nothing "changed"
    list_insert_before(&batch.subList, ti.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    print_item("exp1 sync0-all-unchanged", &ti, &batch.subList, ret);
  }

  // --- Experiment 2: sync=0, EVERY pixel differs (pixelDataPtr != state) - sanity
  // check, should definitely survive with the full rect.
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 1;

    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X1, /*sync=*/0, /*pixelFill=*/5);
    fill_state(Y0, X0, Y1, X1, 9); // state != pixelDataPtr everywhere: everything "changed"
    list_insert_before(&batch.subList, ti.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    print_item("exp2 sync0-all-changed", &ti, &batch.subList, ret);
  }

  // --- Experiment 3: sync=0, exactly ONE pixel differs - does the reported
  // rect narrow tightly around it (and by how much, e.g. 8-row alignment)?
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 1;

    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X1, /*sync=*/0, /*pixelFill=*/5);
    fill_state(Y0, X0, Y1, X1, 5);
    *state_cell(X0 + 5, Y0 + 20) = 9; // single differing pixel: col=X0+5, row=Y0+20
    list_insert_before(&batch.subList, ti.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    print_item("exp3 sync0-one-pixel-changed", &ti, &batch.subList, ret);
    printf("    (changed pixel was at abs col=%d row=%d, rect-relative col=%d row=%d)\n", X0 + 5,
           Y0 + 20, 5, 20);

    // Read back the actual packed values to settle the "(state<<5)|value"
    // packing / "0x0004 unchanged marker" guesses from swtcon_architecture.md
    // §6.3 - both at the one pixel that changed and at an unchanged neighbor
    // in the same 8-row NEON lane group (row=21, same group as row=20).
    uint16_t stateAfterChanged = *state_cell(X0 + 5, Y0 + 20);
    uint16_t stateAfterNeighbor = *state_cell(X0 + 5, Y0 + 21);
    printf("    g_pStateBuffer after: changed-pixel=0x%04x neighbor(unchanged,same group)=0x%04x\n",
           stateAfterChanged, stateAfterNeighbor);
    if (batch.subList.next != &batch.subList) {
      RegionRows* transitionsRr = ti.node->item.pixelTransitions.get();
      // pixelTransitions buffer is column-major, u16/pixel, stride in u16 units - index =
      // stride*(col-x0) + (row-y0), matching FUN_0004f8f0's own addressing.
      auto transition_cell = [&](int col, int row) -> uint16_t {
        uint16_t* base = (uint16_t*)transitionsRr->dataPtr;
        return base[(size_t)transitionsRr->stride * (col - transitionsRr->x0) + (row - transitionsRr->y0)];
      };
      printf("    pixelTransitions buffer: changed-pixel=0x%04x neighbor(unchanged,same group)=0x%04x\n",
             transition_cell(X0 + 5, Y0 + 20), transition_cell(X0 + 5, Y0 + 21));
    }
  }

  // --- Experiment 4: sync=1 ("force") with all pixels matching - docs say
  // force unconditionally writes every pixel; does the rect still come back
  // full regardless of state match?
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 1;

    TestItem ti;
    build_item(&ti, Y0, X0, Y1, X1, /*sync=*/1, /*pixelFill=*/5);
    fill_state(Y0, X0, Y1, X1, 9); // old state=9, new (pixelDataPtr)=5 - distinguishable in the packed value
    list_insert_before(&batch.subList, ti.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    print_item("exp4 sync1-force-all-match", &ti, &batch.subList, ret);
    if (batch.subList.next != &batch.subList) {
      uint16_t stateAfter = *state_cell(X0 + 5, Y0 + 20);
      RegionRows* transitionsRr = ti.node->item.pixelTransitions.get();
      uint16_t* base = (uint16_t*)transitionsRr->dataPtr;
      uint16_t transitionAfter = base[(size_t)transitionsRr->stride * (5) + (20)];
      printf("    g_pStateBuffer after=0x%04x transitions after=0x%04x (old=9 new=5; force packs "
             "(newState<<5)|newState if formula holds)\n",
             stateAfter, transitionAfter);
    }
  }

  // --- Experiment 5: degenerate original rect (y1<y0) - confirm destroy
  // path (item removed from list) as already documented for other
  // degenerate-rect handling in this codebase.
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 1;

    TestItem ti;
    build_item(&ti, /*y0=*/50, /*x0=*/50, /*y1=*/10, /*x1=*/60, /*sync=*/0, /*pixelFill=*/5);
    list_insert_before(&batch.subList, ti.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    printf("exp5 degenerate-rect          ret=%d listEmpty=%d (expect listEmpty=1, item destroyed)\n",
           (int)ret, (int)(batch.subList.next == &batch.subList));
  }

  // --- Experiment 6: two non-overlapping items in the same batch, one
  // all-unchanged and one all-changed - confirms items are processed
  // independently (no cross-item bbox union), and exercises the batch loop
  // (count=2) instead of a single-item edge case.
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 2;

    TestItem tiA, tiB;
    build_item(&tiA, Y0, X0, Y1, X1, /*sync=*/0, /*pixelFill=*/5);
    fill_state(Y0, X0, Y1, X1, 5); // A: all unchanged
    build_item(&tiB, Y0 + 500, X0 + 500, Y1 + 500, X1 + 500, /*sync=*/0, /*pixelFill=*/5);
    fill_state(Y0 + 500, X0 + 500, Y1 + 500, X1 + 500, 9); // B: all changed, far away

    list_insert_before(&batch.subList, tiA.node);
    list_insert_before(&batch.subList, tiB.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    printf("exp6 two-item-batch ret=%d\n", (int)ret);
    // Walk what's left in the list first - a destroyed node is freed, so we
    // must not dereference tiA.node/tiB.node unless still present.
    bool aAlive = false, bAlive = false;
    int remaining = 0;
    for (void* p = batch.subList.next; p != &batch.subList; p = ((ListHead*)p)->next) {
      remaining++;
      if (p == tiA.node)
        aAlive = true;
      if (p == tiB.node)
        bAlive = true;
    }
    printf("    remaining items in list: %d\n", remaining);
    if (aAlive)
      printf("    A (all-unchanged) rect=[y0=%d x0=%d y1=%d x1=%d]\n", tiA.node->item.rectY0,
             tiA.node->item.rectX0, tiA.node->item.rectY1, tiA.node->item.rectX1);
    else
      printf("    A (all-unchanged) DESTROYED\n");
    if (bAlive)
      printf("    B (all-changed)   rect=[y0=%d x0=%d y1=%d x1=%d]\n", tiB.node->item.rectY0,
             tiB.node->item.rectX0, tiB.node->item.rectY1, tiB.node->item.rectX1);
    else
      printf("    B (all-changed)   DESTROYED\n");
  }

  // --- Experiment 7: chunkCount=2 case (X-span >= 29) - all-unchanged, to
  // see whether the 2-chunk path produces the same full-rect-survives
  // result as the 1-chunk path in experiment 1.
  {
    BatchNode batch;
    memset(&batch, 0, sizeof(batch));
    batch.subList.next = &batch.subList;
    batch.subList.prev = &batch.subList;
    batch.count = 1;

    int wideX1 = X0 + 39; // 40 cols: >= 29 -> chunkCount=2
    TestItem ti;
    build_item(&ti, Y0, X0, Y1, wideX1, /*sync=*/0, /*pixelFill=*/5);
    fill_state(Y0, X0, Y1, wideX1, 5);
    list_insert_before(&batch.subList, ti.node);

    printf("  calling dispatch_processed_regions...\n");
    bool ret = dispatch_processed_regions(&batch.subList);
    print_item("exp7 sync0-wide-all-unchanged", &ti, &batch.subList, ret);
  }

  return 0;
}
