#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

#include <string>

namespace rmlib {

class TextRenderObject;

class Text : public Widget<TextRenderObject> {
public:
  Text(std::string text, int fontSize = default_text_size)
    : text(std::move(text)), fontSize(fontSize) {}

  std::unique_ptr<RenderObject> createRenderObject() const;

private:
  friend class TextRenderObject;
  std::string text;
  int fontSize;
};

class TextRenderObject : public RenderObject {
public:
  TextRenderObject(const Text& widget)
    : RenderObject(typeID::type_id<Text>()), widget(&widget) {}

  void update(const Text& newWidget) {
    if (newWidget.fontSize != widget->fontSize ||
        newWidget.text != widget->text) {
      markNeedsDraw();
      markNeedsLayout();
    }

    widget = &newWidget;
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    const auto textSize =
      rmlib::Canvas::getTextSize(widget->text, widget->fontSize);

    Size result;

    if (textSize.x > constraints.max.width) {
      result.width = constraints.max.width;
    } else if (textSize.x < constraints.min.width) {
      result.width = constraints.min.width;
    } else {
      result.width = textSize.x;
    }

    if (textSize.y > constraints.max.height) {
      result.height = constraints.max.height;
    } else if (textSize.y < constraints.min.height) {
      result.height = constraints.min.height;
    } else {
      result.height = textSize.y;
    }

    return result;
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    const auto textSize =
      rmlib::Canvas::getTextSize(widget->text, widget->fontSize);
    const auto x = std::max(0, (rect.width() - textSize.x) / 2);
    const auto y = std::max(0, (rect.height() - textSize.y) / 2);

    const auto point = rect.topLeft + rmlib::Point{ x, y };

    // TODO: intersect with rect, fix size - 1:
    const auto drawRect = rmlib::Rect{ point, point + textSize } & rect;

    canvas.set(drawRect, rmlib::white);
    canvas.drawText(widget->text,
                    point,
                    widget->fontSize,
                    black,
                    white,
                    /* clip */ rect);
    return UpdateRegion{ drawRect };
  }

private:
  const Text* widget;
};

inline std::unique_ptr<RenderObject>
Text::createRenderObject() const {
  return std::make_unique<TextRenderObject>(*this);
}

} // namespace rmlib
