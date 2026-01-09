#include "YaftWidget.h"

// libYaft
#include "conf.h"
#include "parse.h"
#include "terminal.h"
#include "yaft.h"

#include "util.h"

#include <Device.h>

#include <sys/inotify.h>

#include <unistdpp/file.h>

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

YaftConfigAndError
Yaft::getConfigAndError() const {
  if (std::holds_alternative<YaftConfigAndError>(configOrPath)) {
    return std::get<YaftConfigAndError>(configOrPath);
  }

  return loadConfigOrMakeDefault(std::get<std::filesystem::path>(configOrPath));
}

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
    std::cerr << "Error init term\n";
    ctx.stop();
    return;
  }

  auto cfgAndError = getWidget().getConfigAndError();
  config = std::move(cfgAndError.config);

  if (const auto& err = cfgAndError.err; err.has_value()) {
    logTerm(err->msg);
  }

  if (std::holds_alternative<std::filesystem::path>(getWidget().configOrPath)) {
    watchPath = std::get<std::filesystem::path>(getWidget().configOrPath);
    inotifyFd = unistdpp::FD(inotify_init());
    if (inotifyFd.isValid()) {
      inotifyWd = inotify_add_watch(inotifyFd.fd,
                                    watchPath.parent_path().c_str(),
                                    IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE);

      ctx.listenFd(inotifyFd.fd, [this, &ctx] { readInotify(ctx); });
    }
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
  if (config.autoRotate) {
    const auto hasKeyboard =
      ctx.getInputManager().getBaseDevices().pogoKeyboard != nullptr;
    hideKeyboard = hasKeyboard;
    rotation = hasKeyboard ? Rotation::Clockwise : config.rotation;
  } else {
    rotation = config.rotation;
  }
}

void
YaftState::readInotify(rmlib::AppContext& ctx) const {
  union {
    std::array<char, 4096> buf;
    inotify_event ev;
  } evUnion;

  auto read = unistdpp::read(inotifyFd, &evUnion, sizeof(evUnion));
  if (!read.has_value()) {
    std::cerr << "inotify read failed: " << to_string(read.error()) << "\n";
    return;
  }

  bool shouldReload = false;
  const inotify_event* event;
  for (char* ptr = evUnion.buf.begin(); ptr < evUnion.buf.begin() + *read;
       ptr += sizeof(struct inotify_event) + event->len) {
    event = (const struct inotify_event*)ptr;

    if (event->wd == inotifyWd && event->name == watchPath.filename()) {
      shouldReload = true;
    }
  }

  if (!shouldReload) {
    return;
  }

  std::cerr << "inotify: Config updated, reloading\n";
  setState([&](auto& self) {
    auto cfgAndErr = loadConfig(self.watchPath);
    if (cfgAndErr.err.has_value()) {
      self.logTerm(cfgAndErr.err->msg);
    }

    self.config = cfgAndErr.config;
    self.checkLandscape(ctx);
  });
}

YaftState
Yaft::createState() {
  return {};
}
