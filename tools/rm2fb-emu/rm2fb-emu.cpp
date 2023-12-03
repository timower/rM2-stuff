#include "Socket.h"

// rm2fb
#include <Message.h>

// unistdpp
#include <unistdpp/socket.h>

// rmlib
#include <FrameBuffer.h>
#include <Input.h>
#include <UI/Util.h>

#include <arpa/inet.h>
#include <cstdlib>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace unistdpp;
using namespace rmlib::input;

namespace {

int
getType(const PenEvent& touchEv) {
  if (touchEv.isDown()) {
    return 1;
  }
  if (touchEv.isUp()) {
    return 2;
  }
  return 0;
}

Result<std::pair<rmlib::UpdateRegion, rmlib::MemoryCanvas>>
readUpdate(const FD& sock) {
  auto msg = TRY(sock.readAll<UpdateParams>());
  if ((msg.flags & 4) == 0) {
    std::cout << "Got msg: "
              << "{ { " << msg.x1 << ", " << msg.y1 << "; " << msg.x2 << ", "
              << msg.y2 << " }, wave: " << msg.waveform
              << " flags: " << msg.flags << " }\n";
  }

  rmlib::Rect region = { .topLeft = { msg.x1, msg.y1 },
                         .bottomRight = { msg.x2, msg.y2 } };
  rmlib::MemoryCanvas memCanvas(
    region.width(), region.height(), sizeof(uint16_t));

  TRY(sock.readAll(memCanvas.memory.get(), memCanvas.canvas.totalSize()));

  return std::pair{ rmlib::UpdateRegion(region,
                                        (rmlib::fb::Waveform)msg.waveform,
                                        (rmlib::fb::UpdateFlags)msg.flags),
                    std::move(memCanvas) };
}

} // namespace

int
main(int argc, char* argv[]) {
  const char* programName = argv[0]; // NOLINT
  if (argc != 3) {
    std::cout << "Usage: " << programName << " <ip of server> <port> \n";
    return 1;
  }
  const char* portArg = argv[2]; // NOLINT
  const char* addrArg = argv[1]; // NOLINT

  int port = atoi(portArg);
  auto sock =
    fatalOnError(getClientSock(addrArg, port), "Couldn't get tcp socket: ");

  sendMessage(sock, ClientMsg(GetUpdate{}));

  auto fb = rmlib::fb::FrameBuffer::open();
  if (!fb.has_value()) {
    std::cerr << fb.error().msg;
    return EXIT_FAILURE;
  }

  auto input = rmlib::input::InputManager();
  input.openAll(false);

  fb->clear();

  bool running = true;
  while (running) {
    auto fdsOrErr = input.waitForInput(std::nullopt, sock);
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
      const auto type = getType(touchEv);
      if (type != 0) {
        std::cout << "Touch @ " << touchEv.location << "\n";
      }

      ClientMsg input = Input{ touchEv.location.x, touchEv.location.y, type };
      auto res = sendMessage(sock, input);
      if (!res) {
        std::cerr << "Error writing: " << to_string(res.error()) << "\n";
      }
    }

    if (!fds[0]) {
      continue;
    }

    auto msgOrErr = readUpdate(sock);
    if (!msgOrErr.has_value()) {
      std::cerr << "Error reading update: " << to_string(msgOrErr.error())
                << "\n";
      continue;
    }
    auto [update, memCanvas] = std::move(*msgOrErr);
    assert(fb->canvas.rect().contains(update.region));

    rmlib::copy(fb->canvas,
                update.region.topLeft,
                memCanvas.canvas,
                memCanvas.canvas.rect());
    fb->doUpdate(update.region, update.waveform, update.flags);
  }

  return 0;
}
