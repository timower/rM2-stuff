#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <UI/AppContext.h>
#include <UI/DynamicWidget.h>
#include <UI/Text.h>

#include <filesystem>

#include <SDL_events.h>

const std::filesystem::path assets_path = ASSETS_PATH;

using Finder = std::function<bool(const rmlib::RenderObject*)>;

template<typename RO = rmlib::RenderObject>
using FindResult = std::vector<RO*>;

struct Offset {
  float x = 0.5f;
  float y = 0.5f;
};

struct TestContext : rmlib::AppContext {
  struct GoldenImageMatcher : Catch::Matchers::MatcherGenericBase {
    static inline const bool update_golden_files = [] {
      auto* update = std::getenv("UPDATE_GOLDEN");
      return update != nullptr && std::string_view(update) == "1";
    }();

    GoldenImageMatcher(std::filesystem::path path, TestContext& ctx)
      : path(path), ctx(ctx) {}

    template<typename RO>
    bool match(const FindResult<RO>& other) const {
      if (other.size() != 1) {
        return false;
      }

      if (update_golden_files) {
        ctx.writeImage(path, *other.front());
        return true;
      }

      const auto canvas =
        ctx.framebuffer.canvas.subCanvas(other.front()->getRect());
      const auto goldenCanvas = rmlib::ImageCanvas::load(path.c_str());
      if (!goldenCanvas.has_value()) {
        return false;
      }

      return canvas.compare(goldenCanvas->canvas);
    }

    std::string describe() const override { return "Matches image"; }

    std::filesystem::path path;
    TestContext& ctx;
  };

  static constexpr auto default_pump = std::chrono::milliseconds(1);

  static TestContext make() {
    auto appCtx = AppContext::makeContext();
    REQUIRE(appCtx.has_value());
    return TestContext(std::move(*appCtx));
  }

  explicit TestContext(AppContext appCtx)
    : AppContext(std::move(appCtx)), currentWidet(rmlib::Text("")) {
    framebuffer.clear();
  }

  // TODO: Store widget inside render object, not requiring this...
  rmlib::DynamicWidget currentWidet;

  template<typename Widget>
  void pumpWidget(Widget widget) {
    currentWidet = std::move(widget);
    setRootRenderObject(currentWidet.createRenderObject());
    pump();
  }

  void pump(std::chrono::milliseconds duration = default_pump) {
    bool running = true;
    auto handle = addTimer(duration, [&running] { running = false; });
    while (running) {
      step();
    }
  }

  FindResult<> getAllNodes(rmlib::RenderObject* obj = nullptr) {
    if (obj == nullptr) {
      obj = &getRootRenderObject();
    }

    std::vector<rmlib::RenderObject*> result;
    result.emplace_back(obj);

    for (auto* child : obj->getChildren()) {
      auto childNodes = getAllNodes(child);
      result.insert(result.end(), childNodes.begin(), childNodes.end());
    }
    return result;
  }

  // TODO: find stuff
  template<typename Widget>
  FindResult<typename Widget::RenderObjectType> findByType() {
    auto res =
      find([typeID = rmlib::type_id::typeId<Widget>()](const auto& ro) {
        return ro->getWidgetTypeID() == typeID;
      });

    std::vector<typename Widget::RenderObjectType*> casted;
    casted.reserve(res.size());
    std::transform(
      res.begin(), res.end(), std::back_inserter(casted), [](auto* ro) {
        return static_cast<typename Widget::RenderObjectType*>(ro);
      });

    return casted;
  }

  FindResult<rmlib::TextRenderObject> findByText(std::string_view text) {
    auto res = findByType<rmlib::Text>();
    res.erase(std::remove_if(
                res.begin(),
                res.end(),
                [text](auto* ro) { return ro->getWidget().getText() != text; }),
              res.end());
    return res;
  }

  std::vector<rmlib::RenderObject*> find(Finder finder) {
    auto nodes = getAllNodes();
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), std::not_fn(finder)),
                nodes.end());

    return nodes;
  }

  void writeImage(std::filesystem::path path, const rmlib::RenderObject& ro) {
    const auto canvas = framebuffer.canvas.subCanvas(ro.getRect());
    REQUIRE(canvas.writeImage(path.c_str()).has_value());
  }

  GoldenImageMatcher matchesGolden(std::string_view name) {
    return GoldenImageMatcher(assets_path / name, *this);
  }

  void sendEv(SDL_Event& ev) {
    SDL_PushEvent(&ev);
    step();
  }

  template<typename RO>
  void sendInput(bool press, const FindResult<RO>& ros, Offset offset) {
    REQUIRE(ros.size() == 1);
    rmlib::Rect rect = ros.front()->getRect();
    auto diff = rmlib::Point{ int(offset.x * rect.width()),
                              int(offset.y * rect.height()) };
    auto pos = rect.topLeft + diff;

    SDL_Event ev;
    ev.type = press ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    ev.motion.x = pos.x / EMULATE_SCALE;
    ev.motion.y = pos.y / EMULATE_SCALE;
    ev.button.button = SDL_BUTTON_LEFT;
    sendEv(ev);
  }

  template<typename RO>
  void press(const FindResult<RO>& ros, Offset offset = {}) {
    sendInput(true, ros, offset);
  }

  template<typename RO>
  void release(const FindResult<RO>& ros, Offset offset = {}) {
    sendInput(false, ros, offset);
  }

  template<typename RO>
  void tap(const FindResult<RO>& ros, Offset offset = {}) {
    press(ros, offset);
    release(ros, offset);
  }
};
