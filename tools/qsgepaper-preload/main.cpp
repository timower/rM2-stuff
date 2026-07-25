#include <dlfcn.h>
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <cstring>

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

// Symbol addresses for version 3.23.0.54 / 3.23.0.64 (assumes 0x10000 image base)
#define INSTANCE_ADDR 0x35de0
#define INIT_ADDR 0x3b814
#define UPDATE_ADDR 0x3ccac
#define LOCK_ADDR 0x3b690
#define UNLOCK_POST_ADDR 0x3dd90
#define WAIT_ADDR 0x3b644
#define SHUTDOWN_ADDR 0x3b6b4

typedef int (*init_func_t)(void* state_out, int zero);

struct update_data {
    int y;
    int x;
    int height;
    int width;
    int flags;
    int update_mode;
    int zero;
    int pixel_mode;
};
typedef int (*update_func_t)(update_data* data);
typedef void (*lock_func_t)();
typedef void (*unlock_post_func_t)();
typedef void (*wait_func_t)();
typedef void (*shutdown_func_t)(int state_ptr_or_zero);

void* libqsgepaper_handle = nullptr;
init_func_t qsgepaper_init = nullptr;
update_func_t qsgepaper_update = nullptr;
lock_func_t qsgepaper_lock = nullptr;
unlock_post_func_t qsgepaper_unlock_post = nullptr;
wait_func_t qsgepaper_wait = nullptr;
shutdown_func_t qsgepaper_shutdown = nullptr;

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
    libqsgepaper_handle = dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
    if (!libqsgepaper_handle) {
        std::cerr << "Failed to load libqsgepaper.so from system: " << dlerror() << std::endl;
        libqsgepaper_handle = dlopen("./libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
        if (!libqsgepaper_handle) {
            std::cerr << "Failed to load libqsgepaper.so locally: " << dlerror() << std::endl;
            return 1;
        }
    }

    void* instance_func = dlsym(libqsgepaper_handle, "_ZN13EPFramebuffer8instanceEv");
    if (!instance_func) {
        std::cerr << "Failed to find known export symbol: " << dlerror() << std::endl;
        return 1;
    }
    
    // Calculate runtime difference from original mapped addresses
    uintptr_t runtime_offset = (uintptr_t)instance_func - INSTANCE_ADDR;
    qsgepaper_init = (init_func_t)(runtime_offset + INIT_ADDR);
    qsgepaper_update = (update_func_t)(runtime_offset + UPDATE_ADDR);
    qsgepaper_lock = (lock_func_t)(runtime_offset + LOCK_ADDR);
    qsgepaper_unlock_post = (unlock_post_func_t)(runtime_offset + UNLOCK_POST_ADDR);
    qsgepaper_wait = (wait_func_t)(runtime_offset + WAIT_ADDR);
    qsgepaper_shutdown = (shutdown_func_t)(runtime_offset + SHUTDOWN_ADDR);

    char state[256];
    memset(state, 0, sizeof(state));
    
    std::cout << "Initializing swtcon..." << std::endl;
    int init_res = qsgepaper_init(state, 0);
    if (init_res != 0) {
        std::cerr << "Error initializing swtcon, res: " << init_res << std::endl;
        return 1;
    }

    // state struct + 0x14 has the buffer pointer
    uint16_t* image = *(uint16_t**)(state + 0x14);

    std::cout << "Buffer: " << (void*)image << std::endl;

    if (image) {
        auto do_update = [&](update_data& req) {
            qsgepaper_lock();
            qsgepaper_update(&req);
            qsgepaper_unlock_post();
            if (req.flags & Sync) {
                qsgepaper_wait();
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
    qsgepaper_shutdown(0);

    return 0;
}


