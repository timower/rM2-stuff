#include <iostream>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <csignal>
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
    }

    // Memory will be cleaned up by OS, skipping destroy for this test tool.
    std::cout << "Shutting down..." << std::endl;
    swtcon_shutdown(0);

    return 0;
}


