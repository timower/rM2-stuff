// libYaft
#include "conf.h"
#include "parse.h"
#include "terminal.h"
#include "yaft.h"

// yaft(2)
#include "config.h"
#include "keyboard.h"
#include "layout.h"
#include "screen.h"

// rmLib
#include <Device.h>
#include <UI.h>

// stdlib
#include <sstream>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

using namespace rmlib;

namespace {
const char* term_name = "yaft-256color";
const char* shell_cmd = "/bin/bash";

AppContext* globalCtx = nullptr;

void
sig_handler(int signo) {
  if (signo == SIGCHLD) {
    if (globalCtx != nullptr) {
      globalCtx->stop();
    }
    wait(NULL);
  }
}

void
initSignalHandler(AppContext& ctx) {
  globalCtx = &ctx;

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(struct sigaction));
  sigact.sa_handler = sig_handler;
  sigact.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sigact, NULL);
}

bool
fork_and_exec(int* master,
              const char* cmd,
              char* const argv[],
              int lines,
              int cols) {
  pid_t pid;
  struct winsize ws;
  ws.ws_row = lines;
  ws.ws_col = cols;
  /* XXX: this variables are UNUSED (man tty_ioctl),
          but useful for calculating terminal cell size */
  ws.ws_ypixel = CELL_HEIGHT * lines;
  ws.ws_xpixel = CELL_WIDTH * cols;

  pid = forkpty(master, NULL, NULL, &ws);
  if (pid < 0)
    return false;
  else if (pid == 0) { /* child */
    setenv("TERM", term_name, 1);
    execvp(cmd, argv);
    /* never reach here */
    exit(EXIT_FAILURE);
  }
  return true;
}

class YaftState;

class Yaft : public StatefulWidget<Yaft> {
public:
  Yaft(const char* cmd, char* const argv[]) : cmd(cmd), argv(argv) {}

  YaftState createState() const;

private:
  friend class YaftState;
  const char* cmd;
  char* const* argv;
};

class YaftState : public StateBase<Yaft> {
public:
  /// Logs the given string to the terminal console.
  void logTerm(std::string_view str) {
    parse(term.get(), reinterpret_cast<const uint8_t*>(str.data()), str.size());
  }

  YaftConfig getConfig() {
    auto cfgOrErr = loadConfig();

    if (cfgOrErr.isError()) {
      const auto& err = cfgOrErr.getError();
      if (err.type == YaftConfigError::Missing) {
        logTerm("No config, creating new one\r\n");
        saveDefaultConfig();
      } else {
        std::stringstream ss;
        ss << "Config syntax error: " << err.msg << "\r\n";
        logTerm(ss.str());
      }

      return YaftConfig::getDefault();
    }

    return *cfgOrErr;
  }

  void init(AppContext& ctx, const BuildContext&) {
    term = std::make_unique<terminal_t>();

    // term_init needs the maximum size of the terminal.
    int maxSize =
      std::max(ctx.getFbCanvas().width(), ctx.getFbCanvas().height());
    if (!term_init(term.get(), maxSize, maxSize)) {
      std::cout << "Error init term\n";
      ctx.stop();
      return;
    }

    config = getConfig();

    initSignalHandler(ctx);

    if (!fork_and_exec(&term->fd,
                       getWidget().cmd,
                       getWidget().argv,
                       term->lines,
                       term->cols)) {
      puts("Failed to fork!");
      std::exit(EXIT_FAILURE);
    }

    ctx.listenFd(term->fd, [this] {
      std::array<char, 512> buf;
      auto size = read(term->fd, &buf[0], buf.size());

      // Only update if the buffer isn't full. Otherwise more data is comming
      // probably.
      if (size != buf.size()) {
        setState([&](auto& self) {
          parse(self.term.get(), reinterpret_cast<uint8_t*>(&buf[0]), size);
        });
      } else {
        parse(term.get(), reinterpret_cast<uint8_t*>(&buf[0]), size);
      }
    });

    // listen to stdin in debug.
    if constexpr (USE_STDIN) {
      ctx.listenFd(STDIN_FILENO, [this] {
        std::array<char, 512> buf;
        auto size = read(STDIN_FILENO, &buf[0], buf.size());
        if (size > 0) {
          write(term->fd, &buf[0], size);
        }
      });
    }

    if (config.orientation == YaftConfig::Orientation::Auto) {
      isLandscape = device::IsPogoConnected();
      ctx.onDeviceUpdate([this, &ctx] {
        // The pogo state updates after a delay, so wait 100 ms before checking.
        pogoTimer = ctx.addTimer(std::chrono::milliseconds(100), [this] {
          setState(
            [](auto& self) { self.isLandscape = device::IsPogoConnected(); });
        });
      });
    } else {
      isLandscape = config.orientation == YaftConfig::Orientation::Landscape;
    }
  }

  auto build(AppContext& ctx, const BuildContext& buildCtx) const {
    const auto& layout = [this]() -> const Layout& {
      if (isLandscape) {
        return empty_layout;
      }

      if (hidden) {
        return hidden_layout;
      }

      return *config.layout;
    }();

    return Column(Expanded(Screen(term.get(), isLandscape, config.autoRefresh)),
                  Keyboard(term.get(), layout, *config.keymap, [this](int num) {
                    setState([](auto& self) { self.hidden = !self.hidden; });
                  }));
  }

private:
  std::unique_ptr<terminal_t> term;
  TimerHandle pogoTimer;

  YaftConfig config;

  bool hidden = false;
  bool isLandscape = false;
};

YaftState
Yaft::createState() const {
  return {};
}

} // namespace

int
main(int argc, char* argv[]) {
  static const char* shell_args[3] = { shell_cmd, "-l", NULL };

  if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */ {
    std::cout << "setlocale failed\n";
  }

  const char* cmd;
  char* const* args;
  if (argc > 1) {
    cmd = argv[1];
    args = argv + 1;
  } else {
    cmd = shell_args[0];
    args = const_cast<char* const*>(shell_args);
  }

  runApp(Yaft(cmd, args));
  return 0;
}
