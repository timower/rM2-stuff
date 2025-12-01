#include "rm2fb/SharedBuffer.h"

#include <dlfcn.h>
#include <iostream>

void
qimageHook(void (*orig)(void*, int, int, int),
           void* that,
           int width,
           int height,
           int format) {
  static bool firstAlloc = true;

  if (const auto& fb = SharedFB::getInstance();
      width == fb_width && height == fb_height && firstAlloc && fb.isValid()) {
    static const auto q_image_ctor_with_buffer = (void (*)(
      void*, uint8_t*, int32_t, int32_t, int32_t, int, void (*)(void*), void*))
      dlsym(RTLD_NEXT, "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_");

    std::cerr << "REPLACING THE IMAGE with shared memory\n";
    firstAlloc = false;

    q_image_ctor_with_buffer(that,
                             reinterpret_cast<uint8_t*>(fb.getFb()),
                             fb_width,
                             fb_height,
                             fb_width * fb_pixel_size,
                             format,
                             nullptr,
                             nullptr);
    return;
  }

  orig(that, width, height, format);
}
