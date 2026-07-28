#include <iostream>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <csignal>
#include <initializer_list>
#include <ucontext.h>
#include "swtcon.h"

// Report the faulting PC as a libqsgepaper Ghidra address so crashes inside the
// black-box library can be located during the native-init bring-up.
static void
segv_handler(int sig, siginfo_t* si, void* ucv) {
  auto* uc = (ucontext_t*)ucv;
  unsigned long pc = uc->uc_mcontext.arm_pc;
  unsigned long lr = uc->uc_mcontext.arm_lr;
  uintptr_t off = swtcon_runtime_offset();
  fprintf(stderr, "\n*** SIGSEGV: fault_addr=%p pc=0x%lx lr=0x%lx\n",
          si->si_addr, pc, lr);
  fprintf(stderr, "*** ghidra: pc=0x%lx lr=0x%lx (runtime_offset=0x%lx)\n",
          pc - off, lr - off, (unsigned long)off);
  fprintf(stderr,
          "*** regs: r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r4=0x%08lx r5=0x%08lx "
          "r6=0x%08lx r7=0x%08lx r8=0x%08lx r9=0x%08lx r10=0x%08lx fp=0x%08lx ip=0x%08lx "
          "sp=0x%08lx\n",
          (unsigned long)uc->uc_mcontext.arm_r0, (unsigned long)uc->uc_mcontext.arm_r1,
          (unsigned long)uc->uc_mcontext.arm_r2, (unsigned long)uc->uc_mcontext.arm_r3,
          (unsigned long)uc->uc_mcontext.arm_r4, (unsigned long)uc->uc_mcontext.arm_r5,
          (unsigned long)uc->uc_mcontext.arm_r6, (unsigned long)uc->uc_mcontext.arm_r7,
          (unsigned long)uc->uc_mcontext.arm_r8, (unsigned long)uc->uc_mcontext.arm_r9,
          (unsigned long)uc->uc_mcontext.arm_r10, (unsigned long)uc->uc_mcontext.arm_fp,
          (unsigned long)uc->uc_mcontext.arm_ip, (unsigned long)uc->uc_mcontext.arm_sp);
  fflush(stderr);
  signal(sig, SIG_DFL);
  raise(sig);
}

static void
install_segv_handler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = segv_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, nullptr);
}

#define SCREEN_WIDTH 1404
#define SCREEN_HEIGHT 1872

// update modes from swtcon
enum UpdateMode {
    HQ = 2,
    MEDIUM = 3,
    FAST = 4,
};

// UpdateFlags (Sync/FastDraw/ExplicitTemperature) lives in swtcon.h, shared
// with update.cpp's swtcon_update - see that header for why, and for
// why bit 1 is FastDraw rather than the "FullRefresh" this project
// originally (and wrongly) called it.

#define TIME(x)                                                                \
  do {                                                                         \
    auto t1 = std::chrono::high_resolution_clock::now();                       \
    (x);                                                                       \
    auto t2 = std::chrono::high_resolution_clock::now();                       \
    auto duration =                                                            \
      std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();  \
    std::cout << "Duration: " << duration << " us" << std::endl;               \
  } while (0);

int main(int argc, char** argv) {
    install_segv_handler();

    // No argument: run every test case (unchanged default behavior). "0":
    // init only, skip the whole update block (same as the old
    // SWTCON_INIT_ONLY env var, kept working below for compatibility). "N"
    // (>=1): run only test case N, skipping every other one - added to let
    // an A/B pan-capture comparison isolate a single test case, since the
    // racy un-synced-overlap/active-dependency tests (cases 8 and 9) are
    // the prime suspects for divergence that isn't a real bug, just
    // real-time interleaving noise.
    bool has_test_arg = argc > 1;
    int selected_test = has_test_arg ? atoi(argv[1]) : -1;
    auto should_run = [&](int n) { return !has_test_arg || selected_test == n; };

    uint16_t* image = swtcon_init();
    if (!image) {
        return 1;
    }

    std::cout << "Buffer: " << (void*)image << std::endl;

    if (getenv("SWTCON_DUMP")) {
        swtcon_dump_waveform();
        swtcon_dump_buffers();
    }

    if (image && !getenv("SWTCON_INIT_ONLY") && selected_test != 0) {
        auto do_update = [&](update_data& req) {
            swtcon_lock();
            swtcon_update(&req);
            swtcon_unlock_post();
            if (req.flags & Sync) {
                swtcon_wait();
            }
        };

        if (should_run(1)) {
        std::cout << "Testing HQ" << std::endl;
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
          for (int x = 0; x < SCREEN_WIDTH; x++) {
            auto* ptr = &image[y * SCREEN_WIDTH + x];
            if ((x / 10) % 2 == (y / 10) % 2) {
              *ptr = 0;
            } else {
              *ptr = 0xFFFF;
            }
          }
        }

        update_data req1 = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, FastDraw | Sync, HQ, 0, 9 };
        TIME(do_update(req1));
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(2)) {
        std::cout << "Testing medium" << std::endl;
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
          for (int x = 0; x < SCREEN_WIDTH; x++) {
            auto* ptr = &image[y * SCREEN_WIDTH + x];
            if ((x / 22) % 2 == 0 && (y / 22) % 2 == 0) {
              *ptr = 0xFFF;
            } else {
              *ptr = 0x0;
            }
          }
        }
        update_data req2 = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, FastDraw | Sync, MEDIUM, 0, 6 };
        TIME(do_update(req2));
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(3)) {
        std::cout << "Clearing" << std::endl;
        for (int y = 0; y < SCREEN_HEIGHT; y++)
          for (int x = 0; x < SCREEN_WIDTH; x++)
            image[SCREEN_WIDTH * y + x] = 0xFFFF;

        update_data req3 = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, FastDraw | Sync, HQ, 0, 9 };
        TIME(do_update(req3));
        std::cout << "Done" << std::endl;
        getchar();
        }

        // --- Region-overlap coverage ---
        //
        // Everything above is a single full-screen Sync update, which always
        // finds the accumulation list empty: it never exercises
        // subtract_update_region's overlap/split logic. The tests
        // below issue *multiple* swtcon_update() calls under one
        // swtcon_lock()/swtcon_unlock_post() pair (a real, intended usage
        // pattern - e.g. an app coalescing several dirty regions into one
        // hardware pass) so each later update's rect is clipped against the
        // earlier ones still sitting in the accumulation list.
        //
        auto rect_req = [](int y0, int x0, int y1, int x1, int mode, int pixel_mode, int flags = 0) {
            return update_data{ y0, x0, y1, x1, flags, mode, 0, pixel_mode };
        };
        auto fill_rect = [&](int y0, int x0, int y1, int x1, uint16_t value) {
            for (int y = y0; y < y1 && y < SCREEN_HEIGHT; y++)
                for (int x = x0; x < x1 && x < SCREEN_WIDTH; x++)
                    image[y * SCREEN_WIDTH + x] = value;
        };
        auto do_batch = [&](std::initializer_list<update_data> reqs) {
            swtcon_lock();
            for (update_data req : reqs) {
                swtcon_update(&req);
            }
            swtcon_unlock_post();
            swtcon_wait();
        };
        // Same as do_batch but deliberately skips swtcon_wait() - lets the
        // caller queue a second, overlapping batch while the first is still
        // actively playing back frames (see the active-dependency test below).
        auto submit_nowait = [&](std::initializer_list<update_data> reqs) {
            swtcon_lock();
            for (update_data req : reqs) {
                swtcon_update(&req);
            }
            swtcon_unlock_post();
        };

        if (should_run(4)) {
        std::cout << "Overlap: two non-overlapping regions in one batch" << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        fill_rect(0, 0, 800, 600, 0x0);
        fill_rect(1100, 900, SCREEN_HEIGHT, SCREEN_WIDTH, 0x0);
        TIME(do_batch({
          rect_req(0, 0, 800, 600, HQ, 9),
          rect_req(1100, 900, SCREEN_HEIGHT, SCREEN_WIDTH, HQ, 9),
        }));
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(5)) {
        std::cout << "Overlap: hole punched in the middle of a pending region "
                     "(exercises all four split pieces)" << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        fill_rect(200, 200, 1600, 1200, 0x0);
        fill_rect(600, 500, 1200, 800, 0xFFFF);
        TIME(do_batch({
          rect_req(200, 200, 1600, 1200, HQ, 9),  // queued first -> becomes the "old" region
          rect_req(600, 500, 1200, 800, HQ, 9),   // queued second -> punches a hole in it
        }));
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(6)) {
        std::cout << "Overlap: single-edge overlap (exercises exactly one "
                     "split piece)" << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        fill_rect(300, 300, 1400, 900, 0x0);
        fill_rect(300, 700, 1400, 1200, 0x8410);
        TIME(do_batch({
          rect_req(300, 300, 1400, 900, HQ, 9),
          rect_req(300, 700, 1400, 1200, HQ, 9), // same y-range, x-range only partially overlaps
        }));
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(7)) {
        std::cout << "Overlap: second region fully contains the first "
                     "(exercises full-containment removal, zero pieces)" << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        fill_rect(700, 600, 1100, 900, 0x0);
        fill_rect(100, 100, 1700, 1300, 0x8410);
        TIME(do_batch({
          rect_req(700, 600, 1100, 900, HQ, 9),   // small, queued first
          rect_req(100, 100, 1700, 1300, HQ, 9),  // fully contains the first, queued second
        }));
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(8)) {
        std::cout << "Overlap: burst of un-synced overlapping updates "
                     "(best-effort coverage of clipping against not-yet-"
                     "claimed queued batches, races the worker thread)" << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        for (int i = 0; i < 8; i++) {
            int y0 = 100 * i, x0 = 100 * i;
            int y1 = y0 + 300, x1 = x0 + 300;
            fill_rect(y0, x0, y1, x1, (i % 2) ? 0x0 : 0x8410);
            update_data req = rect_req(y0, x0, y1, x1, HQ, 9);
            swtcon_lock();
            swtcon_update(&req);
            swtcon_unlock_post();
            // Deliberately no swtcon_wait() here - the next iteration's queue_update
            // may run before the worker claims this batch, exercising the
            // "clip pending, unclaimed batches too" loop in swtcon_update.
        }
        swtcon_lock();
        update_data drain = rect_req(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, HQ, 9);
        swtcon_update(&drain);
        swtcon_unlock_post();
        swtcon_wait();
        std::cout << "Done" << std::endl;
        getchar();
        }

        // --- Playback-kernel coverage (see AGENTS.md's FUN_0004a140/
        // FUN_0004a234 next-step and swtcon-ab-test's KERN capture, which
        // found the tests above never exercise the "plain" kernel or
        // chunk_count=1) ---

        if (should_run(9)) {
        std::cout << "Overlap: in-flight active dependency (both items "
                     "FastDraw, neither Sync) forcing the second item "
                     "onto the 'plain' playback kernel (0x4a140) instead of "
                     "'overlap' (0x4a234) for as long as the dependency is "
                     "active - the branch never hit by any test above. "
                     "Needs the first item to actually clear its own "
                     "dispatch gate (display_thread_func's "
                     "nFrameCleanupCursor pacing check) before the second "
                     "is submitted, which takes several real frame ticks -"
                     " a short sleep is not enough (confirmed empirically "
                     "in swtcon-ab-test)." << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        fill_rect(300, 300, 1600, 1300, 0x0);
        submit_nowait({ rect_req(300, 300, 1600, 1300, HQ, 9, FastDraw) });
        usleep(300000);
        fill_rect(700, 700, 1200, 1000, 0x8410);
        submit_nowait({ rect_req(700, 700, 1200, 1000, HQ, 9, FastDraw) });
        swtcon_wait();
        std::cout << "Done" << std::endl;
        getchar();
        }

        if (should_run(10)) {
        std::cout << "Overlap: small isolated rect (area well under 20000px) "
                     "-> forces playback_chunk_count's synchronous "
                     "chunk_count=1 dispatch path, never exercised by the "
                     "large rects above (which always split into 2 chunks). "
                     "Flips white then black so the second update is "
                     "guaranteed to be a real change regardless of what an "
                     "earlier test left committed here - an all-unchanged "
                     "item is silently discarded before dispatch." << std::endl;
        fill_rect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, 0xFFFF);
        fill_rect(100, 100, 180, 180, 0xFFFF);
        TIME(do_batch({ rect_req(100, 100, 180, 180, HQ, 9) }));
        fill_rect(100, 100, 180, 180, 0x0);
        TIME(do_batch({ rect_req(100, 100, 180, 180, HQ, 9) }));
        std::cout << "Done" << std::endl;
        getchar();
        }
    }

    // Memory will be cleaned up by OS, skipping destroy for this test tool.
    std::cout << "Shutting down..." << std::endl;
    swtcon_shutdown(0);

    return 0;
}


