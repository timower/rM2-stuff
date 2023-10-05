#include "ImageHook.h"
#include "Message.h"
#include "Socket.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <libevdev/libevdev-uinput.h>

namespace {
std::atomic_bool running = true;

void
onSigint(int num) {
  running = false;
}

void
setupExitHandler() {
  struct sigaction action;

  action.sa_handler = onSigint;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("Sigaction");
    exit(EXIT_FAILURE);
  }
}

int
writeAll(int fd, const char* buf, int size) {
  int written = 0;

  while (written < size) {
    int res = write(fd, buf + written, size - written);
    if (res == -1) {
      return -1;
    }

    written += res;
  }

  return written;
}

bool
doUpdate(const UpdateParams& params, int tcpFd) {
  static_assert(sizeof(UpdateParams) == 6 * 4, "Params wrong size?");
  write(tcpFd, &params, sizeof(UpdateParams));

  int width = params.x2 - params.x1 + 1;
  int height = params.y2 - params.y1 + 1;
  int size = width * height;
  std::vector<uint16_t> buffer(size);

  for (int row = 0; row < height; row++) {
    int fbRow = row + params.y1;

    memcpy(buffer.data() + row * width,
           fb.mem + fbRow * fb_width + params.x1,
           width * sizeof(uint16_t));
  }

  int writeSize = size * sizeof(uint16_t);
  int written = writeAll(tcpFd, (const char*)buffer.data(), writeSize);
  if (written == -1) {
    perror("Write");
    exit(EXIT_FAILURE);
  }

  return true;
}

// Starts a tcp server and waits for a client to connect.
int
getTcpSocket(int port) {
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    perror("Socket");
    return -1;
  }
  // lose the pesky "Address already in use" error message
  int yes = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
    perror("setsockopt");
    return -1;
  }

  sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
    perror("bind");
    return -1;
  }

  if (listen(listenfd, 1) != 0) {
    perror("Listen");
    return -1;
  }

  int res = accept(listenfd, nullptr, nullptr);
  close(listenfd);

  return res;
}

struct libevdev_uinput*
makeDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "Fake-stylus");
  libevdev_enable_event_type(dev, EV_SYN);

  libevdev_enable_event_type(dev, EV_KEY);
  libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_PEN, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_RUBBER, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_TOUCH, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_STYLUS, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_STYLUS2, nullptr);

  libevdev_enable_event_type(dev, EV_ABS);
  struct input_absinfo info = {};
  info.minimum = 0;
  info.maximum = 20966;
  info.resolution = 100;
  libevdev_enable_event_code(dev, EV_ABS, ABS_X, &info);
  info.minimum = 0;
  info.maximum = 15725;
  libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &info);
  info.resolution = 0;

  info.minimum = 0;
  info.maximum = 4095;
  libevdev_enable_event_code(dev, EV_ABS, ABS_PRESSURE, &info);

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_DISTANCE, &info);

  info.minimum = -9000;
  info.maximum = 9000;
  libevdev_enable_event_code(dev, EV_ABS, ABS_TILT_X, &info);
  libevdev_enable_event_code(dev, EV_ABS, ABS_TILT_Y, &info);

  struct libevdev_uinput* uidev;
  auto err = libevdev_uinput_create_from_device(
    dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
  if (err != 0) {
    perror("uintput");
    std::cerr << "Error making uinput device\n";
    return nullptr;
  }

  std::cout << "Added uintput device\n";

  return uidev;
}

struct Input {
  int32_t x;
  int32_t y;
  int32_t type; // 1 = down, 2 = up
};

void
handleInput(const Input& inp, libevdev_uinput& dev) {
  constexpr auto wacom_width = 15725;
  constexpr auto wacom_height = 20967;

  constexpr auto screen_width = 1404;
  constexpr auto screen_height = 1872;

  int x = float(inp.x) * wacom_width / screen_width;
  int y = wacom_height - float(inp.y) * wacom_height / screen_height;

  libevdev_uinput_write_event(&dev, EV_ABS, ABS_X, y);
  libevdev_uinput_write_event(&dev, EV_ABS, ABS_Y, x);

  if (inp.type != 0) {
    auto value = inp.type == 1 ? 1 : 0;
    libevdev_uinput_write_event(&dev, EV_KEY, BTN_TOOL_PEN, value);
    libevdev_uinput_write_event(&dev, EV_KEY, BTN_TOUCH, value);
  }

  libevdev_uinput_write_event(&dev, EV_SYN, SYN_REPORT, 0);
}

} // namespace

int
main() {
  setupExitHandler();
  std::cout << "Mem: " << fb.mem << "\n";

  auto* uinputDev = makeDevice();

  const char* socketAddr = DEFAULT_SOCK_ADDR;

  // Setup server socket.
  if (unlink(socketAddr) != 0) {
    perror("Socket unlink");
  }

  Socket serverSock;
  if (!serverSock.bind(socketAddr)) {
    return EXIT_FAILURE;
  }

  // Open shared memory
  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }
  memset(fb.mem, 0xff, fb_size);

  std::cout << "Waiting for connection on port 8888\n";
  int tcpFd = getTcpSocket(8888);
  if (tcpFd == -1) {
    std::cerr << "Unable to get TCP connection\n";
    return EXIT_FAILURE;
  }

  std::cout << "SWTCON :p!\n";
  while (running) {

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(tcpFd, &fds);
    FD_SET(serverSock.sock, &fds);

    auto res = select(
      std::max(tcpFd, serverSock.sock) + 1, &fds, nullptr, nullptr, nullptr);
    if (res < 0) {
      perror("Select");
      break;
    }

    if (FD_ISSET(serverSock.sock, &fds)) {
      auto [msg, addr] = serverSock.recvfrom<UpdateParams>();
      if (!msg.has_value()) {
        continue;
      }

      bool res = doUpdate(*msg, tcpFd);

      // Don't log Stroke updates
      if (msg->flags != 4) {
        std::cerr << "UPDATE " << *msg << ": " << res << "\n";
      }

      serverSock.sendto(res, addr);
    }

    if (FD_ISSET(tcpFd, &fds)) {

      Input inp;
      auto len = read(tcpFd, &inp, sizeof(Input));
      if (len != sizeof(Input)) {
        perror("Reading input");
        break;
      }

      handleInput(inp, *uinputDev);
    }
  }

  return 0;
}
