#include "SharedBuffer.h"
#include "Socket.h"

// rm2fb
#include <Message.h>

// unistdpp
#include <unistdpp/socket.h>

// rmlib
#include <FrameBuffer.h>
#include <Input.h>
#include <UI.h>
#include <UI/Util.h>

#include <arpa/inet.h>
#include <cstdlib>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace unistdpp;
using namespace rmlib;
using namespace rmlib::input;

namespace {

constexpr auto header_size = 64;

Input::Action
getType(const PenEvent& touchEv) {
  if (touchEv.isDown()) {
    return Input::Down;
  }
  if (touchEv.isUp()) {
    return Input::Up;
  }
  return Input::Move;
}

struct UpdateMsg {
  rmlib::UpdateRegion updateRegion;
  rmlib::MemoryCanvas memCanvas;
};

Result<UpdateMsg>
readUpdate(const FD& sock) {
  auto msg = TRY(sock.readAll<UpdateParams>());
  // if ((msg.flags & 4) == 0) {
  std::cout << "Got msg: " << msg << "\n";
  //}

  rmlib::Rect region = { .topLeft = { msg.x1, msg.y1 },
                         .bottomRight = { msg.x2, msg.y2 } };
  rmlib::MemoryCanvas memCanvas(
    region.width(), region.height(), sizeof(uint16_t));

  TRY(sock.readAll(memCanvas.memory.get(), memCanvas.canvas.totalSize()));

  return UpdateMsg{ rmlib::UpdateRegion(region,
                                        (rmlib::fb::Waveform)msg.waveform,
                                        (rmlib::fb::UpdateFlags)msg.flags),
                    std::move(memCanvas) };
}

class BetterButton : public StatefulWidget<BetterButton> {
  class State : public StateBase<BetterButton> {
  public:
    auto build(AppContext& ctx, const BuildContext& buildCtx) const {
      bool isDown = getWidget().alwaysEnabled || down;
      return GestureDetector(
        Padding(Border(Border(Text(getWidget().text),
                              Insets::all(2),
                              isDown ? black : white),
                       Insets::all(2)),
                Insets::all(2)),
        Gestures{}
          .onTouchDown([this](auto pos) {
            setState([](auto& self) { self.down = true; });
            if (const auto& widget = getWidget(); widget.onDown) {
              widget.onDown();
            }
          })
          .onTap([this] {
            setState([](auto& self) { self.down = false; });
            getWidget().onClick();
          }));
    }

  private:
    bool down = false;
  };

public:
  BetterButton(std::string text,
               Callback onClick,
               Callback onDown = {},
               bool alwaysEnabled = false)
    : text(std::move(text))
    , onClick(std::move(onClick))
    , onDown(std::move(onDown))
    , alwaysEnabled(alwaysEnabled) {}

  static State createState() { return State{}; }

private:
  std::string text;
  Callback onClick;
  Callback onDown;
  bool alwaysEnabled;
};

class FBRenderObject;

class FB : public Widget<FBRenderObject> {
public:
  using EvCallback = std::function<void(const PenEvent&)>;
  FB(const Canvas& canvas,
     std::vector<UpdateRegion>& updates,
     EvCallback onInput)
    : canvas(canvas), updateRegions(&updates), onInput(std::move(onInput)) {}

  std::unique_ptr<RenderObject> createRenderObject() const;

private:
  friend class FBRenderObject;
  Canvas canvas;
  std::vector<UpdateRegion>* updateRegions = nullptr;
  EvCallback onInput;
};

class FBRenderObject : public LeafRenderObject<FB> {
public:
  using LeafRenderObject<FB>::LeafRenderObject;

  void update(const FB& newWidget) {
    if (newWidget.canvas != widget->canvas) {
      markNeedsDraw(true);
    }
    if (!newWidget.updateRegions->empty()) {
      markNeedsDraw(false);
    }

    widget = &newWidget;
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    const auto w = widget->canvas.width();
    const auto h = widget->canvas.height();

    return Size{ std::clamp(w, constraints.min.width, constraints.max.width),
                 std::clamp(
                   h, constraints.min.height, constraints.max.height) };
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    auto result = UpdateRegion{ rect };

    canvas.copy(rect.topLeft, widget->canvas, widget->canvas.rect());

    if (!isFullDraw()) {
      result = std::accumulate(widget->updateRegions->begin(),
                               widget->updateRegions->end(),
                               UpdateRegion{},
                               std::bit_or<UpdateRegion>{});
      result.region += rect.topLeft;
    }

    widget->updateRegions->clear();
    return result;
  }

  void handleInput(const Event& ev) final {
    if (!std::holds_alternative<PenEvent>(ev)) {
      return;
    }
    const auto& touchEv = std::get<PenEvent>(ev);
    if (!getRect().contains(touchEv.location)) {
      return;
    }

    auto movedEv = touchEv;
    movedEv.location -= getRect().topLeft;
    getWidget().onInput(movedEv);
  }
};

inline std::unique_ptr<RenderObject>
FB::createRenderObject() const {
  return std::make_unique<FBRenderObject>(*this);
}

class Rm2fbState;

class Rm2fb : public StatefulWidget<Rm2fb> {
public:
  static Rm2fbState createState();
  Rm2fb(const char* host, int port) : host(host), port(port) {}

  const char* host;
  int port;
};

class Rm2fbState : public StateBase<Rm2fb> {
public:
  Rm2fbState()
    : StateBase()
    , pendingUpdates(std::make_unique<std::vector<UpdateRegion>>())
    , memCanvas(fb_width, fb_height, sizeof(uint16_t)) {}

  void handleMsg() const {
    auto msgOrErr = readUpdate(socket);
    if (!msgOrErr.has_value()) {
      std::cerr << "Error reading update: " << to_string(msgOrErr.error())
                << "\n";
      if (msgOrErr.error() == FD::eof_error) {
        std::exit(EXIT_FAILURE);
      }
      return;
    }
    setState([&](auto& self) {
      auto [updateRegion, updateCanvas] = std::move(*msgOrErr);
      self.memCanvas.canvas.copy(updateRegion.region.topLeft,
                                 updateCanvas.canvas,
                                 updateCanvas.canvas.rect());
      self.pendingUpdates->push_back(updateRegion);
    });
  }

  void init(AppContext& appCtx, const BuildContext& buildCtx) {
    socket = fatalOnError(getClientSock(getWidget().host, getWidget().port),
                          "Couldn't get tcp socket: ");

    appCtx.listenFd(socket.fd, [this] { handleMsg(); });

    // Get the initial full screen image by sending a GetUpdate message.
    sendMessage(socket, ClientMsg(GetUpdate{}));
  }

  void onInput(const PenEvent& ev) const {
    auto type = getType(ev);
    if (type != 0) {
      std::cout << "Touch @ " << ev.location << "\n";
    }

    ClientMsg input = Input{ ev.location.x, ev.location.y, type, touch };
    auto res = sendMessage(socket, input);
    if (!res) {
      std::cerr << "Error writing: " << to_string(res.error()) << "\n";
    }
  }

  auto header() const {
    return Row(
      BetterButton(
        "X",
        [this] {
          ClientMsg msg = PowerButton{ .down = false };
          fatalOnError(sendMessage(socket, msg));
        },
        [this] {
          ClientMsg msg = PowerButton{ .down = true };
          fatalOnError(sendMessage(socket, msg));
        }),
      Expanded(Text("rM2-FB Emulator")),
      Button(
        "Refresh",
        [this] { fatalOnError(sendMessage(socket, ClientMsg(GetUpdate{}))); }),
      BetterButton(
        "Touch",
        [this] { setState([](auto& self) { self.touch = true; }); },
        {},
        touch),
      BetterButton(
        "Pen",
        [this] { setState([](auto& self) { self.touch = false; }); },
        {},
        !touch));
  }

  auto build(AppContext& ctx, const BuildContext& buildCtx) const {
    return Column(Sized(header(), std::nullopt, header_size),
                  FB(memCanvas.canvas, *pendingUpdates, [this](const auto& ev) {
                    onInput(ev);
                  }));
  }

private:
  std::unique_ptr<std::vector<UpdateRegion>> pendingUpdates;
  MemoryCanvas memCanvas;
  FD socket;
  bool touch = true;
};

Rm2fbState
Rm2fb::createState() {
  return {};
}

} // namespace

int
main(int argc, char* argv[]) {
  const char* programName = argv[0]; // NOLINT
  if (argc != 3) {
    std::cout << "Usage: " << programName << " <ip of server> <port> \n";
    return 1;
  }
  const char* portArg = argv[2]; // NOLINT
  const char* addrArg = argv[1]; // NOLINT

  int port = atoi(portArg);

  fatalOnError(runApp(Cleared(Rm2fb(addrArg, port)),
                      Size{ fb_width, fb_height + header_size }));

  return 0;
}
