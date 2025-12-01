#include <rm2fb/ControlSocket.h>

namespace {

void
usage() {
  std::cerr << "Usage:\n";
  std::cerr << " rm2fbctl list\n";
  std::cerr << " rm2fbctl switch <pid>\n";
  std::exit(EXIT_FAILURE);
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
      std::cout << (client.active ? "*" : " ") << client.pid << " "
                << client.name << "\n";
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
