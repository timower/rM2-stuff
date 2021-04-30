#pragma once

#include <UI/RenderObject.h>
#include <UI/TypeID.h>

namespace rmlib {

class DynamicWidget {
private:
  // TODO: have common base class with SSRO -> proxyRO
  struct DynamicRenderObject : public RenderObject {
    DynamicRenderObject(std::unique_ptr<RenderObject> ro,
                        typeID::type_id_t typeID)
      : RenderObject(typeID), child(std::move(ro)) {}

    RenderObject& getChild() { return *child; }

    void setChild(std::unique_ptr<RenderObject> ro, typeID::type_id_t typeID) {
      child = std::move(ro);
      this->mTypeID = typeID;
    }

    void handleInput(const rmlib::input::Event& ev) override {
      child->handleInput(ev);
    }

    UpdateRegion cleanup(rmlib::Canvas& canvas) override {
      if (isFullDraw()) {
        return RenderObject::cleanup(canvas);
      }

      return child->cleanup(canvas);
    }

    void markNeedsLayout() override {
      RenderObject::markNeedsLayout();
      if (child) {
        child->markNeedsLayout();
      }
    }

    void markNeedsDraw(bool full = true) override {
      RenderObject::markNeedsDraw(full);
      if (child) {
        child->markNeedsDraw(full);
      }
    }

    void rebuild(AppContext& context, const BuildContext* parent) final {
      // RenderObject::rebuild(context, parent);
      // const auto buildCtx = BuildContext{ *this, parent };
      child->rebuild(context, parent);
    }

    void reset() override {
      RenderObject::reset();
      child->reset();
    }

  protected:
    Size doLayout(const Constraints& constraints) override {
      return child->layout(constraints);
    }

    UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
      return child->draw(rect, canvas);
    }

    bool getNeedsDraw() const override {
      return RenderObject::getNeedsDraw() || child->needsDraw();
    }

    bool getNeedsLayout() const override {
      return RenderObject::getNeedsLayout() || child->needsLayout();
    }

  private:
    std::unique_ptr<RenderObject> child;
  };

  struct DynamicWidgetBase {
    virtual std::unique_ptr<RenderObject> createRenderObject() const = 0;
    virtual void update(RenderObject& RO) const = 0;
    virtual ~DynamicWidgetBase() = default;
  };

  template<typename W>
  struct DynamicWidgetImpl : public DynamicWidgetBase {
    DynamicWidgetImpl(W w) : widget(std::move(w)) {}

    std::unique_ptr<RenderObject> createRenderObject() const final {
      return std::make_unique<DynamicRenderObject>(widget.createRenderObject(),
                                                   typeID::type_id<W>());
    }

    void update(RenderObject& ro) const final {
      auto& dynRo = *static_cast<DynamicRenderObject*>(&ro);
      if (dynRo.getWidgetTypeID() != typeID::type_id<W>()) {
        dynRo.setChild(widget.createRenderObject(), typeID::type_id<W>());
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

  void update(RenderObject& RO) const { mWidget->update(RO); }

private:
  std::unique_ptr<DynamicWidgetBase> mWidget;
};

} // namespace rmlib
