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
  if ((msg.flags & 4) == 0) {
    std::cout << "Got msg: "
              << "{ { " << msg.x1 << ", " << msg.y1 << "; " << msg.x2 << ", "
              << msg.y2 << " }, wave: " << msg.waveform
              << " flags: " << msg.flags << " }\n";
  }

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
    if (newWidget.canvas.getMemory() != widget->canvas.getMemory()) {
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

    copy(canvas, rect.topLeft, widget->canvas, widget->canvas.rect());

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

    getWidget().onInput(touchEv);
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
      return;
    }
    setState([&](auto& self) {
      auto [updateRegion, updateCanvas] = std::move(*msgOrErr);
      copy(self.memCanvas.canvas,
           updateRegion.region.topLeft,
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

  auto build(AppContext& ctx, const BuildContext& buildCtx) const {
    auto result = FB(memCanvas.canvas, *pendingUpdates, [this](const auto& ev) {
      auto type = getType(ev);
      if (type != 0) {
        std::cout << "Touch @ " << ev.location << "\n";
      }

      ClientMsg input =
        Input{ ev.location.x, ev.location.y, type, /* touch */ true };
      auto res = sendMessage(socket, input);
      if (!res) {
        std::cerr << "Error writing: " << to_string(res.error()) << "\n";
      }
    });
    return result;
  }

private:
  std::unique_ptr<std::vector<UpdateRegion>> pendingUpdates;
  MemoryCanvas memCanvas;
  FD socket;
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

  fatalOnError(runApp(Rm2fb(addrArg, port)));

  return 0;
}
