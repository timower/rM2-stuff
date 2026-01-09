#include "Socket.h"

// rm2fb
#include <Message.h>

// rMlib
#include <Canvas.h>

#include <unistdpp/socket.h>

#include <iostream>
#include <vector>

using namespace unistdpp;

namespace {
constexpr auto tap_wait = 1000; // us

bool
doScreenshot(unistdpp::FD& sock, std::vector<std::string_view> args) {
  sendMessage(sock, ClientMsg(GetUpdate{}));
  auto msgOrErr = sock.readAll<UpdateParams>();
  if (!msgOrErr) {
    std::cerr << "Error reading: " << to_string(msgOrErr.error()) << "\n";
    return false;
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
  auto bufSize = width * height * sizeof(uint16_t);
  std::vector<uint8_t> buffer(bufSize);

  auto res = sock.readAll(buffer.data(), bufSize);
  if (!res) {
    std::cerr << "Error reading: " << to_string(res.error()) << "\n";
    return false;
  }

  auto canvas = rmlib::Canvas(buffer.data(), width, height, sizeof(uint16_t));

  const auto name =
    args.empty() ? std::string_view("screenshot.png") : args.front();
  fatalOnError(canvas.writeImage(name.data()));

  return true;
}

bool
doInput(unistdpp::FD& sock, int x, int y, bool touch) {
  auto input = Input{
    .x = x,
    .y = y,
    .type = Input::Down,
    .touch = touch,
  };

  fatalOnError(sendMessage(sock, ClientMsg(input)));
  usleep(tap_wait);

  input.type = Input::Up;
  fatalOnError(sendMessage(sock, ClientMsg(input)));
  usleep(tap_wait);

  return true;
}

bool
doMovePen(unistdpp::FD& sock, std::vector<std::string_view> args) {
  if (args.size() != 3) {
    std::cerr << "Move requires 3 args\n";
    return false;
  }

  auto input = Input{
    .x = atoi(args[1].data()),
    .y = atoi(args[2].data()),
    .type = Input::Move,
    .touch = args[0] == "touch",
  };

  fatalOnError(sendMessage(sock, ClientMsg(input)));
  usleep(tap_wait);

  return true;
}

bool
doTouch(unistdpp::FD& sock, std::vector<std::string_view> args) {
  if (args.size() != 2) {
    std::cerr << "Touch requires 2 args, x and y\n";
    return false;
  }

  return doInput(sock, atoi(args[0].data()), atoi(args[1].data()), true);
}

bool
doPen(unistdpp::FD& sock, std::vector<std::string_view> args) {
  if (args.size() != 2) {
    std::cerr << "Touch requires 2 args, x and y\n";
    return false;
  }

  return doInput(sock, atoi(args[0].data()), atoi(args[1].data()), false);
}

bool
doPower(unistdpp::FD& sock, std::vector<std::string_view> args) {
  auto input = PowerButton{ .down = true };

  fatalOnError(sendMessage(sock, ClientMsg(input)));
  usleep(tap_wait);

  input.down = false;
  fatalOnError(sendMessage(sock, ClientMsg(input)));
  usleep(tap_wait);

  return true;
}

const std::unordered_map<std::string_view, decltype(&doScreenshot)> actions = {
  {
    { "screenshot", doScreenshot },
    { "touch", doTouch },
    { "pen", doPen },
    { "power", doPower },
    { "move", doMovePen },
  }
};
} // namespace

int
main(int argc, char* argv[]) {
  if (argc < 4) {
    // NOLINTNEXTLINE
    std::cout << "Usage: " << argv[0] << " <ip> <port> <action> ... \n";
    return EXIT_FAILURE;
  }

  std::string_view action = argv[3]; // NOLINT
  if (actions.count(action) == 0) {
    std::cout << "Unknown action: " << action << "\n";
    return EXIT_FAILURE;
  }
  const auto actionFn = actions.find(action)->second;

  int port = atoi(argv[2]);                 // NOLINT
  auto sock = getClientSock(argv[1], port); // NOLINT
  if (!sock.has_value()) {
    std::cout << "Couldn't get tcp socket: " << to_string(sock.error()) << "\n";
    return EXIT_FAILURE;
  }

  std::vector<std::string_view> args;
  for (int i = 4; i < argc; i++) {
    args.emplace_back(argv[i]); // NOLINT
  }

  return actionFn(*sock, std::move(args)) ? EXIT_SUCCESS : EXIT_FAILURE;
}
