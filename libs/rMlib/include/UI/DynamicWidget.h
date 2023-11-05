#pragma once

#include <UI/RenderObject.h>
#include <UI/TypeID.h>

namespace rmlib {

class DynamicWidget {
private:
  struct DynamicRenderObject : public SingleChildRenderObject<DynamicWidget> {
    DynamicRenderObject(std::unique_ptr<RenderObject> ro)
      : SingleChildRenderObject(nullptr, std::move(ro)) {}

    RenderObject& getChild() { return *child; }

    void setChild(std::unique_ptr<RenderObject> ro) {
      markNeedsDraw(true);
      child = std::move(ro);
    }
  };

  struct DynamicWidgetBase {
    virtual std::unique_ptr<RenderObject> createRenderObject() const = 0;
    virtual void update(RenderObject& ro) const = 0;
    virtual ~DynamicWidgetBase() = default;
  };

  template<typename W>
  struct DynamicWidgetImpl : public DynamicWidgetBase {
    DynamicWidgetImpl(W w) : widget(std::move(w)) {}

    std::unique_ptr<RenderObject> createRenderObject() const final {
      return std::make_unique<DynamicRenderObject>(widget.createRenderObject());
    }

    void update(RenderObject& ro) const final {
      auto& dynRo = *dynamic_cast<DynamicRenderObject*>(&ro);
      if (dynRo.getChild().getWidgetTypeID() != type_id::typeId<W>()) {
        dynRo.setChild(widget.createRenderObject());
      } else {
        widget.update(dynRo.getChild());
      }
    }

  private:
    W widget;
  };

public:
  template<typename W>
  DynamicWidget(W w)
    : mWidget(std::make_unique<DynamicWidgetImpl<W>>(std::move(w))) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return mWidget->createRenderObject();
  }

  void update(RenderObject& ro) const { mWidget->update(ro); }

private:
  // TODO: shared pointer? A widget should be copyable...
  std::unique_ptr<DynamicWidgetBase> mWidget;
};

} // namespace rmlib
