#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include "rMLibTestHelper.h"

#include <FrameBuffer.h>
#include <Input.h>

#include <UI/AppContext.h>
#include <UI/Button.h>
#include <UI/DynamicWidget.h>
#include <UI/Flex.h>
#include <UI/Layout.h>
#include <UI/Navigator.h>
#include <UI/StatelessWidget.h>
#include <UI/Text.h>

#include <SDL_events.h>

#include <filesystem>

using namespace rmlib;

// TODO: move somewhere?
extern bool rmlib_disable_window;

const auto disable_window_on_start = [] {
  rmlib_disable_window = true;
  return true;
}();

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

  REQUIRE_THAT(ctx.findByText("Test"), ctx.matchesGolden("text-basic.png"));
}

TEST_CASE("Sized", "[rmlib][ui]") {
  auto ctx = TestContext::make();

  SECTION("limited") {
    ctx.pumpWidget(Center(Sized(Text("Test"), 50, 50)));

    auto txt = ctx.findByText("Test");
    REQUIRE_THAT(txt, Catch::Matchers::SizeIs(1));

    REQUIRE(txt.front()->getSize() == Size{ 50, 50 });
    REQUIRE_THAT(txt, ctx.matchesGolden("text-sized.png"));
  }

  SECTION("unlimited") {
    ctx.pumpWidget(Center(Sized(Text("Test"), std::nullopt, std::nullopt)));

    auto txt = ctx.findByText("Test");
    REQUIRE_THAT(txt, Catch::Matchers::SizeIs(1));

    REQUIRE(txt.front()->getSize() != Size{ 50, 50 });
    REQUIRE_THAT(txt, ctx.matchesGolden("text-sized2.png"));
  }

  SECTION("partial") {
    ctx.pumpWidget(Center(Sized(Text("Test"), std::nullopt, 20)));

    auto txt = ctx.findByText("Test");
    REQUIRE_THAT(txt, Catch::Matchers::SizeIs(1));

    REQUIRE(txt.front()->getSize().height == 20);
    REQUIRE_THAT(txt, ctx.matchesGolden("text-sized3.png"));
  }

  SECTION("partial") {
    ctx.pumpWidget(Center(Sized(Text("Test"), 200, std::nullopt)));

    auto txt = ctx.findByText("Test");
    REQUIRE_THAT(txt, Catch::Matchers::SizeIs(1));

    REQUIRE(txt.front()->getSize().width == 200);
    REQUIRE_THAT(txt, ctx.matchesGolden("text-sized4.png"));
  }
}

TEST_CASE("Container", "[rmlib][ui]") {
  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(
    Container(Text("Test"), Insets::all(5), Insets::all(6), Insets::all(3))));
  REQUIRE_THAT(ctx.findByType<Padding<Border<Padding<Text>>>>(),
               ctx.matchesGolden("container-text.png"));
}

TEST_CASE("Flex", "[rmlib][ui]") {
  auto ctx = TestContext::make();

  SECTION("Column") {
    ctx.pumpWidget(Center(Column(Text("Foo"), Text("Bar"))));

    auto column = ctx.findByType<Flex<Text, Text>>();
    REQUIRE_THAT(column, ctx.matchesGolden("column-text.png"));
  }

  SECTION("Row") {
    ctx.pumpWidget(Center(Row(Text("Foo"), Text("Bar"))));

    auto column = ctx.findByType<Flex<Text, Text>>();
    REQUIRE_THAT(column, ctx.matchesGolden("row-text.png"));
  }

  SECTION("Expanded") {
    ctx.pumpWidget(
      Center(Row(Text("Foo"), Expanded(Text("Test")), Text("Bar"))));

    auto column = ctx.findByType<Flex<Text, Expanded<Text>, Text>>();
    REQUIRE_THAT(column, ctx.matchesGolden("expanded-text.png"));
  }

  SECTION("Expanded - flex") {
    ctx.pumpWidget(
      Center(Sized(Column(Expanded(Border(Text("Foo"), Insets::all(2)), 2.0f),
                          Expanded(Border(Text("Test"), Insets::all(2))),
                          Border(Text("Bar"), Insets::all(2))),
                   200,
                   364)));

    auto txt1 = ctx.findByText("Foo");
    auto txt2 = ctx.findByText("Test");
    auto txt3 = ctx.findByText("Bar");
    REQUIRE(txt1.size() == 1);
    REQUIRE(txt2.size() == 1);
    REQUIRE(txt3.size() == 1);

    REQUIRE(txt1.front()->getSize().height ==
            Catch::Approx(txt2.front()->getSize().height * 2).margin(10));

    auto column = ctx.findByType<
      Flex<Expanded<Border<Text>>, Expanded<Border<Text>>, Border<Text>>>();
    REQUIRE_THAT(column, ctx.matchesGolden("expanded-flex-text.png"));
  }
}

TEST_CASE("Dynamic", "[rmlib][ui]") {
  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(DynamicWidget(Text("12"))));

  REQUIRE(ctx.findByType<Text>().size() == 1);
}

class LabledInt : public StatelessWidget<LabledInt> {
public:
  LabledInt(std::string label, int n) : label(std::move(label)), integer(n) {}

  auto build(AppContext& context, const BuildContext&) const {
    return Row(Text(label), Text(std::to_string(integer)));
  }

private:
  std::string label;
  int integer;
};

TEST_CASE("Stateless", "[rmlib][ui]") {
  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LabledInt("Foo: ", 55)));

  REQUIRE_THAT(ctx.findByType<LabledInt>(), ctx.matchesGolden("stateless.png"));
}

TEST_CASE("Button", "[rmlib][ui]") {
  auto ctx = TestContext::make();

  int clicked = 0;

  ctx.pumpWidget(Center(Button("Click", [&] { clicked++; })));
  REQUIRE(clicked == 0);

  auto btnTxt = ctx.findByText("Click");
  auto btn = ctx.findByType<Button>();

  REQUIRE_THAT(btn, ctx.matchesGolden("button-released.png"));
  ctx.press(btnTxt);
  ctx.pump();
  REQUIRE(clicked == 0);
  REQUIRE_THAT(btn, ctx.matchesGolden("button-pressed.png"));
  ctx.release(btnTxt);
  ctx.pump();
  REQUIRE(clicked == 1);
  REQUIRE_THAT(btn, ctx.matchesGolden("button-released.png"));

  ctx.tap(btnTxt);
  REQUIRE(clicked == 2);
}

class CounterTest : public StatefulWidget<CounterTest> {
public:
  class State : public StateBase<CounterTest> {
  public:
    void init(AppContext& context, const BuildContext&) {}

    DynamicWidget build(AppContext& context, const BuildContext&) const {
      if (count < 5) {
        return Column(LabledInt("Counter: ", count),
                      Row(Button("-1", [this] { decrease(); }),
                          Button("+1", [this] { increase(); })));
      } else {
        return Button("reset", [this]() { reset(); });
      }
    }

  private:
    void reset() const {
      setState([](auto& self) { self.count = 0; });
    }
    void increase() const {
      setState([](auto& self) { self.count++; });
    }

    void decrease() const {
      setState([](auto& self) { self.count--; });
    }

    int count = 0;
    TimerHandle timer;
  };

public:
  State createState() const { return State{}; }
};

TEST_CASE("StateFull - Dynamic", "[rmlib][ui]") {
  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(CounterTest()));

  auto counter = ctx.findByType<CounterTest>();

  {
    REQUIRE_THAT(ctx.findByText("0"), Catch::Matchers::SizeIs(1));
    REQUIRE_THAT(ctx.findByText("+1"), Catch::Matchers::SizeIs(1));
    REQUIRE_THAT(ctx.findByText("-1"), Catch::Matchers::SizeIs(1));

    REQUIRE_THAT(counter, ctx.matchesGolden("counter-init.png"));

    ctx.tap(ctx.findByText("+1"));
    ctx.pump();
    REQUIRE_THAT(ctx.findByText("1"), Catch::Matchers::SizeIs(1));
    REQUIRE_THAT(counter, ctx.matchesGolden("counter-1.png"));

    ctx.tap(ctx.findByText("-1"));
    ctx.pump();
    REQUIRE_THAT(ctx.findByText("0"), Catch::Matchers::SizeIs(1));
    REQUIRE_THAT(counter, ctx.matchesGolden("counter-init.png"));

    for (int i = 1; i < 5; i++) {
      ctx.tap(ctx.findByText("+1"));
      ctx.pump();
      REQUIRE_THAT(ctx.findByText(std::to_string(i)),
                   Catch::Matchers::SizeIs(1));
    }
    REQUIRE_THAT(counter, ctx.matchesGolden("counter-5.png"));

    ctx.tap(ctx.findByText("+1"));
    ctx.pump();
    REQUIRE_THAT(counter, ctx.matchesGolden("counter-reset.png"));
  }

  auto resetBtn = ctx.findByText("reset");
  ctx.tap(resetBtn);
  ctx.pump();
  REQUIRE_THAT(counter, ctx.matchesGolden("counter-init.png"));
}

class DialogTest : public StatelessWidget<DialogTest> {
public:
  DialogTest(std::string msg) : msg(std::move(msg)) {}

  auto build(AppContext&, const BuildContext& ctx) const {
    return Border(
      Cleared(Column(
        Text(msg),
        Row(Padding(Button("OK", [&ctx] { Navigator::of(ctx).pop(true); }),
                    Insets::all(10)),
            Padding(Button("Cancel", [&ctx] { Navigator::of(ctx).pop(false); }),
                    Insets::all(10))))),
      Insets::all(5));
  }

  std::string msg;
};

class NavTest : public StatelessWidget<NavTest> {
public:
  auto build(AppContext&, const BuildContext& buildCtx) const {
    return Button("show dialog", [&buildCtx] {
      Navigator::of(buildCtx)
        .push<bool>([] { return DialogTest("First dialog"); })
        .then([&buildCtx](bool val) {
          std::cout << "Closed First\n";
          return Navigator::of(buildCtx).push<bool>([val] {
            return DialogTest("Second dialog: " +
                              std::string(val ? "OK" : "Cancel"));
          });
        })
        .then([](bool val) { std::cout << "Closed second\n"; });
    });
  }
};

TEST_CASE("Navigator", "[rmlib][ui]") {
  auto ctx = TestContext::make();

  ctx.pumpWidget(Center(Navigator(NavTest())));
  auto navTest = ctx.findByType<Navigator>();

  REQUIRE_THAT(navTest, ctx.matchesGolden("nav-init.png"));

  ctx.tap(ctx.findByText("show dialog"));
  ctx.pump();
  REQUIRE_THAT(navTest, ctx.matchesGolden("nav-second.png"));

  ctx.tap(ctx.findByText("OK"));
  ctx.pump();
  REQUIRE_THAT(navTest, ctx.matchesGolden("nav-third.png"));

  ctx.tap(ctx.findByText("OK"));
  ctx.pump();
  REQUIRE_THAT(navTest, ctx.matchesGolden("nav-init.png"));
}

// TODO: image, colored, Positioned
