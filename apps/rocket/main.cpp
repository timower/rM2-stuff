#include "Launcher.h"

#include <UI.h>

#ifdef __linux__
#include <systemd/sd-daemon.h>
#endif

using namespace rmlib;

namespace {
constexpr std::array static_app_paths = { "/opt/etc/draft", "/etc/draft" };

std::vector<AppDescription>
readAppDescriptions() {
  const static auto app_paths = [] {
    std::vector<std::string> paths;
    std::transform(static_app_paths.begin(),
                   static_app_paths.end(),
                   std::back_inserter(paths),
                   [](const auto* str) { return std::string(str); });

    if (const auto* home = getenv("HOME"); home != nullptr) {
      paths.push_back(std::string(home) + "/.config/draft");
    }

    return paths;
  }();

  std::vector<AppDescription> appDescriptions;
  for (const auto& appsPath : app_paths) {
    auto decriptions = readAppFiles(appsPath);
    std::move(decriptions.begin(),
              decriptions.end(),
              std::back_inserter(appDescriptions));
  }
  return appDescriptions;
}
} // namespace

int
main(int argc, char* argv[]) {
  // Connect ctrl client here to make sure rm2fb server is started.
  ControlClient ctlClient;
  unistdpp::fatalOnError(ctlClient.init(),
                         "Failed to connnect to rm2fb.control: ");
  unistdpp::fatalOnError(ctlClient.getClients(), "Failed to list clients: ");

  // Then wait for uinput devices if requested.
  if (getenv("ROCKET_WAIT_FOR_INPUT") != nullptr) {
    input::InputManager manager;
    unistdpp::fatalOnError(manager.openAll());
    while (manager.numDevices() == 0) {
      std::cerr << "Waiting for any device!\n";
      unistdpp::fatalOnError(manager.waitForInput({}));
    }
  }

  // Finally, tell systemd we're ready.
#ifdef __linux__
  sd_notify(0, "READY=1");
#endif

  unistdpp::fatalOnError(runApp(
    LauncherWidget(ctlClient, readAppDescriptions), {}, /*clearOnExit=*/true));
  return EXIT_SUCCESS;
}
