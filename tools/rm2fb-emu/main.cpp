#include <FrameBuffer.h>
#include <Input.h>

#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace rmlib::input;

namespace {

bool running = true;

struct Params {
  int32_t y1;
  int32_t x1;
  int32_t y2;
  int32_t x2;

  int32_t flags;
  int32_t waveform;
};
static_assert(sizeof(Params) == 6 * 4, "Params has wrong size?");

struct Input {
  int32_t x;
  int32_t y;
  int32_t type; // 1 = down, 2 = up
};

int
getClientSock(const char* addr, int port) {

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
    perror("\n inet_pton error occured\n");
    return -1;
  }

  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Connect");
    return -1;
  }

  return sockfd;
}

int
readAll(int fd, char* buf, int size) {
  int readden = 0;

  while (readden < size) {
    int res = read(fd, buf + readden, size - readden);
    if (res == -1) {
      return -1;
    }
    readden += res;
  }
  return readden;
}

} // namespace

int
main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("\n Usage: %s <ip of server> <port> \n", argv[0]);
    return 1;
  }

  int port = atoi(argv[2]);
  auto sock = getClientSock(argv[1], port);
  if (sock == -1) {
    return EXIT_FAILURE;
  }

  auto fb = rmlib::fb::FrameBuffer::open();
  if (!fb.has_value()) {
    std::cerr << fb.error().msg;
    return EXIT_FAILURE;
  }

  auto input = rmlib::input::InputManager();
  input.openAll(false);

  fb->clear();

  while (running) {
    auto fdsOrErr = input.waitForInput(std::nullopt, sock);
    if (!fdsOrErr.has_value()) {
      std::cerr << "Error input: " << fdsOrErr.error().msg;
      break;
    }

    auto [events, fds] = *fdsOrErr;

    for (const auto& event : events) {
      if (!std::holds_alternative<TouchEvent>(event)) {
        continue;
      }

      const auto& touchEv = std::get<TouchEvent>(event);
      const auto type = [&] {
        if (touchEv.isDown()) {
          return 1;
        }
        if (touchEv.isUp()) {
          return 2;
        }
        return 0;
      }();

      auto input = Input{ touchEv.location.x, touchEv.location.y, type };
      int res = write(sock, &input, sizeof(Input));
      if (res != sizeof(Input)) {
        perror("Write input");
      }
    }

    if (!fds[0]) {
      continue;
    }

    Params msg;
    int size = read(sock, &msg, sizeof(Params));
    if (size != sizeof(Params)) {
      std::cerr << "Read: " << size;
      perror(" Read failed?");
      break;
    }

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
    size = readAll(sock, (char*)buffer.data(), readSize);
    if (size != readSize) {
      std::cerr << "Read: " << size;
      perror(" Read failed?");
      break;
    }

    uint16_t* mem = (uint16_t*)fb->canvas.getMemory();
    for (int row = 0; row < height; row++) {
      int fbRow = row + msg.y1;
      memcpy(mem + fbRow * fb->canvas.width() + msg.x1,
             buffer.data() + row * width,
             width * sizeof(uint16_t));
    }

    fb->doUpdate(
      { .topLeft = { msg.x1, msg.y1 }, .bottomRight = { msg.x2, msg.y2 } },
      (rmlib::fb::Waveform)msg.waveform,
      (rmlib::fb::UpdateFlags)msg.flags);
  }

  return 0;
}
