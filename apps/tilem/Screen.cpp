#include "Screen.h"

using namespace rmlib;

namespace {
constexpr auto frame_time = std::chrono::milliseconds(50); // 50 ms ->  20 fps
}

ScreenRenderObject::ScreenRenderObject(const Screen& widget)
  : LeafRenderObject<Screen>(widget)
  , lcd(tilem_lcd_buffer_new())
  , oldLcd(tilem_lcd_buffer_new()) {

  assert(lcd != nullptr && oldLcd != nullptr);
  if (widget.calc != nullptr) {
    addTimer(widget.calc);
  }
}

ScreenRenderObject::~ScreenRenderObject() {
  if (timerID != -1) {
    tilem_z80_remove_timer(widget->calc, timerID);
  }

  tilem_lcd_buffer_free(lcd);
  tilem_lcd_buffer_free(oldLcd);
}

void
ScreenRenderObject::stateFrameCallback(TilemCalc* calc, void* selfPtr) {
  auto* self = reinterpret_cast<ScreenRenderObject*>(selfPtr);
  self->markNeedsDraw(/* full */ false);
}

void
ScreenRenderObject::addTimer(TilemCalc* calc) {
  timerID = tilem_z80_add_timer(calc,
                                std::chrono::microseconds(frame_time).count(),
                                std::chrono::microseconds(frame_time).count(),
                                /* real time */ 1,
                                &stateFrameCallback,
                                this);
}

void
ScreenRenderObject::update(const Screen& newWidget) {
  if (newWidget.calc != widget->calc && newWidget.calc != nullptr) {
    if (timerID != -1) {
      tilem_z80_remove_timer(widget->calc, timerID);
    }

    addTimer(newWidget.calc);
  }
  widget = &newWidget;
}

Size
ScreenRenderObject::doLayout(const Constraints& constraints) {
  return constraints.max;
}

UpdateRegion
ScreenRenderObject::doDraw(Rect rect, Canvas& canvas) {
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
        const uint16_t pixel = data ? black : white;

        *canvasPtr = pixel;

        subX += inc_x;
        canvasPtr += 1;
      }
      subY += inc_y;
      canvasLinePtr += canvasStride;
    }
  }
  std::swap(lcd, oldLcd);

  return { rect, fb::Waveform::DU, fb::UpdateFlags::Priority };
}

std::unique_ptr<RenderObject>
Screen::createRenderObject() const {
  return std::make_unique<ScreenRenderObject>(*this);
}
