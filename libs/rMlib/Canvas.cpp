#include "Canvas.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace rmlib {

std::optional<ImageCanvas>
ImageCanvas::load(const char* path, int components) {
  Canvas result;
  result.memory = stbi_load(
    path, &result.width, &result.height, &result.components, components);
  if (result.memory == nullptr) {
    return std::nullopt;
  }

  if (components != 0) {
    result.components = components;
  }

  return ImageCanvas{ result };
}

std::optional<ImageCanvas>
ImageCanvas::load(uint8_t* data, int size, int components) {
  Canvas result;
  result.memory = stbi_load_from_memory(
    data, size, &result.width, &result.height, &result.components, components);
  if (result.memory == nullptr) {
    return std::nullopt;
  }

  if (components != 0) {
    result.components = components;
  }

  return ImageCanvas{ result };
}

void
ImageCanvas::release() {
  if (canvas.memory != nullptr) {
    stbi_image_free(canvas.memory);
  }
  canvas.memory = nullptr;
}

MemoryCanvas::MemoryCanvas(const Canvas& other) {
  memory = std::make_unique<uint8_t[]>(other.totalSize());
  canvas = other;
  canvas.memory = memory.get();
  copy(canvas, { 0, 0 }, other, other.rect());
}

} // namespace rmlib
