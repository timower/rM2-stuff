#pragma once

#include <Input.h>

#include <UI/BuildContext.h>
#include <UI/TypeID.h>
#include <UI/Util.h>

namespace rmlib {

class AppContext;
class RenderObject;

struct BuildContext {
  const RenderObject& renderObject;
  const BuildContext* parent;

  BuildContext(const RenderObject& ro, const BuildContext* parent)
    : renderObject(ro), parent(parent) {}

  template<typename RO>
  const RO* getRenderObject() const;
};

class RenderObject {
  static int roCount;

public:
  RenderObject(typeID::type_id_t typeID) : mTypeID(typeID), mID(roCount++) {
#ifndef NDEBUG
    std::cout << "alloc RO: " << mID << "\n";
#endif
  }

  virtual ~RenderObject() {
#ifndef NDEBUG
    std::cout << "free RO: " << mID << "\n";
#endif
    roCount--;
  }

  Size layout(const Constraints& constraints) {
    if (needsLayout()) {
      const auto result = doLayout(constraints);
      assert(result.width != Constraints::unbound &&
             result.height != Constraints::unbound);
      assert(constraints.contain(result));

      mNeedsLayout = false;

      lastSize = result;
      return result;
    }
    return lastSize;
  }

  virtual UpdateRegion cleanup(rmlib::Canvas& canvas) {
    if (isFullDraw()) {
      canvas.set(this->rect, rmlib::white);
      return UpdateRegion{ this->rect, rmlib::fb::Waveform::DU };
    }
    return {};
  }

  UpdateRegion draw(rmlib::Rect rect, rmlib::Canvas& canvas) {
    auto result = UpdateRegion{};

    // TODO: do we need to distinguish when cleanup is used?
    if (needsDraw()) {
      this->rect = rect;
      result |= doDraw(rect, canvas);

      mNeedsDraw = No;
    }

    return result;
  }

  virtual void handleInput(const rmlib::input::Event& ev) {}

  virtual void rebuild(AppContext& context, const BuildContext* parent) {
    buildContext.emplace(*this, parent);

    if (mNeedsRebuild) {
#ifndef NDEBUG
      mInRebuild = true;
#endif
      doRebuild(context, *buildContext);
      mNeedsRebuild = false;

#ifndef NDEBUG
      mInRebuild = false;
#endif
    }
  }

  bool needsDraw() {
    return needsDrawCache.getOrSetTo([this] { return getNeedsDraw(); });
  }

  bool needsLayout() {
    return needsLayoutCache.getOrSetTo([this] { return getNeedsLayout(); });
  }

  const rmlib::Rect& getRect() const { return rect; }

  const Size& getSize() const { return lastSize; }

  virtual void markNeedsLayout() { mNeedsLayout = true; }
  virtual void markNeedsDraw(bool full = true) {
    mNeedsDraw = full ? Full : (mNeedsDraw == No ? Partial : mNeedsDraw);
  }

  void markNeedsRebuild() {
    assert(!mInRebuild && "Don't mark rebuild from within rebuild()");
    mNeedsRebuild = true;
  }

  virtual void reset() {
    needsLayoutCache.reset();
    needsDrawCache.reset();
  }

  typeID::type_id_t getWidgetTypeID() const { return mTypeID; }

protected:
  virtual Size doLayout(const Constraints& Constraints) = 0;
  virtual UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) = 0;
  virtual void doRebuild(AppContext& context,
                         const BuildContext& buildContext) {}

  virtual bool getNeedsDraw() const { return mNeedsDraw != No; }
  virtual bool getNeedsLayout() const { return mNeedsLayout; }

  bool isPartialDraw() const { return mNeedsDraw == Partial; }
  bool isFullDraw() const { return mNeedsDraw == Full; }

  typeID::type_id_t mTypeID;
  std::optional<BuildContext> buildContext;

private:
  int mID;
  rmlib::Rect rect;

  Size lastSize = { 0, 0 };

  // TODO: are both needed?
  CachedBool needsLayoutCache;
  bool mNeedsLayout = true;

  CachedBool needsDrawCache;
  // bool mNeedsDraw = true;
  enum { No, Full, Partial } mNeedsDraw = Full;

  bool mNeedsRebuild = false;

#ifndef NDEBUG
protected:
  bool mInRebuild = false;
#endif
};

int RenderObject::roCount = 0;

template<typename Widget>
class LeafRenderObject : public RenderObject {
public:
  using WidgetType = Widget;

  LeafRenderObject(const Widget& widget)
    : RenderObject(typeID::type_id<Widget>()), widget(&widget) {}

protected:
  const Widget* widget;
};

template<typename Widget>
class SingleChildRenderObject : public RenderObject {
public:
  using WidgetType = Widget;

  SingleChildRenderObject(const Widget& widget)
    : RenderObject(typeID::type_id<Widget>())
    , widget(&widget)
    , child(widget.child.createRenderObject()) {}

  SingleChildRenderObject(const Widget& widget,
                          std::unique_ptr<RenderObject> child)
    : RenderObject(typeID::type_id<Widget>())
    , widget(&widget)
    , child(std::move(child)) {}

  // SingleChildRenderObject(std::unique_ptr<RenderObject> child)
  //   : SingleChildRenderObject(), child(std::move(child)) {}

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
    RenderObject::rebuild(context, parent);
#ifndef NDEBUG
    mInRebuild = true;
#endif
    child->rebuild(context, &*buildContext);
#ifndef NDEBUG
    mInRebuild = false;
#endif
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

  const Widget* widget;
  std::unique_ptr<RenderObject> child;
};

template<typename Widget>
std::vector<std::unique_ptr<RenderObject>>
getChildren(const std::vector<Widget>& widgets) {

  std::vector<std::unique_ptr<RenderObject>> children;
  children.reserve(widgets.size());

  std::transform(widgets.begin(),
                 widgets.end(),
                 std::back_inserter(children),
                 [](const auto& child) { return child.createRenderObject(); });

  return children;
}

template<typename Widget>
class MultiChildRenderObject : public RenderObject {
public:
  using WidgetType = Widget;

  MultiChildRenderObject(std::vector<std::unique_ptr<RenderObject>> children)
    : RenderObject(typeID::type_id<Widget>()), children(std::move(children)) {}

  void handleInput(const rmlib::input::Event& ev) override {
    for (const auto& child : children) {
      child->handleInput(ev);
    }
  }

  UpdateRegion cleanup(rmlib::Canvas& canvas) override {
    if (isFullDraw()) {
      return RenderObject::cleanup(canvas);
    }

    auto result = UpdateRegion{};
    for (const auto& child : children) {
      result |= child->cleanup(canvas);
    }
    return result;
  }

  void markNeedsLayout() override {
    RenderObject::markNeedsLayout();
    for (auto& child : children) {
      child->markNeedsLayout();
    }
  }

  void markNeedsDraw(bool full = true) override {
    RenderObject::markNeedsDraw(full);
    for (auto& child : children) {
      child->markNeedsDraw(full);
    }
  }

  void rebuild(AppContext& context, const BuildContext* parent) final {
    RenderObject::rebuild(context, parent);

#ifndef NDEBUG
    mInRebuild = true;
#endif
    for (const auto& child : children) {
      child->rebuild(context, &*buildContext);
    }

#ifndef NDEBUG
    mInRebuild = false;
#endif
  }

  void reset() override {
    RenderObject::reset();
    for (const auto& child : children) {
      child->reset();
    }
  }

protected:
  // template<typename Widget>
  void updateChildren(const Widget& widget, const Widget& newWidget) {

    auto updateEnd = children.size();

    if (newWidget.children.size() != children.size()) {
      // TODO: move the ROs out of tree and reuse?
      if (newWidget.children.size() < children.size()) {
        children.resize(newWidget.children.size());
        updateEnd = newWidget.children.size();
      } else {
        children.reserve(newWidget.children.size());
        std::transform(
          std::next(newWidget.children.begin(), updateEnd),
          newWidget.children.end(),
          std::back_inserter(children),
          [](const auto& child) { return child.createRenderObject(); });
      }
    }

    for (size_t i = 0; i < updateEnd; i++) {
      newWidget.children[i].update(*children[i]);
    }
  }

  bool getNeedsDraw() const override {
    return RenderObject::getNeedsDraw() ||
           std::any_of(children.begin(), children.end(), [](const auto& child) {
             return child->needsDraw();
           });
  }

  bool getNeedsLayout() const override {
    return RenderObject::getNeedsLayout() ||
           std::any_of(children.begin(), children.end(), [](const auto& child) {
             return child->needsLayout();
           });
  }

  std::vector<std::unique_ptr<RenderObject>> children;
};

template<typename RO>
const RO*
BuildContext::getRenderObject() const {
  const auto widgetTypeID = typeID::type_id<typename RO::WidgetType>();

  if (renderObject.getWidgetTypeID() == widgetTypeID) {
    return static_cast<const RO*>(&renderObject);
  }

  if (parent == nullptr) {
    return nullptr;
  }

  return parent->getRenderObject<RO>();
}

} // namespace rmlib
