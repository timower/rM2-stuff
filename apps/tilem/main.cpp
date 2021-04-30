#include <FrameBuffer.h>
#include <Input.h>

#include <UI.h>
#include <UI/Navigator.h>

#include "scancodes.h"
#include "tilem.h"

#include <atomic>
#include <csignal>
#include <iostream>
#include <optional>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "skin.h"

using namespace rmlib;
using namespace rmlib::input;

namespace {

constexpr auto calc_save_extension = ".sav";
constexpr auto calc_default_rom = "ti84p.rom";

const auto FPS = 100;
const auto TPS = std::chrono::milliseconds(1000) / FPS;
const auto frame_time = std::chrono::milliseconds(50); // 50 ms ->  20 fps

struct Key {
  int scancode;
  std::string_view front;
  std::string_view shift = "";
  std::string_view alpha = "";
  float width = 1;
};

const std::vector<std::vector<Key>> keymap = {
  { { TILEM_KEY_YEQU, "Y=", "STAT PLOT", "F1" },
    { TILEM_KEY_WINDOW, "WINDOW", "TBLST", "F2" },
    { TILEM_KEY_ZOOM, "ZOOM", "FORMAT", "F3" },
    { TILEM_KEY_TRACE, "TRACE", "CALC", "F4" },
    { TILEM_KEY_GRAPH, "GRAPH", "TABLE", "F5" } },

  { { 0, "", "", "", 3.5f }, { TILEM_KEY_UP, "Λ", "", "", 1 } },

  { { TILEM_KEY_2ND, "2ND", "", "" },
    { TILEM_KEY_MODE, "MODE", "QUIT", "" },
    { TILEM_KEY_DEL, "DEL", "INS", "" },
    { TILEM_KEY_LEFT, "<", "", "" },
    { TILEM_KEY_RIGHT, ">", "", "" } },

  { { TILEM_KEY_ALPHA, "ALPHA", "A-LOCK", "" },
    { TILEM_KEY_GRAPHVAR, "X,T,θ,n", "LINK", "" },
    { TILEM_KEY_STAT, "STAT", "LIST", "" },
    { 0, "", "", "", 0.5f },
    { TILEM_KEY_DOWN, "V", "", "", 1 } },

  { { TILEM_KEY_MATH, "MATH", "TEST", "A" },
    { TILEM_KEY_MATRIX, "APPS", "ANGLE", "B" },
    { TILEM_KEY_PRGM, "PRGM", "DRAW", "C" },
    { TILEM_KEY_VARS, "VARS", "DISTR", "" },
    { TILEM_KEY_CLEAR, "CLEAR", "", "" } },

  { { TILEM_KEY_RECIP, "x^-1", "MATRIX", "D" },
    { TILEM_KEY_SIN, "SIN", "SIN^-1", "E" },
    { TILEM_KEY_COS, "COS", "COS^-1", "F" },
    { TILEM_KEY_TAN, "TAN", "TAN^-1", "G" },
    { TILEM_KEY_POWER, "^", "π", "H" } },

  { { TILEM_KEY_SQUARE, "x²", "√", "I" },
    { TILEM_KEY_COMMA, ",", "EE", "J" },
    { TILEM_KEY_LPAREN, "(", "{", "K" },
    { TILEM_KEY_RPAREN, ")", "}", "L" },
    { TILEM_KEY_DIV, "/", "e", "M" } },

  { { TILEM_KEY_LOG, "LOG", "10^x", "N" },
    { TILEM_KEY_7, "7", "u", "O" },
    { TILEM_KEY_8, "8", "v", "P" },
    { TILEM_KEY_9, "9", "w", "Q" },
    { TILEM_KEY_MUL, "*", "[", "R" } },

  { { TILEM_KEY_LN, "LN", "e^x", "S" },
    { TILEM_KEY_4, "4", "L4", "T" },
    { TILEM_KEY_5, "5", "L5", "U" },
    { TILEM_KEY_6, "6", "L6", "V" },
    { TILEM_KEY_SUB, "-", "]", "W" } },

  { { TILEM_KEY_STORE, "STO>", "RCL", "X" },
    { TILEM_KEY_1, "1", "L1", "Y" },
    { TILEM_KEY_2, "2", "L2", "Z" },
    { TILEM_KEY_3, "3", "L3", "θ" },
    { TILEM_KEY_ADD, "+", "MEM", "\"" } },

  { { TILEM_KEY_ON, "ON", "OFF", "" },
    { TILEM_KEY_0, "0", "CATALOG", "_" },
    { TILEM_KEY_DECPNT, ".", "i", ":" },
    { TILEM_KEY_CHS, "(-)", "ANS", "?" },
    { TILEM_KEY_ENTER, "ENTER", "ENTRY", "SOLVE" } },

};

class KeypadRenderObject;

class Keypad : public Widget<KeypadRenderObject> {
public:
  Keypad(TilemCalc* calc) : calc(calc) {
    maxRowSize = std::max_element(keymap.begin(),
                                  keymap.end(),
                                  [](const auto& a, const auto& b) {
                                    return a.size() < b.size();
                                  })
                   ->size();
    numRows = keymap.size();
  }

  std::unique_ptr<RenderObject> createRenderObject() const;

private:
  friend class KeypadRenderObject;
  TilemCalc* calc = nullptr;
  size_t maxRowSize;
  size_t numRows;
};

class KeypadRenderObject : public LeafRenderObject<Keypad> {
public:
  using LeafRenderObject<Keypad>::LeafRenderObject;

  void update(const Keypad& newWidget) { widget = &newWidget; }

protected:
  Size doLayout(const Constraints& constraints) final {
    keyWidth = constraints.max.width / widget->maxRowSize;
    keyHeight = keyWidth / key_aspect;

    const auto width = std::clamp(keyWidth * int(widget->maxRowSize),
                                  constraints.min.width,
                                  constraints.max.width);
    const auto height = std::clamp(keyHeight * int(widget->numRows),
                                   constraints.min.height,
                                   constraints.max.height);
    padding = constraints.max.width - keyWidth * widget->maxRowSize;

    return { width, height };
  }

  void drawKey(Canvas& canvas, Point pos, const Key& key, int keyWidth) {
    const auto frontLabelHeight = key.shift.empty() && key.alpha.empty()
                                    ? keyHeight
                                    : int(front_label_factor * keyHeight);
    const auto upperLabelHeight = keyHeight - frontLabelHeight;

    // Draw front label.
    {
      const auto fontSize = std::min(
        frontLabelHeight, int(key_aspect * keyWidth / key.front.size()));
      const auto fontSizes = Canvas::getTextSize(key.front, fontSize);

      const auto xOffset = (keyWidth - fontSizes.x) / 2;
      const auto yOffset =
        upperLabelHeight + (frontLabelHeight - fontSizes.y) / 2;
      const auto position = pos + Point{ xOffset, yOffset };

      canvas.drawText(key.front, position, fontSize);
    }

    // Draw alpha and 2nd label.
    {
      const auto upperLength = key.alpha.size() + key.shift.size();
      const auto fontSize =
        std::min(upperLabelHeight, int(1.6 * keyWidth / upperLength));

      auto testStr = std::string(key.shift);
      if (!key.alpha.empty()) {
        testStr += " " + std::string(key.alpha);
      }

      const auto fontSizes = Canvas::getTextSize(testStr, fontSize);

      const auto xOffset = (keyWidth - fontSizes.x) / 2;
      const auto yOffset = (upperLabelHeight - fontSizes.y) / 2;

      const auto position = pos + Point{ xOffset, yOffset };

      canvas.drawText(key.shift, position, fontSize, 0x55);

      if (!key.alpha.empty()) {
        const auto spacing =
          Canvas::getTextSize(std::string(key.shift) + " ", fontSize);
        const auto positonA = pos + Point{ xOffset + spacing.x, yOffset };
        canvas.drawText(key.alpha, positonA, fontSize, 0xaa);
      }
    }

    canvas.drawRectangle(
      pos, pos + Point{ keyWidth - 1, keyHeight - 1 }, black);
  }

  UpdateRegion doDraw(Rect rect, Canvas& canvas) final {
    keyLocations.clear();
    canvas.set(rect, white);

    int y = rect.topLeft.y;
    for (const auto& row : keymap) {

      int padding = this->padding;
      int x = rect.topLeft.x;
      for (const auto& key : row) {
        auto keyW = int(keyWidth * key.width);
        if (padding > 0) {
          keyW += 1;
          padding -= 1;
        }

        if (key.scancode != 0) {
          keyLocations.emplace_back(
            Rect{ { x, y }, { x + keyW - 1, y + keyHeight - 1 } }, &key);
          drawKey(canvas, { x, y }, key, keyW);
        }

        x += keyW;
      }

      y += keyHeight;
    }

    return { rect };
  }

  void handleInput(const Event& ev) final {
    if (widget->calc == nullptr) {
      return;
    }

    std::visit(
      [this](const auto& ev) {
        if constexpr (is_pointer_event<decltype(ev)>) {
          if (ev.isMove()) {
            return;
          }

          if (ev.isUp()) {
            auto it = keyPointers.find(ev.id);
            if (it == keyPointers.end()) {
              return;
            }

            tilem_keypad_release_key(widget->calc, it->second->scancode);
            keyPointers.erase(it);
            return;
          }

          if (ev.isDown()) {
            for (const auto& [rect, keyPtr] : keyLocations) {
              if (rect.contains(ev.location)) {
                tilem_keypad_press_key(widget->calc, keyPtr->scancode);
                keyPointers.emplace(ev.id, keyPtr);
                break;
              }
            }
          }
        }
      },
      ev);
  }

private:
  constexpr static auto key_aspect = 1.5;
  constexpr static auto front_label_factor = 0.6;

  std::vector<std::pair<Rect, const Key*>> keyLocations;
  std::unordered_map<int, const Key*> keyPointers;
  int keyWidth;
  int keyHeight;
  int padding;
};

std::unique_ptr<RenderObject>
Keypad::createRenderObject() const {
  return std::make_unique<KeypadRenderObject>(*this);
}

class ScreenRenderObject;

class Screen : public Widget<ScreenRenderObject> {
public:
  Screen(TilemCalc* calc) : calc(calc) {}

  std::unique_ptr<RenderObject> createRenderObject() const;

private:
  friend class ScreenRenderObject;
  TilemCalc* calc = nullptr;
};

class ScreenRenderObject : public LeafRenderObject<Screen> {
public:
  ScreenRenderObject(const Screen& widget)
    : LeafRenderObject<Screen>(widget)
    , lcd(tilem_lcd_buffer_new())
    , oldLcd(tilem_lcd_buffer_new()) {

    assert(lcd != nullptr && oldLcd != nullptr);
    if (widget.calc != nullptr) {
      addTimer(widget.calc);
    }
  }

  static void stateFrameCallback(TilemCalc* calc, void* selfPtr) {
    auto* self = reinterpret_cast<ScreenRenderObject*>(selfPtr);
    self->markNeedsDraw(/* full */ false);
  }

  void addTimer(TilemCalc* calc) {
    tilem_z80_add_timer(calc,
                        std::chrono::microseconds(frame_time).count(),
                        std::chrono::microseconds(frame_time).count(),
                        /* real time */ 1,
                        &stateFrameCallback,
                        this);
  }

  void update(const Screen& newWidget) {
    if (newWidget.calc != widget->calc && newWidget.calc != nullptr) {
      addTimer(newWidget.calc);
    }
    widget = &newWidget;
  }

protected:
  Size doLayout(const Constraints& constraints) final {
    return constraints.max;
  }

  UpdateRegion doDraw(Rect rect, Canvas& canvas) final {
    if (widget->calc == nullptr) {
      return {};
    }

    tilem_lcd_get_frame(widget->calc, lcd);
    if (isPartialDraw() && oldLcd->contrast == lcd->contrast &&
        (lcd->contrast == 0 ||
         (oldLcd->data != nullptr &&
          std::memcmp(lcd->data, oldLcd->data, lcd->rowstride * lcd->height) ==
            0))) {
      return {};
    }

    if (lcd->contrast == 0) {
      canvas.set(rect, black);
    } else {
      float inc_x = float(lcd->width) / rect.width();
      float inc_y = float(lcd->height) / rect.height();

      const auto canvasStride = canvas.lineSize() / sizeof(uint16_t);
      auto* canvasLinePtr =
        canvas.getPtr<uint16_t>(rect.topLeft.x, rect.topLeft.y);

      float subY = 0;
      for (int y = rect.topLeft.y; y <= rect.bottomRight.y; y++) {
        const uint8_t* lcdRow = &lcd->data[int(subY) * lcd->rowstride];
        auto* canvasPtr = canvasLinePtr;

        float subX = 0;
        for (int x = rect.topLeft.x; x <= rect.bottomRight.x; x++) {
          const uint8_t data = lcdRow[int(subX)];
          const uint8_t pixel = data ? 0 : 0xff;

          *canvasPtr = pixel;

          subX += inc_x;
          canvasPtr += 1;
        }
        subY += inc_y;
        canvasLinePtr += canvasStride;
      }
    }
    std::swap(lcd, oldLcd);

    return { rect, fb::Waveform::DU };
  }

private:
  TilemLCDBuffer* lcd = nullptr;
  TilemLCDBuffer* oldLcd = nullptr;
};

std::unique_ptr<RenderObject>
Screen::createRenderObject() const {
  return std::make_unique<ScreenRenderObject>(*this);
}

class DownloadDialog : public StatefulWidget<DownloadDialog> {
public:
  class State : public StateBase<DownloadDialog> {
  public:
    void init(AppContext& ctx, const BuildContext& buildCtx) {
      startTimer = ctx.addTimer(std::chrono::seconds(0), [&] {
        constexpr auto url =
          "http://tiroms.weebly.com/uploads/1/1/0/5/110560031/ti84plus.rom";
        auto cmd = "wget -O '" + getWidget().romPath + "' " + url;

        std::cout << cmd << "\n";
        system(cmd.c_str());

        Navigator::of(buildCtx).pop();
      });
    }

    auto build(AppContext& appCtx, const BuildContext& ctx) const {

      return Center(Border(
        Cleared(Text("Downloading ROM '" + getWidget().romPath + "' ...")),
        Insets::all(5)));
    }

  private:
    TimerHandle startTimer;
  };

  DownloadDialog(std::string_view romPath) : romPath(romPath) {}

  State createState() const { return State{}; }

  std::string romPath;
};

class ErrorDialog : public StatelessWidget<ErrorDialog> {
public:
  ErrorDialog(std::string_view romPath) : romPath(romPath) {}

  auto build(AppContext& appCtx, const BuildContext& ctx) const {
    return Center((Border(
      Cleared(Column(
        Text("Loading ROM '" + romPath + "' failed"),
        Row(Padding(Button("Download", [&ctx] { Navigator::of(ctx).pop(); }),
                    Insets::all(10)),
            Padding(Button("Exit", [&appCtx] { appCtx.stop(); }),
                    Insets::all(10))))),
      Insets::all(5))));
  }

  std::string romPath;
};

class Calculator;
class CalcState;

class Calculator : public StatefulWidget<Calculator> {
public:
  Calculator(std::string romPath)
    : romPath(romPath), savePath(romPath + calc_save_extension) {}

  CalcState createState() const;

private:
  friend class CalcState;

  std::string romPath;
  std::string savePath;
};

class CalcState : public StateBase<Calculator> {
public:
  void init(AppContext& context, const BuildContext& buildCtx) {
    mCalc = loadCalc();

    if (mCalc == nullptr) {
      popupTimer = context.addTimer(std::chrono::seconds(0), [&] {
        Navigator::of(buildCtx)
          .push(
            [&romPath = getWidget().romPath] { return ErrorDialog(romPath); })
          .then([&buildCtx, &romPath = getWidget().romPath] {
            return Navigator::of(buildCtx).push(
              [&romPath] { return DownloadDialog(romPath); });
          })
          .then([this, &context, &buildCtx] {
            setState([&context, &buildCtx](auto& self) {
              self.init(context, buildCtx);
            });
          });
      });
      return;
    }

    std::cout << "loaded rom, entering mainloop\n";
    lastUpdateTime = std::chrono::steady_clock::now();
    updateTimer = context.addTimer(
      TPS, [this] { updateCalcState(); }, TPS);
  }

  auto closeButton(AppContext& context, int fontSize) const {
    return Sized(GestureDetector(
                   Border(Text("X", fontSize), Insets{ 0, 0, /* left */ 2, 0 }),
                   Gestures{}.OnTap([&context] { context.stop(); })),
                 fontSize,
                 fontSize);
  }

  auto header(AppContext& context, int width) const {
    constexpr auto fontSize = 48;
    // TODO: expand option
    return Cleared(Border(
      Row(Sized(Text("Tilem", fontSize), width - fontSize - 2, std::nullopt),
          closeButton(context, fontSize)),
      Insets::all(1)));
  }

  auto build(AppContext& context, const BuildContext& buildCtx) const {

    constexpr auto scale = 6.5;
    constexpr auto width = scale * 96;
    constexpr auto height = scale * 64;
    return Center(Border(Column(header(context, width),
                                Sized(Screen(mCalc), width, height),
                                Sized(Keypad(mCalc), width, std::nullopt)),
                         Insets::all(1)));
  }

  ~CalcState() {
    if (mCalc == nullptr) {
      return;
    }

    std::cout << "Saving state to: " + getWidget().savePath + "\n";
    auto* save = fopen(getWidget().savePath.c_str(), "w");
    if (save == nullptr) {
      perror("Error opening save file");
    } else {
      tilem_calc_save_state(mCalc, nullptr, save);
      fclose(save);
    }
  }

private:
  TilemCalc* loadCalc() const {
    FILE* rom = fopen(getWidget().romPath.c_str(), "r");
    if (rom == nullptr) {
      perror("Error opening rom file");
      return nullptr;
    }

    FILE* save = fopen(getWidget().savePath.c_str(), "r");
    if (save == nullptr) {
      perror("No save");
    }

    auto* result = tilem_calc_new(TILEM_CALC_TI84P);
    if (result == nullptr) {
      std::cerr << "Error init Calc\n";
      return nullptr;
    }

    if (tilem_calc_load_state(result, rom, save) != 0) {
      perror("Error loading rom or save");

      fclose(rom);
      if (save != nullptr) {
        fclose(save);
      }
      return nullptr;
    }

    fclose(rom);
    if (save != nullptr) {
      fclose(save);
    }

    return result;
  }

  void updateCalcState() {
    const auto time = std::chrono::steady_clock::now();
    auto diff = time - lastUpdateTime;

    // Skip frames if we were paused.
    if (diff > std::chrono::seconds(1)) {
      std::cout << "Skipping frames...\n";
      diff = TPS;
    }

    tilem_z80_run_time(
      mCalc,
      std::chrono::duration_cast<std::chrono::microseconds>(diff).count(),
      nullptr);

    lastUpdateTime = time;
  }

  TilemCalc* mCalc = nullptr;

  TimerHandle updateTimer;
  TimerHandle popupTimer;

  std::chrono::steady_clock::time_point lastUpdateTime;
};

CalcState
Calculator::createState() const {
  return CalcState{};
}

} // namespace

int
main(int argc, char* argv[]) {
  const auto* calc_name = argc > 1 ? argv[1] : calc_default_rom;

  const auto err = runApp(Navigator(Calculator(calc_name)));

  if (err.isError()) {
    std::cerr << err.getError().msg << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
