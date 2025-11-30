#include "unistdpp/file.h"
#include <rm2fb/ControlSocket.h>

namespace {

void
usage() {
  std::cerr << "Usage:\n";
  std::cerr << " rm2fbctl list\n";
  std::cerr << " rm2fbctl switch <pid>\n";
  std::exit(EXIT_FAILURE);
}

unistdpp::Result<std::string>
getProcName(pid_t pid) {
  auto fd = TRY(unistdpp::open(
    (std::filesystem::path("/proc") / std::to_string(pid) / "cmdline").c_str(),
    O_RDONLY));

  std::array<char, 512> buf;
  auto size = TRY(fd.readAll(buf.data(), buf.size()));
  auto res = std::string(buf.data(), size);

  return std::filesystem::path(res).filename().string();
}

} // namespace

int
main(int argc, char* argv[]) {
  if (argc < 2) {
    usage();
  }

  ControlClient client;
  unistdpp::fatalOnError(client.init(), "rm2fb client init failed");

  std::string_view action = argv[1];
  if (action == "list") {
    auto clients = unistdpp::fatalOnError(client.getClients());
    for (const auto& client : clients) {
      auto name = getProcName(client.pid).value_or("<error>");
      std::cout << (client.active ? "*" : " ") << client.pid << " " << name
                << "\n";
    }

    return EXIT_SUCCESS;
  }

  if (action == "switch" && argc == 3) {
    pid_t pid = atoi(argv[2]);
    unistdpp::fatalOnError(client.switchTo(pid));
    return EXIT_SUCCESS;
  }

  usage();
}
