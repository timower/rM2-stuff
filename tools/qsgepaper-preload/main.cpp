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

enum UpdateFlags {
    Sync = 1 << 0,
    FullRefresh = 1 << 1,
    FastDraw = 1 << 2,
};

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
    uint16_t* image = swtcon_init();
    if (!image) {
        return 1;
    }
    
    std::cout << "Buffer: " << (void*)image << std::endl;

    if (getenv("SWTCON_DUMP")) {
        swtcon_dump_waveform();
        swtcon_dump_buffers();
    }

    if (image) {
        auto do_update = [&](update_data& req) {
            swtcon_lock();
            swtcon_update(&req);
            swtcon_unlock_post();
            if (req.flags & Sync) {
                swtcon_wait();
            }
        };

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
        
        update_data req1 = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, FullRefresh | Sync, HQ, 0, 9 };
        TIME(do_update(req1));
        std::cout << "Done" << std::endl;
        getchar();

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
        update_data req2 = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, FullRefresh | Sync, MEDIUM, 0, 6 };
        TIME(do_update(req2));
        std::cout << "Done" << std::endl;
        getchar();

        std::cout << "Clearing" << std::endl;
        for (int y = 0; y < SCREEN_HEIGHT; y++)
          for (int x = 0; x < SCREEN_WIDTH; x++)
            image[SCREEN_WIDTH * y + x] = 0xFFFF;
        
        update_data req3 = { 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, FullRefresh | Sync, HQ, 0, 9 };
        TIME(do_update(req3));
        std::cout << "Done" << std::endl;
        getchar();

        // --- Region-overlap coverage ---
        //
        // Everything above is a single full-screen Sync update, which always
        // finds the accumulation list empty: it never exercises
        // native_subtract_update_region's overlap/split logic. The tests
        // below issue *multiple* swtcon_update() calls under one
        // swtcon_lock()/swtcon_unlock_post() pair (a real, intended usage
        // pattern - e.g. an app coalescing several dirty regions into one
        // hardware pass) so each later update's rect is clipped against the
        // earlier ones still sitting in the accumulation list.
        //
        // update_data's "height"/"width" fields are actually the
        // opposite-corner (y1/x1) coordinates, not sizes - see AGENTS.md.
        auto rect_req = [](int y0, int x0, int y1, int x1, int mode, int pixel_mode) {
            return update_data{ y0, x0, y1, x1, 0, mode, 0, pixel_mode };
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

    // Memory will be cleaned up by OS, skipping destroy for this test tool.
    std::cout << "Shutting down..." << std::endl;
    swtcon_shutdown(0);

    return 0;
}


