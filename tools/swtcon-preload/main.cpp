#include <dlfcn.h>
#include <iostream>
#include <unistd.h>

#include <chrono>

#include "swtcon.h"

#define TIME(x)                                                                \
  do {                                                                         \
    auto t1 = std::chrono::high_resolution_clock::now();                       \
    (x);                                                                       \
    auto t2 = std::chrono::high_resolution_clock::now();                       \
    auto duration =                                                            \
      std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();  \
    std::cout << "Duration: " << duration << " us" << std::endl;               \
  } while (0);

int
myMain(int argc, char** argv, char** env) {

  auto* state = swtcon_init("/dev/fb0");
  if (state == nullptr) {
    std::cerr << "Error initializing swtcon" << std::endl;
    return 1;
  }

  swtcon_dump(state);

  if (true) {
    uint16_t* image = (uint16_t*)swtcon_getbuffer(state);

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
    TIME(swtcon_update(
      state, { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT }, HQ, FullRefresh | Sync));
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
    TIME(swtcon_update(state,
                       { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                       MEDIUM,
                       FullRefresh | Sync));
    std::cout << "Done" << std::endl;
    getchar();

    std::cout << "Clearing" << std::endl;
    for (int y = 0; y < SCREEN_HEIGHT; y++)
      for (int x = 0; x < SCREEN_WIDTH; x++)
        image[SCREEN_WIDTH * y + x] = 0xFFFF;
    TIME(swtcon_update(
      state, { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT }, HQ, FullRefresh | Sync));
    std::cout << "Done" << std::endl;
    getchar();

    int y = SCREEN_HEIGHT / 6;
    std::cout << "Testing HQ (partial): " << y << std::endl;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      image[(y - 1) * SCREEN_WIDTH + x] = 0;
      image[y * SCREEN_WIDTH + x] = 0;
      image[(y + 1) * SCREEN_WIDTH + x] = 0;
    }
    TIME(swtcon_update(state, { 0, y - 2, SCREEN_WIDTH, y + 2 }, HQ, Sync));
    std::cout << "Done" << std::endl;
    getchar();

    y = 3 * SCREEN_HEIGHT / 6;
    std::cout << "Testing medium (partial): " << y << std::endl;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      image[(y - 1) * SCREEN_WIDTH + x] = 0;
      image[y * SCREEN_WIDTH + x] = 0;
      image[(y + 1) * SCREEN_WIDTH + x] = 0;
    }
    TIME(swtcon_update(state, { 0, y - 2, SCREEN_WIDTH, y + 2 }, MEDIUM, Sync));
    std::cout << "Done" << std::endl;
    getchar();

    y = 5 * SCREEN_HEIGHT / 6;
    std::cout << "Testing direct update (partial): " << y << std::endl;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      image[(y - 1) * SCREEN_WIDTH + x] = 0;
      image[y * SCREEN_WIDTH + x] = 0;
      image[(y + 1) * SCREEN_WIDTH + x] = 0;
    }
    TIME(swtcon_update(state, { 0, y - 2, SCREEN_WIDTH, y + 2 }, FAST, Sync));
    std::cout << "Done" << std::endl;
    getchar();

    y = 2 * SCREEN_HEIGHT / 6;
    std::cout << "Testing direct update (partial + FastDraw): " << y
              << std::endl;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      image[(y - 1) * SCREEN_WIDTH + x] = 0;
      image[y * SCREEN_WIDTH + x] = 0;
      image[(y + 1) * SCREEN_WIDTH + x] = 0;
    }
    TIME(swtcon_update(
      state, { 0, y - 2, SCREEN_WIDTH, y + 2 }, FAST, FastDraw | Sync));
    std::cout << "Done" << std::endl;
  }

  swtcon_destroy(state);

  return 0;
}

extern "C" {

int
__libc_start_main(int (*_main)(int, char**, char**),
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(void),
                  void (*rtld_fini)(void),
                  void* stack_end) {

  static auto* func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  std::cout << "Hello preload" << std::endl;

  auto ret = func_main(myMain, argc, argv, init, fini, rtld_fini, stack_end);

  return ret;
}
}
