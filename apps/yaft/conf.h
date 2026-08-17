#pragma once

// App-level tunables, trimmed from vendor/libYaft/conf.h (the rest was
// libYaft-internal and no longer applies with libghostty-vt).

// Palette indices into color_list[] (see color.h).
enum {
  DEFAULT_FG = 7,
  DEFAULT_BG = 0,
  ACTIVE_CURSOR_COLOR = 2,
  PASSIVE_CURSOR_COLOR = 1,
};

enum {
  // Always draw even if vt is not active.
  BACKGROUND_DRAW = false,
  // Missing glyph fallback, single width: U+0020 (SPACE).
  SUBSTITUTE_HALF = 0x0020,
  // Missing glyph fallback, double width: U+003F (QUESTION MARK).
  SUBSTITUTE_WIDE = 0x003F,
  // Read stdin too (debug only; can conflict with launchers).
  USE_STDIN = false,
};
