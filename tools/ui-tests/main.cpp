#include <functional>
#include <iostream>
#include <type_traits>

#include <unistd.h>

#include <Canvas.h>
#include <FrameBuffer.h>
#include <Input.h>

#include <UI.h>
#include <UI/Navigator.h>
#include <UI/Stack.h>
#include <UI/Wrap.h>

using namespace rmlib;

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

class ToggleTest : public StatefulWidget<ToggleTest> {
public:
  class State : public StateBase<ToggleTest> {
  public:
    auto build(AppContext& context, const BuildContext&) const {
      return GestureDetector(Border(Padding(Text(""), Insets::all(on ? 0 : 8)),
                                    Insets::all(on ? 10 : 2)),
                             Gestures{}.OnTap([this]() {
                               setState([](auto& self) { self.on = !self.on; });
                             }));
    }

  private:
    bool on = true;
  };

public:
  State createState() const { return State{}; }
};

class TimerTest : public StatefulWidget<TimerTest> {
public:
  class State : public StateBase<TimerTest> {
  public:
    void init(AppContext& context, const BuildContext&) {
      timer = context.addTimer(
        std::chrono::seconds(1), [this] { tick(); }, std::chrono::seconds(1));
    }

    auto build(AppContext& context, const BuildContext&) const {
      return Text(getWidget().name + ": " + std::to_string(ticks));
    }

  private:
    void tick() const {
      setState([](auto& self) { self.ticks *= 2; });
    }

    int ticks = 1;
    TimerHandle timer;
  };

  TimerTest(std::string name) : name(name) {}

  State createState() const { return State{}; }

  std::string name;
};

class CounterTest : public StatefulWidget<CounterTest> {
public:
  class State : public StateBase<CounterTest> {
  public:
    void init(AppContext& context) {}

    DynamicWidget build(AppContext& context, const BuildContext&) const {
      if (count < 5) {
        return Column(LabledInt("Counter: ", count),
                      Row(Button("-1", [this] { decrease(); }),
                          Button("+1", [this] { increase(); })),
                      TimerTest(std::to_string(count)));
      } else {
        return Row(Button("reset", [this]() { reset(); }), ToggleTest());
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

class TestW : public StatelessWidget<TestW> {
public:
  TestW(int i) : x(i) {}

  auto build(AppContext&, const BuildContext&) const {
    return Container(Text("Test: " + std::to_string(x)),
                     Insets::all(2),
                     Insets::all(2),
                     Insets::all(2));
  }

private:
  int x;
};

class WrapTest : public StatefulWidget<WrapTest> {
public:
  class State : public StateBase<WrapTest> {
  public:
    void init(AppContext& context) {}

    auto build(AppContext& context, const BuildContext&) const {
      std::vector<TestW> widgets;
      for (auto i = 0; i < count; i++) {
        widgets.emplace_back(i);
      }

      return Row(Column(Button("+1", [this] { increase(); }),
                        Button("-1", [this] { decrease(); })),
                 Wrap(widgets, Axis::Vertical));
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

class DialogTest : public StatelessWidget<DialogTest> {
public:
  DialogTest(std::string msg) : msg(std::move(msg)) {}

  auto build(AppContext&, const BuildContext& ctx) const {
    return Center((Border(
      Cleared(Column(
        Text(msg),
        Row(Padding(Button("OK", [&ctx] { Navigator::of(ctx).pop(true); }),
                    Insets::all(10)),
            Padding(Button("Cancel", [&ctx] { Navigator::of(ctx).pop(false); }),
                    Insets::all(10))))),
      Insets::all(5))));
  }

  std::string msg;
};

class NavTest : public StatelessWidget<NavTest> {
public:
  auto build(AppContext&, const BuildContext& buildCtx) const {
    return Center(Button("show dialog", [&buildCtx] {
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
    }));
  }
};

auto
navTest() {
  return Navigator(NavTest());
}

class DrawRenderObject;

class Drawer : public rmlib::Widget<DrawRenderObject> {
public:
  Drawer() {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const;
};

class DrawRenderObject : public LeafRenderObject<Drawer> {
public:
  static constexpr int thickness = 20;

  using LeafRenderObject::LeafRenderObject;
  ~DrawRenderObject() override {}

  void update(const Drawer& newWidget) {}

protected:
  rmlib::Size doLayout(const rmlib::Constraints& constraints) final {
    return constraints.max;
  }

  rmlib::UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) final {
    if (points.size() < 2) {
      return {};
    }

    UpdateRegion result;
    result.waveform = rmlib::fb::Waveform::DU;
    result.flags = fb::UpdateFlags::Priority;

    auto lastPoint = points.front();
    result.region |= Rect{ lastPoint, lastPoint };
    for (const auto& point : points) {
      if (point == lastPoint) {
        continue;
      }

      canvas.drawLine(lastPoint, point, black, thickness);
      canvas.drawDisk(point, thickness, black);

      lastPoint = point;
      result.region |= Rect{ lastPoint, lastPoint };
    }

    points.clear();
    if (down) {
      points.push_back(lastPoint);
    }

    result.region = Insets::all(-thickness).shrink(result.region);
    return result;
  }

  void handleInput(const rmlib::input::Event& ev) final {
    if (!std::holds_alternative<rmlib::input::PenEvent>(ev)) {
      return;
    }

    const auto& penEv = std::get<input::PenEvent>(ev);
    if (!getRect().contains(penEv.location)) {
      return;
    }

    if (penEv.isDown()) {
      down = true;
    }

    if (down && (points.empty() || points.back() != penEv.location)) {
      points.emplace_back(penEv.location);
      markNeedsDraw(false);
    }

    if (penEv.isUp()) {
      down = false;
      points.clear();
    }
  }

private:
  bool down = false;
  std::vector<Point> points;
};

std::unique_ptr<rmlib::RenderObject>
Drawer::createRenderObject() const {
  return std::make_unique<DrawRenderObject>(*this);
}

int
main() {
  // auto optErr = runApp(Center(Row(Text("Test:"), CounterTest())));
  auto optErr = runApp(Cleared(Drawer()));

  if (!optErr.has_value()) {
    std::cerr << optErr.error().msg << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
