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
const char* termName = "yaft-256color";

AppContext* globalCtx = nullptr;

void
sigHandler(int signo) {
  if (signo == SIGCHLD) {
    if (globalCtx != nullptr) {
      globalCtx->stop();
    }
    wait(nullptr);
  }
}

void
initSignalHandler(AppContext& ctx) {
  globalCtx = &ctx;

  struct sigaction sigact{};
  memset(&sigact, 0, sizeof(struct sigaction));
  sigact.sa_handler = sigHandler;
  sigact.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sigact, nullptr);
}

bool
forkAndExec(int* master,
            const char* cmd,
            char* const argv[],
            int lines,
            int cols) {
  pid_t pid = 0;
  struct winsize ws{};
  ws.ws_row = lines;
  ws.ws_col = cols;
  /* XXX: this variables are UNUSED (man tty_ioctl),
          but useful for calculating terminal cell size */
  ws.ws_ypixel = CELL_HEIGHT * lines;
  ws.ws_xpixel = CELL_WIDTH * cols;

  pid = eforkpty(master, nullptr, nullptr, &ws);
  if (pid < 0) {
    return false;
  }
  if (pid == 0) { /* child */
    setenv("TERM", termName, 1);
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
YaftState::init(rmlib::AppContext& ctx, const rmlib::BuildContext& /*unused*/) {
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

  if (!forkAndExec(&term->fd,
                   getWidget().cmd,
                   getWidget().argv,
                   term->lines,
                   term->cols)) {
    puts("Failed to fork!");
    std::exit(EXIT_FAILURE);
  }

  ctx.listenFd(term->fd, [this] {
    std::array<char, 512> buf{};
    auto size = read(term->fd, buf.data(), buf.size());

    // Only update if the buffer isn't full. Otherwise more data is comming
    // probably.
    if (size != int(buf.size())) {
      setState([&](auto& self) {
        parse(self.term.get(), reinterpret_cast<uint8_t*>(buf.data()), size);
      });
    } else {
      parse(term.get(), reinterpret_cast<uint8_t*>(buf.data()), size);
    }
  });

  // listen to stdin in debug.
  if constexpr (USE_STDIN != 0U) {
    ctx.listenFd(STDIN_FILENO, [this] {
      std::array<char, 512> buf{};
      auto size = read(STDIN_FILENO, buf.data(), buf.size());
      if (size > 0) {
        write(term->fd, buf.data(), size);
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
        [](auto& self) { self.isLandscape = device::isPogoConnected(); });
    });
  } else {
    isLandscape = config.orientation == YaftConfig::Orientation::Landscape;
  }
}

YaftState
Yaft::createState() {
  return {};
}
