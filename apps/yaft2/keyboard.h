#pragma once

#include "UI.h"

struct KeyInfo;
struct EvKeyInfo;
struct terminal_t;

struct Layout;

class KeyboardRenderObject;

class Keyboard : public rmlib::Widget<KeyboardRenderObject> {
public:
  // default size:
  constexpr static int key_height = 64;
  constexpr static int key_width = 128;

  Keyboard(struct terminal_t* term, const Layout& layout)
    : term(term), layout(layout) {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const;

private:
  friend class KeyboardRenderObject;

  struct terminal_t* term;
  const Layout& layout;
};

class KeyboardRenderObject : public rmlib::LeafRenderObject<Keyboard> {
  using time_source = std::chrono::steady_clock;

public:
  KeyboardRenderObject(const Keyboard& keyboard);

  void update(const Keyboard& keyboard);

protected:
  void doRebuild(rmlib::AppContext& ctx, const rmlib::BuildContext&) final;

  rmlib::Size doLayout(const rmlib::Constraints& constraints) final;
  rmlib::UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) final;

  void handleInput(const rmlib::input::Event& ev) final;

private:
  void updateRepeat();

  void updateLayout();

  void sendKeyDown(const KeyInfo& key);
  void sendKeyDown(const EvKeyInfo& key);

  const KeyInfo* getKey(const rmlib::Point& point);

  void clearSticky();

  template<typename Ev>
  void handleTouchEvent(const Ev& ev);

  void handleKeyEvent(const rmlib::input::KeyEvent& ev);

  rmlib::UpdateRegion drawKey(rmlib::Point pos,
                              const KeyInfo& key,
                              rmlib::Canvas& canvas);

  // fields
  float keyWidth;
  float keyHeight;

  struct KeyState {
    int slot = -1;

    bool stuck = false; // Used for mod keys, get stuck after tap.
    bool held = false;  // Used for mod keys, held down after long press.

    bool dirty = false;

    bool isDown() const { return slot != -1 || stuck || held; }

    time_source::time_point nextRepeat;
  };

  const KeyInfo* shiftKey = nullptr;
  const KeyInfo* ctrlKey = nullptr;
  const KeyInfo* altKey = nullptr;
  std::unordered_map<const KeyInfo*, KeyState> keyState;

  struct PhysKeyState {
    bool down = false;
    time_source::time_point nextRepeat;
  };
  std::unordered_map<const EvKeyInfo*, PhysKeyState> physKeyState;

  rmlib::TimerHandle repeatTimer;
};
