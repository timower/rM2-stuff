#pragma once

#include "glyph.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <ghostty/vt.h>

enum class CellWidth { Half, Wide, NextToWide };

struct TermCell {
  const glyph_t* glyph = nullptr;
  const glyph_t* boldGlyph = nullptr;
  uint32_t fg = 0; // packed 0xRRGGBB
  uint32_t bg = 0; // packed 0xRRGGBB
  bool bold = false;
  bool underline = false;
  CellWidth width = CellWidth::Half;
  bool hasPixmap = false; // always false; no sixel/kitty-graphics rendering.
};

// Adapter around libghostty-vt: owns the terminal, its render state, the pty
// fd, and the codepoint -> bitmap-glyph lookup. Exposes roughly the same
// shape the app relied on from libYaft's terminal_t.
class Terminal {
public:
  Terminal(int pixelWidth, int pixelHeight);
  ~Terminal();

  Terminal(const Terminal&) = delete;
  Terminal& operator=(const Terminal&) = delete;

  // Feeds pty-output bytes through the VT parser, updating terminal state.
  void parse(const uint8_t* data, size_t len);

  // Writes bytes to the pty (keyboard/mouse input).
  void write(const uint8_t* data, size_t len);

  void resize(int pixelWidth, int pixelHeight, bool report);

  int fd = -1;

  int cols = 0;
  int lines = 0;
  int width = 0;
  int height = 0;
  int marginTop = 0;
  int marginLeft = 0;

  // Set when ghostty reports the whole frame changed (e.g. screen clear).
  // Mirrors the old terminal_t::shouldClear full-refresh hint.
  bool shouldClear = false;
  void consumeShouldClear();

  // Whether anything is dirty at all (used outside of a draw pass to decide
  // if a redraw needs scheduling).
  bool hasPendingDirty() const;

  bool cursorVisible = false;
  int cursorX = 0;
  int cursorY = 0;

  bool isAppCursorMode() const;
  bool mouseTrackingEnabled() const;
  bool mouseMoveMode() const;

  // Per-frame row iteration: call beginDraw() once, then nextRow() for each
  // line in order. Matches doDraw()'s existing sequential line loop.
  void beginDraw();
  bool nextRow();
  bool isRowDirty() const;
  void clearRowDirty();
  TermCell cellAt(int col);

private:
  void updateRenderState();
  void refreshCursor();
  GhosttyCellWide wideAt(int col);
  TermCell resolveCell(int col, GhosttyCellWide wide);
  TermCell blankCell();

  GhosttyTerminal term_ = nullptr;
  GhosttyRenderState renderState_ = nullptr;
  GhosttyRenderStateRowIterator rowIter_ = nullptr;
  GhosttyRenderStateRowCells cells_ = nullptr;
  GhosttyRenderStateColors colors_{};

  // Reused scratch buffer for multi-codepoint grapheme clusters (rare; only
  // the base codepoint is actually rendered).
  std::vector<uint32_t> graphemeBuf_;
};
