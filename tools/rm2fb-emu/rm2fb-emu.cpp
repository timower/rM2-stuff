#include "Socket.h"

// rm2fb
#include <Message.h>

#include <unistdpp/socket.h>

#include <FrameBuffer.h>
#include <Input.h>

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace unistdpp;
using namespace rmlib::input;

namespace {

bool running = true;

} // namespace

int
main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("\n Usage: %s <ip of server> <port> \n", argv[0]);
    return 1;
  }

  int port = atoi(argv[2]);
  auto sock = getClientSock(argv[1], port);
  if (!sock.has_value()) {
    std::cout << "Couldn't get tcp socket: " << toString(sock.error()) << "\n";
    return EXIT_FAILURE;
  }

  sendMessage(*sock, ClientMsg(GetUpdate{}));

  auto fb = rmlib::fb::FrameBuffer::open();
  if (!fb.has_value()) {
    std::cerr << fb.error().msg;
    return EXIT_FAILURE;
  }

  auto input = rmlib::input::InputManager();
  input.openAll(false);

  fb->clear();

  while (running) {
    auto fdsOrErr = input.waitForInput(std::nullopt, *sock);
    if (!fdsOrErr.has_value()) {
      std::cerr << "Error input: " << fdsOrErr.error().msg;
      break;
    }

    auto [events, fds] = *fdsOrErr;

    for (const auto& event : events) {
      if (!std::holds_alternative<PenEvent>(event)) {
        continue;
      }

      const auto& touchEv = std::get<PenEvent>(event);
      const auto type = [&] {
        if (touchEv.isDown()) {
          return 1;
        }
        if (touchEv.isUp()) {
          return 2;
        }
        return 0;
      }();

      if (type != 0) {
        std::cout << "Touch @ " << touchEv.location << "\n";
      }

      ClientMsg input = Input{ touchEv.location.x, touchEv.location.y, type };
      auto res = sendMessage(*sock, input);
      if (!res) {
        std::cerr << "Error writing: " << toString(res.error()) << "\n";
      }
    }

    if (!fds[0]) {
      continue;
    }

    auto msgOrErr = sock->readAll<UpdateParams>();
    if (!msgOrErr) {
      std::cerr << "Error reading: " << toString(msgOrErr.error()) << "\n";
      break;
    }
    auto msg = *msgOrErr;

    if ((msg.flags & 4) == 0) {
      std::cout << "Got msg: "
                << "{ { " << msg.x1 << ", " << msg.y1 << "; " << msg.x2 << ", "
                << msg.y2 << " }, wave: " << msg.waveform
                << " flags: " << msg.flags << " }\n";
    }

    int width = msg.x2 - msg.x1 + 1;
    int height = msg.y2 - msg.y1 + 1;
    int bufSize = width * height;
    std::vector<uint16_t> buffer(bufSize);

    int readSize = bufSize * sizeof(uint16_t);
    auto res = sock->readAll(buffer.data(), readSize);
    if (!res) {
      std::cerr << "Error reading: " << toString(res.error()) << "\n";
      break;
    }

    rmlib::Rect region = { .topLeft = { msg.x1, msg.y1 },
                           .bottomRight = { msg.x2, msg.y2 } };
    assert(fb->canvas.rect().contains(region));

    uint16_t* mem = (uint16_t*)fb->canvas.getMemory();
    for (int row = 0; row < height; row++) {
      int fbRow = row + msg.y1;
      memcpy(mem + fbRow * fb->canvas.width() + msg.x1,
             buffer.data() + row * width,
             width * sizeof(uint16_t));
    }

    fb->doUpdate(region,
                 (rmlib::fb::Waveform)msg.waveform,
                 (rmlib::fb::UpdateFlags)msg.flags);
  }

  return 0;
}
