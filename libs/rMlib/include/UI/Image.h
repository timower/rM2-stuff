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

  Canvas canvas;
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
    const auto& image = widget->canvas;

    if (image.rect().size() == rect.size()) {
      canvas.copy(rect.topLeft, image, image.rect());
      return UpdateRegion{ rect };
    }

    auto rectW = float(rect.width());
    auto rectH = float(rect.height());
    auto canvasW = float(image.width());
    auto canvasH = float(image.height());
    float scaleX = rectW / canvasW;
    float scaleY = rectH / canvasH;
    int offsetX = 0;
    int offsetY = 0;

    if (!widget->stretch) {
      if (scaleX > scaleY) {
        scaleX = scaleY;
        offsetX = int((rectW - canvasW * scaleX) / 2);
      } else {
        scaleY = scaleX;
        offsetY = int((rectH - canvasH * scaleY) / 2);
      }
    }

    canvas.transform(
      [&](int x, int y, int old) {
        auto subY = int(float(y - rect.topLeft.y - offsetY) / scaleY);
        auto subX = int(float(x - rect.topLeft.x - offsetX) / scaleX);
        if (!image.rect().contains(Point{ subX, subY })) {
          return old;
        }

        return image.getPixel(subX, subY);
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
