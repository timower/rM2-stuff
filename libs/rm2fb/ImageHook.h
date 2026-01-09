#pragma once

/// Returns a Qimage pointing at the shared framebuffer on first call,
/// if the size matches the framebuffer size.
void
qimageHook(void (*orig)(void*, int, int, int),
           void* that,
           int width,
           int height,
           int format);
