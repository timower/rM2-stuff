#include "Messages.h"

#include <ControlSocket.h>
#include <ImageHook.h>
#include <Message.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <unistdpp/poll.h>
#include <unistdpp/socket.h>
#include <unistdpp/unistdpp.h>

#include <libevdev/libevdev-uinput.h>

using namespace unistdpp;

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
struct libevdev_uinput*
makeWacomDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "Wacom I2C Digitizer");
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

  struct libevdev_uinput* uidev = nullptr;
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

struct libevdev_uinput*
makeTouchDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "pt_mt");
  libevdev_enable_event_type(dev, EV_SYN);

  libevdev_enable_event_type(dev, EV_KEY);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F1, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F2, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F3, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F4, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F5, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F6, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F7, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F8, nullptr);

  libevdev_enable_event_type(dev, EV_REL);
  libevdev_enable_event_type(dev, EV_ABS);
  struct input_absinfo info = {};
  info.resolution = 0;

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_DISTANCE, &info);

  info.minimum = 0;
  info.maximum = 31;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_SLOT, &info);

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, &info);
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TOUCH_MINOR, &info);

  info.minimum = -127;
  info.maximum = 127;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_ORIENTATION, &info);

  info.minimum = 0;
  info.maximum = 1403;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_X, &info);
  info.minimum = 0;
  info.maximum = 1871;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_Y, &info);

  info.minimum = 0;
  info.maximum = 1;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TOOL_TYPE, &info);

  info.minimum = 0;
  info.maximum = 65535;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID, &info);

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_PRESSURE, &info);

  struct libevdev_uinput* uidev = nullptr;
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

struct libevdev_uinput*
makeButtonDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "30371337.snvs:snvs-powerkey");
  libevdev_enable_event_type(dev, EV_SYN);

  libevdev_enable_event_type(dev, EV_KEY);
  libevdev_enable_event_code(dev, EV_KEY, KEY_POWER, nullptr);

  struct libevdev_uinput* uidev = nullptr;
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

bool
doUpdate(const FD& fd, const UpdateParams& params) {
  static_assert(sizeof(UpdateParams) == sizeof(Params), "Params wrong size?");
  if (auto res = fd.writeAll(params); !res) {
    std::cerr << "Error writing: " << toString(res.error()) << "\n";
    exit(EXIT_FAILURE);
  }

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
  auto res = fd.writeAll(buffer.data(), writeSize);
  if (!res) {
    std::cerr << "Error writing: " << toString(res.error()) << "\n";
    exit(EXIT_FAILURE);
  }

  return true;
}

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

// Starts a tcp server and waits for a client to connect.
Result<FD>
getTcpSocket(int port) {
  auto listenfd = TRY(unistdpp::socket(AF_INET, SOCK_STREAM, 0));

  // lose the pesky "Address already in use" error message
  int yes = 1;
  TRY(unistdpp::setsockopt(
    listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  TRY(unistdpp::bind(listenfd, Address::fromHostPort(INADDR_ANY, port)));
  TRY(unistdpp::listen(listenfd, 1));

  // return unistdpp::accept(listenfd, nullptr, nullptr);
  return listenfd;
}
} // namespace

int
main() {
  setupExitHandler();
  std::cout << "Mem: " << fb.mem << "\n";

  auto* buttonDev = makeButtonDevice();
  auto* uinputDev = makeWacomDevice();
  auto* touchDev = makeTouchDevice();

  const char* socketAddr = DEFAULT_SOCK_ADDR;

  // Setup server socket.
  if (unlink(socketAddr) != 0) {
    perror("Socket unlink");
  }

  ControlSocket serverSock;
  if (!serverSock.init(socketAddr)) {
    return EXIT_FAILURE;
  }

  // Open shared memory
  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }
  memset(fb.mem, UINT8_MAX, fb_size);

  constexpr auto port = 8888;
  std::cout << "Listening for TCP connections on: " << port << "\n";
  auto tcpFd = getTcpSocket(port);
  if (!tcpFd) {
    std::cerr << "Unable to get TCP connection\n";
    return EXIT_FAILURE;
  }

  constexpr auto tcp_poll_idx = 0;
  constexpr auto server_poll_idx = 1;
  constexpr auto fixed_poll_fds = server_poll_idx + 1;

  std::vector<unistdpp::FD> clientSocks;

  std::cout << "SWTCON :p!\n";
  while (running) {
    std::vector<pollfd> pollfds(fixed_poll_fds);
    pollfds.reserve(clientSocks.size() + fixed_poll_fds);
    pollfds[tcp_poll_idx] = waitFor(*tcpFd, Wait::Read);
    pollfds[server_poll_idx] = waitFor(serverSock.sock, Wait::Read);
    std::transform(
      clientSocks.begin(),
      clientSocks.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client, Wait::Read); });

    auto res = unistdpp::poll(pollfds);
    if (!res) {
      std::cerr << "Poll error: " << toString(res.error()) << "\n";
      break;
    }

    if (canRead(pollfds[tcp_poll_idx])) {
      auto clientOrErr = unistdpp::accept(*tcpFd, nullptr, nullptr);
      if (clientOrErr) {
        std::cout << "Got new client!\n";
        clientSocks.emplace_back(std::move(*clientOrErr));
        pollfds.push_back(waitFor(clientSocks.back(), Wait::Read));
      } else if (!clientOrErr) {
        std::cerr << "Client accept errror: " << toString(clientOrErr.error())
                  << "\n";
      }
    }

    if (canRead(pollfds[server_poll_idx])) {
      auto result =
        serverSock.recvfrom<UpdateParams>().and_then([&](auto result) {
          auto [msg, addr] = result;

          auto res = false;
          for (const auto& clientSock : clientSocks) {
            res |= doUpdate(clientSock, msg);
          }

          // Don't log Stroke updates
          if (msg.flags != 4) {
            std::cerr << "UPDATE " << msg << ": " << res << "\n";
          }

          return serverSock.sendto(res, addr);
        });
      if (!result) {
        std::cerr << "Error update: " << toString(result.error()) << "\n";
      }
    }

    for (size_t idx = fixed_poll_fds; idx < pollfds.size(); idx++) {
      if (!canRead(pollfds[idx])) {
        continue;
      }

      auto& clientSock = clientSocks[idx - fixed_poll_fds];
      clientSock.readAll<Input>()
        .map([&](auto input) {
          if (uinputDev != nullptr) {
            handleInput(input, *uinputDev);
          }
        })
        .or_else([&](auto err) {
          std::cerr << "Reading input: " << toString(err) << "\n";
          if (err == unistdpp::FD::eof_error) {
            clientSock.close();
          }
        });
    }

    // Remove closed clients
    clientSocks.erase(
      std::remove_if(clientSocks.begin(),
                     clientSocks.end(),
                     [](const auto& sock) { return !sock.isValid(); }),
      clientSocks.end());
  }

  if (buttonDev != nullptr) {
    libevdev_uinput_destroy(buttonDev);
  }

  if (touchDev != nullptr) {
    libevdev_uinput_destroy(touchDev);
  }

  if (uinputDev != nullptr) {
    libevdev_uinput_destroy(uinputDev);
  }

  return 0;
}
