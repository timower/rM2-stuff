#include "Message.h"
#include "SharedBuffer.h"
#include "Socket.h"

#include <chrono>
#include <cstring>
#include <stdlib.h>

namespace {

Socket clientSock;
SharedFB fb = SharedFB(DEFAULT_FB_NAME);

bool
send(const UpdateParams& params) {
  clientSock.sendto(params);
  auto [result, _] = clientSock.recvfrom<bool>();

  if (!result.value_or(false)) {
    std::cerr << "Error sending update!\n";
    return false;
  }
  return true;
}

// void
// draw(int i, int max) {
//   int xmax = std::min(max, fb_width);
//   int ymax = std::min(max, fb_height);
//   for (int x = 0; x < xmax; x++) {
//     for (int y = 0; y < ymax; y++) {
//       int dx = x / 10;
//       int dy = y / 10;
//       fb.mem[y * fb_width + x] = ((dx + dy + i) % 2 == 0) ? 0xffff : 0x0000;
//     }
//   }
// }

void
fill(int xstart, int ystart, int d, int color) {
  for (int x = xstart; x < xstart + d; x++) {
    for (int y = ystart; y < ystart + d; y++) {
      fb.mem[y * fb_width + x] = color;
    }
  }
}

} // namespace

int
main(int argc, char* argv[]) {
  if (argc != 5) {
    return EXIT_FAILURE;
  }

  auto numTests = atoi(argv[1]);
  auto waveform = atoi(argv[2]);
  auto flags = atoi(argv[3]);
  auto max = atoi(argv[4]);

  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }

  if (!clientSock.bind(nullptr)) {
    return EXIT_FAILURE;
  }
  if (!clientSock.connect(DEFAULT_SOCK_ADDR)) {
    return EXIT_FAILURE;
  }

  std::memset(fb.mem, 0xffff, fb_size);
  send({
    .y1 = 0,
    .x1 = 0,
    .y2 = fb_height - 1,
    .x2 = fb_width - 1,
    .flags = 3,    // full & sync
    .waveform = 2, // GL16
  });

  // auto duration = std::chrono::high_resolution_clock::duration::zero();
  auto start = std::chrono::high_resolution_clock::now();
  int x = 0;
  int y = 0;
  bool color = true;
  for (int i = 0; i < numTests; i++) {
    // draw(i, max);
    fill(x, y, max, color ? 0x0000 : 0xffff);

    UpdateParams test;

    test.x1 = x;
    test.y1 = y;
    test.x2 = x + max;
    test.y2 = y + max;
    test.waveform = waveform;
    test.flags = flags;
    if (i == numTests - 1) {
      test.flags |= 2; // sync
    }

    x += max;
    if (x > fb_width) {
      x = 0;
      y += max;
    }
    if (y > fb_height) {
      y = 0;
      x = 0;
      color = !color;
    }
    send(test);
  }
  auto end = std::chrono::high_resolution_clock::now();

  int ms =
    std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() /
    numTests;
  std::cout << "Update took " << ms << "us\n";

  return 0;
}
