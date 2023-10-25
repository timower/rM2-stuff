#pragma once

#include "Canvas.h"
#include "Error.h"

namespace rmlib::fb {

// Waveform ints that match rm2 'actual' updates
enum class Waveform { DU = 0, GC16 = 1, GC16Fast = 2, A2 = 3 };

enum UpdateFlags { None = 0, FullRefresh = 1, Sync = 2, Priority = 4 };

struct FrameBuffer {
  enum Type { rM1, Shim, rM2Stuff };

  /// Opens the framebuffer.
  static ErrorOr<FrameBuffer> open();

  FrameBuffer(FrameBuffer&& other)
    : type(other.type), fd(other.fd), canvas(other.canvas) {
    other.fd = -1;
    other.canvas = Canvas{};
  }

  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;
  FrameBuffer& operator=(FrameBuffer&& other) {
    close();
    std::swap(other, *this);
    return *this;
  }

  /// Closes the framebuffer and unmaps the memory.
  ~FrameBuffer() { close(); }

  void doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const;

  void drawText(std::string_view text,
                Point location,
                int size = default_text_size,
                Waveform waveform = Waveform::GC16Fast,
                UpdateFlags flags = UpdateFlags::None) {
    auto textSize = Canvas::getTextSize(text, size);
    canvas.drawText(text, location, size);
    doUpdate({ location, location + textSize }, waveform, flags);
  }

  void clear() {
    canvas.set(0xFFFF);
    doUpdate(canvas.rect(), Waveform::GC16Fast, UpdateFlags::None);
  }

  // members
  Type type;
  int fd = -1;
  Canvas canvas;

private:
  FrameBuffer(Type type, int fd, Canvas canvas)
    : type(type), fd(fd), canvas(std::move(canvas)) {}

  void close();

  static ErrorOr<Type> detectType();
};

} // namespace rmlib::fb
