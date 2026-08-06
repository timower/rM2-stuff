#include "parse.h"
#include "screen.h"
#include "yaft.h"

// rmLib
#include <UI.h>

#include "unistdpp/file.h"

// stdlib
#include <fstream>
#include <unistd.h>

using namespace rmlib;

namespace {

class ReaderState;

class Reader : public rmlib::StatefulWidget<Reader> {
public:
  Reader(const char* path, const char* readyPath = nullptr)
    : filePath(path), readyPath(readyPath) {}

  static ReaderState createState();

private:
  friend class ReaderState;
  const char* filePath;
  // Touched once listenFd() below is registered, i.e. once it's safe for a
  // writer to append to filePath without racing our lseek(SEEK_END) (a
  // write landing before that point is otherwise lost forever).
  const char* readyPath;
};

class ReaderState : public rmlib::StateBase<Reader> {
public:
  void logTerm(std::string_view str) {
    parse(term.get(), reinterpret_cast<const uint8_t*>(str.data()), str.size());
  }

  void putTerm(const char* data, std::size_t size) {
    std::vector<uint8_t> res;
    res.reserve(size);

    const auto* end = data + size;
    const auto* comma = std::find(data, end, ';');

    for (const auto* it = comma == end ? data : comma + 1; it < end; it++) {
      auto c = *it;
      if (c == '\n') {
        res.push_back('\r');
      }
      res.push_back(c);
    }

    parse(term.get(), res.data(), res.size());
  }

  void init(rmlib::AppContext& ctx, const rmlib::BuildContext& buildCtx) {
    term = std::make_unique<terminal_t>();

    int maxSize =
      std::max(ctx.getFbCanvas().width(), ctx.getFbCanvas().height());
    if (!term_init(term.get(), maxSize, maxSize)) {
      std::cerr << "Error init term\n";
      ctx.stop();
      return;
    }

    fileFd =
      unistdpp::fatalOnError(unistdpp::open(getWidget().filePath, O_RDONLY));
    unistdpp::fatalOnError(unistdpp::lseek(fileFd, 0, SEEK_END));

    ctx.listenFd(fileFd.fd, [this, &ctx] {
      std::array<char, 512> buf{};
      auto size = read(fileFd.fd, buf.data(), buf.size());
      if (size < 0) {
        logTerm(strerror(errno));
        perror("Read error!");
        std::exit(2);
      }
      if (size == 0) {
        ctx.stop();
      } else if (size == buf.size()) {
        putTerm(buf.data(), size);
      } else {
        setState([&](auto& self) { self.putTerm(buf.data(), size); });
      }
    });

    if (getWidget().readyPath != nullptr) {
      std::ofstream(getWidget().readyPath).close();
    }
  }

  auto build(rmlib::AppContext& ctx, const rmlib::BuildContext& _) const {
    return Screen(term.get(), /* isLandscape */ false, /* autoRefresh */ 0);
  }

private:
  std::unique_ptr<terminal_t> term;
  unistdpp::FD fileFd;
};

ReaderState
Reader::createState() {
  return {};
}

} // namespace

int
main(int argc, char* argv[]) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <file> [ready-file]\n";
    return 1;
  }

  /* for wcwidth() */
  char* locale = nullptr;
  if ((locale = setlocale(LC_ALL, "en_US.UTF-8")) == nullptr &&
      (locale = setlocale(LC_ALL, "")) == nullptr) {
    std::cerr << "setlocale failed\n";
  }
  std::cerr << "Locale is: " << locale << "\n";

  unistdpp::fatalOnError(
    runApp(Reader(argv[1], argc == 3 ? argv[2] : nullptr)));

  return 0;
}
