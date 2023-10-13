
#include <catch2/catch_test_macros.hpp>

#include <FrameBuffer.h>
#include <Input.h>

#include <UI/AppContext.h>
#include <UI/Layout.h>
#include <UI/Text.h>

#include <SDL_events.h>

using namespace rmlib;

constexpr auto default_pump = std::chrono::milliseconds(1);

extern bool rmlib_disable_window;

const auto disable_window_on_start = [] {
  rmlib_disable_window = true;
  return true;
}();

using Finder = std::function<bool(const RenderObject*)>;

struct TestContext : AppContext {
  static TestContext make() {
    auto appCtx = AppContext::makeContext();
    REQUIRE(appCtx.has_value());
    return TestContext(std::move(*appCtx));
  }

  TestContext(AppContext appCtx) : AppContext(std::move(appCtx)) {}

  template<typename Widget>
  void pumpWidget(const Widget& widget) {
    setRootRenderObject(widget.createRenderObject());
    pump();
  }

  void pump(std::chrono::milliseconds duration = default_pump) {
    bool running = true;
    auto handle = addTimer(duration, [&running] { running = false; });
    while (running) {
      step();
    }
  }

  std::vector<RenderObject*> getAllNodes(RenderObject* obj = nullptr) {
    if (obj == nullptr) {
      obj = &getRootRenderObject();
    }

    std::vector<RenderObject*> result;
    result.emplace_back(obj);

    for (auto* child : obj->getChildren()) {
      auto childNodes = getAllNodes(child);
      result.insert(result.end(), childNodes.begin(), childNodes.end());
    }
    return result;
  }

  // TODO: find stuff
  template<typename Widget>
  std::vector<typename Widget::RenderObjectType*> findByType() {
    auto res =
      find([typeID = rmlib::typeID::type_id<Widget>()](const auto& ro) {
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

  std::vector<TextRenderObject*> findByText(std::string_view text) {
    auto res = findByType<Text>();
    res.erase(std::remove_if(
                res.begin(),
                res.end(),
                [text](auto* ro) { return ro->getWidget().getText() != text; }),
              res.end());
    return res;
  }

  std::vector<RenderObject*> find(Finder finder) {
    auto nodes = getAllNodes();
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), std::not_fn(finder)),
                nodes.end());

    return nodes;
  }
};

TEST_CASE("Init Framebuffer", "[rmlib]") {
  auto fb = rmlib::fb::FrameBuffer::open();
  REQUIRE(fb.has_value());
}

TEST_CASE("Init Input", "[rmlib]") {
  rmlib::input::InputManager input;

  REQUIRE(input.openAll().has_value());

  auto evs = input.waitForInput(std::chrono::milliseconds(2));
  REQUIRE(evs.has_value());
  REQUIRE(evs->size() == 0);

  SDL_Event testEv;
  testEv.type = SDL_MOUSEBUTTONDOWN;
  testEv.motion.x = 5;
  testEv.motion.y = 10;
  testEv.button.button = SDL_BUTTON_LEFT;

  SDL_PushEvent(&testEv);
  evs = input.waitForInput(std::chrono::milliseconds(2));
  REQUIRE(evs.has_value());
  REQUIRE(evs->size() == 1);
  REQUIRE(std::holds_alternative<input::TouchEvent>(evs->front()));
}

TEST_CASE("Text", "[rmlib][ui]") {
  auto ctx = TestContext::make();

  ctx.pumpWidget(Center(Text("Test")));

  auto txt = ctx.findByType<Text>();
  REQUIRE(txt.size() == 1);
  REQUIRE(txt.front()->getWidget().getText() == "Test");

  auto txt2 = ctx.findByText("Test");
  REQUIRE(txt == txt2);
}
