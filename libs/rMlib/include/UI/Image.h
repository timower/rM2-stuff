#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

namespace rmlib {

class ColoredRenderObject;

class Colored : public Widget<ColoredRenderObject> {
private:
public:
  Colored(int color) : color(color) {}

  std::unique_ptr<RenderObject> createRenderObject() const;

private:
  friend class ColoredRenderObject;
  int color;
};

class ColoredRenderObject : public LeafRenderObject<Colored> {
public:
  using LeafRenderObject<Colored>::LeafRenderObject;

  void update(const Colored& newWidget) {
    if (newWidget.color != widget->color) {
      std::cout << "color changed\n";
      markNeedsDraw();
    }
    widget = &newWidget;
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    auto result = constraints.max;

    if (result.height == Constraints::unbound) {
      result.height = constraints.min.height;
    }

    if (result.width == Constraints::unbound) {
      result.width = constraints.min.width;
    }

    return result;
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    canvas.set(rect, widget->color);
    return UpdateRegion{ rect };
  }
};

inline std::unique_ptr<RenderObject>
Colored::createRenderObject() const {
  return std::make_unique<ColoredRenderObject>(*this);
}

class ImageRenderObject;

class Image : public Widget<ImageRenderObject> {
private:
public:
  Image(const Canvas& canvas, bool stretch = false)
    : canvas(canvas), stretch(stretch) {}

  std::unique_ptr<RenderObject> createRenderObject() const;

  const Canvas& canvas;
  bool stretch;
};

class ImageRenderObject : public LeafRenderObject<Image> {
public:
  using LeafRenderObject<Image>::LeafRenderObject;

  void update(const Image& newWidget) {
    if (newWidget.canvas != widget->canvas) {
      markNeedsDraw();
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
    float scale_x = (float)rect.width() / widget->canvas.width();
    float scale_y = (float)rect.height() / widget->canvas.height();
    int offset_x = 0;
    int offset_y = 0;

    if (!widget->stretch) {
      if (scale_x > scale_y) {
        scale_x = scale_y;
        offset_x = (rect.width() - widget->canvas.width() * scale_x) / 2;
      } else {
        scale_y = scale_x;
        offset_y = (rect.height() - widget->canvas.height() * scale_y) / 2;
      }
    }

    canvas.transform(
      [&](int x, int y, int old) {
        int subY = (y - rect.topLeft.y - offset_y) / scale_y;
        int subX = (x - rect.topLeft.x - offset_x) / scale_x;
        if (!widget->canvas.rect().contains(Point{ subX, subY })) {
          return old;
        }

        auto pixel = widget->canvas.getPixel(subX, subY);
        // auto* pixel = widget->canvas.getPtr(subX, subY);
        return pixel;
      },
      rect);

    return UpdateRegion{ rect };
  }
};

inline std::unique_ptr<RenderObject>
Image::createRenderObject() const {
  return std::make_unique<ImageRenderObject>(*this);
}

} // namespace rmlib
