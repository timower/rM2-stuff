#include "YaftWidget.h"

// libYaft
#include "conf.h"
#include "parse.h"
#include "terminal.h"
#include "yaft.h"

#include "util.h"

#include <Device.h>

#include <sstream>

using namespace rmlib;

namespace {
const char* term_name = "yaft-256color";

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

  pid = eforkpty(master, NULL, NULL, &ws);
  if (pid < 0) {
    return false;
  } else if (pid == 0) { /* child */
    setenv("TERM", term_name, 1);
    execvp(cmd, argv);
    /* never reach here */
    exit(EXIT_FAILURE);
  }
  return true;
}
} // namespace

YaftState::~YaftState() {
  if (term) {
    term_die(term.get());
  }
}

void
YaftState::logTerm(std::string_view str) {
  parse(term.get(), reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void
YaftState::init(rmlib::AppContext& ctx, const rmlib::BuildContext&) {
  term = std::make_unique<terminal_t>();

  // term_init needs the maximum size of the terminal.
  int maxSize = std::max(ctx.getFbCanvas().width(), ctx.getFbCanvas().height());
  if (!term_init(term.get(), maxSize, maxSize)) {
    std::cout << "Error init term\n";
    ctx.stop();
    return;
  }

  if (const auto& err = getWidget().configError; err.has_value()) {
    logTerm(err->msg);
  }

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

  checkLandscape(ctx);
  ctx.onDeviceUpdate([this, &ctx] { modify().checkLandscape(ctx); });
}

void
YaftState::checkLandscape(rmlib::AppContext& ctx) {
  const auto& config = getWidget().config;

  if (config.orientation == YaftConfig::Orientation::Auto) {

    // The pogo state updates after a delay, so wait 100 ms before checking.
    pogoTimer = ctx.addTimer(std::chrono::milliseconds(100), [this] {
      setState(
        [](auto& self) { self.isLandscape = device::IsPogoConnected(); });
    });
  } else {
    isLandscape = config.orientation == YaftConfig::Orientation::Landscape;
  }
}

YaftState
Yaft::createState() const {
  return {};
}
