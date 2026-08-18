#include "terminal_adapter.h"

#include "color.h"
#include "conf.h"

#include <array>
#include <cstring>
#include <utility>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr int kXMargin = 2;
constexpr int kYMargin = 2;
constexpr uint32_t kUcs2Chars = 0x10000;
constexpr uint32_t kDefaultChar = 0x20; // SPACE, used for empty cells.

void
computeGeometry(int pixelWidth,
                int pixelHeight,
                int& cols,
                int& lines,
                int& marginTop,
                int& marginLeft) {
  cols = (pixelWidth - kXMargin) / CELL_WIDTH;
  lines = (pixelHeight - kYMargin) / CELL_HEIGHT;
  marginTop = (pixelHeight - lines * CELL_HEIGHT) / 2;
  marginLeft = (pixelWidth - cols * CELL_WIDTH) / 2;
}

constexpr uint32_t
pack(GhosttyColorRgb c) {
  return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | uint32_t(c.b);
}

// codepoint -> glyph_t* lookup, mirroring what libYaft's term_init() built
// into term->glyph[UCS2_CHARS]/term->bold_glyph[UCS2_CHARS]. Shared across
// all Terminal instances since it's a pure function of the static font data.
struct GlyphTable {
  std::array<const glyph_t*, kUcs2Chars> regular{};
  std::array<const glyph_t*, kUcs2Chars> bold{};
};

const GlyphTable&
glyphTable() {
  static const GlyphTable table = [] {
    GlyphTable t;
    for (const auto& g : glyphs) {
      if (g.code < kUcs2Chars) {
        t.regular[g.code] = &g;
      }
    }
    for (const auto& g : bold_glyphs) {
      if (g.code < kUcs2Chars) {
        t.bold[g.code] = &g;
      }
    }
    return t;
  }();
  return table;
}

// Resolves a codepoint + desired cell width (1 or 2) to glyph bitmaps,
// mirroring libYaft's addch() substitution logic. Codepoints outside the
// UCS2 table or with a mismatched width fall back to SUBSTITUTE_HALF/WIDE.
void
resolveGlyphs(uint32_t codepoint,
              int width,
              const glyph_t*& regular,
              const glyph_t*& bold) {
  if (codepoint == 0) {
    codepoint = kDefaultChar;
  }

  const auto& table = glyphTable();
  const glyph_t* g =
    codepoint < kUcs2Chars ? table.regular[codepoint] : nullptr;

  if (g == nullptr || g->width != width) {
    g = table.regular[width == 1 ? SUBSTITUTE_HALF : SUBSTITUTE_WIDE];
    regular = g;
    bold = g;
    return;
  }

  regular = g;
  bold = table.bold[codepoint] != nullptr ? table.bold[codepoint] : g;
}

void
writePtyCallback(GhosttyTerminal /*terminal*/,
                 void* userdata,
                 const uint8_t* data,
                 size_t len) {
  static_cast<Terminal*>(userdata)->write(data, len);
}

} // namespace

Terminal::Terminal(int pixelWidth, int pixelHeight) {
  computeGeometry(pixelWidth, pixelHeight, cols, lines, marginTop, marginLeft);
  width = pixelWidth;
  height = pixelHeight;

  ghostty_terminal_new(nullptr, &term_, uint16_t(cols), uint16_t(lines));

  std::array<GhosttyColorRgb, 256> palette{};
  for (int i = 0; i < 256; i++) {
    uint32_t c = color_list[i];
    palette[i] = { uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c) };
  }
  ghostty_terminal_set(
    term_, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, palette.data());
  ghostty_terminal_set(
    term_, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &palette[DEFAULT_FG]);
  ghostty_terminal_set(
    term_, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &palette[DEFAULT_BG]);

  constexpr const char* kTermName = "xterm-256color";
  GhosttyString termName{ reinterpret_cast<const uint8_t*>(kTermName),
                          strlen(kTermName) };
  ghostty_terminal_set(term_, GHOSTTY_TERMINAL_OPT_TERMINFO_NAME, &termName);

  ghostty_terminal_set(term_, GHOSTTY_TERMINAL_OPT_USERDATA, this);
  ghostty_terminal_set(term_,
                       GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                       reinterpret_cast<const void*>(writePtyCallback));

  ghostty_render_state_new(nullptr, &renderState_);
  ghostty_render_state_row_iterator_new(nullptr, &rowIter_);
  ghostty_render_state_row_cells_new(nullptr, &cells_);

  updateRenderState();

  ghostty_key_encoder_new(nullptr, &keyEncoder_);
  ghostty_key_event_new(nullptr, &keyEvent_);

  ghostty_mouse_encoder_new(nullptr, &mouseEncoder_);
  ghostty_mouse_event_new(nullptr, &mouseEvent_);
  constexpr bool kTrackLastCell = true;
  ghostty_mouse_encoder_setopt(
    mouseEncoder_, GHOSTTY_MOUSE_ENCODER_OPT_TRACK_LAST_CELL, &kTrackLastCell);
  updateMouseEncoderSize();
}

Terminal::~Terminal() {
  ghostty_mouse_event_free(mouseEvent_);
  ghostty_mouse_encoder_free(mouseEncoder_);
  ghostty_key_event_free(keyEvent_);
  ghostty_key_encoder_free(keyEncoder_);

  ghostty_render_state_row_cells_free(cells_);
  ghostty_render_state_row_iterator_free(rowIter_);
  ghostty_render_state_free(renderState_);
  ghostty_terminal_free(term_);
}

void
Terminal::updateRenderState() {
  ghostty_render_state_update(renderState_, term_);
  // Refreshed here (not just in beginDraw()) so cursor state is current as
  // soon as state changes, not only at the next draw pass -- the widget
  // layer needs the up-to-date position to notice cursor-only movement
  // (e.g. arrow keys) that leaves no other ghostty-tracked dirty state.
  refreshCursor();
}

void
Terminal::refreshCursor() {
  bool visible = false;
  ghostty_terminal_get(term_, GHOSTTY_TERMINAL_DATA_CURSOR_VISIBLE, &visible);
  cursorVisible = visible;

  uint16_t cx = 0;
  uint16_t cy = 0;
  ghostty_terminal_get(term_, GHOSTTY_TERMINAL_DATA_CURSOR_X, &cx);
  ghostty_terminal_get(term_, GHOSTTY_TERMINAL_DATA_CURSOR_Y, &cy);
  cursorX = cx;
  cursorY = cy;
}

void
Terminal::parse(const uint8_t* data, size_t len) {
  ghostty_terminal_vt_write(term_, data, len);
  updateRenderState();
}

void
Terminal::write(const uint8_t* data, size_t len) {
  ::write(fd, data, len);
}

void
Terminal::resize(int pixelWidth, int pixelHeight, bool report) {
  if (pixelWidth == width && pixelHeight == height) {
    return;
  }

  computeGeometry(pixelWidth, pixelHeight, cols, lines, marginTop, marginLeft);
  width = pixelWidth;
  height = pixelHeight;

  ghostty_terminal_resize(
    term_, uint16_t(cols), uint16_t(lines), CELL_WIDTH, CELL_HEIGHT);
  updateRenderState();
  updateMouseEncoderSize();

  if (report) {
    struct winsize ws{};
    ws.ws_col = uint16_t(cols);
    ws.ws_row = uint16_t(lines);
    ws.ws_xpixel = uint16_t(CELL_WIDTH * cols);
    ws.ws_ypixel = uint16_t(CELL_HEIGHT * lines);
    ioctl(fd, TIOCSWINSZ, &ws);
  }
}

void
Terminal::consumeShouldClear() {
  shouldClear = false;
  GhosttyRenderStateDirty clean = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
  ghostty_render_state_set(
    renderState_, GHOSTTY_RENDER_STATE_OPTION_DIRTY, &clean);
}

bool
Terminal::hasPendingDirty() const {
  GhosttyRenderStateDirty dirty = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
  ghostty_render_state_get(
    renderState_, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty);
  return dirty != GHOSTTY_RENDER_STATE_DIRTY_FALSE;
}

bool
Terminal::mouseTrackingEnabled() const {
  bool tracking = false;
  ghostty_terminal_get(term_, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &tracking);
  return tracking;
}

void
Terminal::sendKey(GhosttyKeyAction action,
                  GhosttyKey key,
                  GhosttyMods mods,
                  std::string_view utf8,
                  uint32_t unshiftedCodepoint) {
  ghostty_key_encoder_setopt_from_terminal(keyEncoder_, term_);
  // Keep the Kitty keyboard protocol off even if a client negotiated it:
  // our keys never carry a real physical-key identity (see below), so
  // switching encodings on negotiation only breaks well-known bindings
  // like Ctrl+C/Ctrl+D in clients that assume CSI u once negotiated.
  constexpr GhosttyKittyKeyFlags kKittyDisabled = GHOSTTY_KITTY_KEY_DISABLED;
  ghostty_key_encoder_setopt(
    keyEncoder_, GHOSTTY_KEY_ENCODER_OPT_KITTY_FLAGS, &kKittyDisabled);

  ghostty_key_event_set_action(keyEvent_, action);
  ghostty_key_event_set_key(keyEvent_, key);
  ghostty_key_event_set_mods(keyEvent_, mods);
  ghostty_key_event_set_utf8(keyEvent_, utf8.data(), utf8.size());
  ghostty_key_event_set_unshifted_codepoint(keyEvent_, unshiftedCodepoint);

  std::array<char, 64> buf{};
  size_t written = 0;
  if (ghostty_key_encoder_encode(
        keyEncoder_, keyEvent_, buf.data(), buf.size(), &written) ==
        GHOSTTY_SUCCESS &&
      written > 0) {
    write(reinterpret_cast<const uint8_t*>(buf.data()), written);
  }
}

void
Terminal::sendMouseButton(GhosttyMouseAction action,
                          GhosttyMouseButton button,
                          float x,
                          float y,
                          bool anyButtonPressed) {
  ghostty_mouse_event_set_action(mouseEvent_, action);
  ghostty_mouse_event_set_button(mouseEvent_, button);
  ghostty_mouse_event_set_position(mouseEvent_, { x, y });
  ghostty_mouse_event_set_mods(mouseEvent_, 0);
  encodeAndWriteMouse(anyButtonPressed);
}

void
Terminal::sendMouseMotion(float x, float y, bool anyButtonPressed) {
  ghostty_mouse_event_set_action(mouseEvent_, GHOSTTY_MOUSE_ACTION_MOTION);
  ghostty_mouse_event_clear_button(mouseEvent_);
  ghostty_mouse_event_set_position(mouseEvent_, { x, y });
  ghostty_mouse_event_set_mods(mouseEvent_, 0);
  encodeAndWriteMouse(anyButtonPressed);
}

void
Terminal::encodeAndWriteMouse(bool anyButtonPressed) {
  ghostty_mouse_encoder_setopt_from_terminal(mouseEncoder_, term_);
  ghostty_mouse_encoder_setopt(mouseEncoder_,
                               GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED,
                               &anyButtonPressed);

  std::array<char, 32> buf{};
  size_t written = 0;
  if (ghostty_mouse_encoder_encode(
        mouseEncoder_, mouseEvent_, buf.data(), buf.size(), &written) ==
        GHOSTTY_SUCCESS &&
      written > 0) {
    write(reinterpret_cast<const uint8_t*>(buf.data()), written);
  }
}

void
Terminal::updateMouseEncoderSize() {
  GhosttyMouseEncoderSize size{};
  size.size = sizeof(size);
  size.screen_width = uint32_t(width);
  size.screen_height = uint32_t(height);
  size.cell_width = uint32_t(CELL_WIDTH);
  size.cell_height = uint32_t(CELL_HEIGHT);
  size.padding_top = uint32_t(marginTop);
  size.padding_left = uint32_t(marginLeft);
  size.padding_bottom = uint32_t(height - marginTop - lines * CELL_HEIGHT);
  size.padding_right = uint32_t(width - marginLeft - cols * CELL_WIDTH);
  ghostty_mouse_encoder_setopt(
    mouseEncoder_, GHOSTTY_MOUSE_ENCODER_OPT_SIZE, &size);
}

void
Terminal::beginDraw() {
  ghostty_render_state_get(
    renderState_, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &rowIter_);

  GhosttyRenderStateDirty dirty = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
  ghostty_render_state_get(
    renderState_, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty);
  shouldClear = dirty == GHOSTTY_RENDER_STATE_DIRTY_FULL;

  colors_ = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
  ghostty_render_state_colors_get(renderState_, &colors_);
}

bool
Terminal::nextRow() {
  if (!ghostty_render_state_row_iterator_next(rowIter_)) {
    return false;
  }
  ghostty_render_state_row_get(
    rowIter_, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells_);
  return true;
}

bool
Terminal::isRowDirty() const {
  bool dirty = false;
  ghostty_render_state_row_get(
    rowIter_, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &dirty);
  return dirty;
}

void
Terminal::clearRowDirty() {
  bool clean = false;
  ghostty_render_state_row_set(
    rowIter_, GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);
}

GhosttyCellWide
Terminal::wideAt(int col) {
  ghostty_render_state_row_cells_select(cells_, uint16_t(col));
  GhosttyCell raw = 0;
  ghostty_render_state_row_cells_get(
    cells_, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw);
  GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
  ghostty_cell_get(raw, GHOSTTY_CELL_DATA_WIDE, &wide);
  return wide;
}

TermCell
Terminal::resolveCell(int col, GhosttyCellWide wide) {
  ghostty_render_state_row_cells_select(cells_, uint16_t(col));

  uint32_t graphemeLen = 0;
  ghostty_render_state_row_cells_get(
    cells_, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN, &graphemeLen);

  uint32_t codepoint = 0;
  if (graphemeLen > 0) {
    graphemeBuf_.resize(graphemeLen);
    ghostty_render_state_row_cells_get(
      cells_,
      GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF,
      graphemeBuf_.data());
    codepoint = graphemeBuf_[0];
  }

  GhosttyStyle style = GHOSTTY_INIT_SIZED(GhosttyStyle);
  ghostty_render_state_row_cells_get(
    cells_, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);

  GhosttyColorRgb fgRgb{};
  GhosttyColorRgb bgRgb{};
  const bool hasFg =
    ghostty_render_state_row_cells_get(
      cells_, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fgRgb) ==
    GHOSTTY_SUCCESS;
  const bool hasBg =
    ghostty_render_state_row_cells_get(
      cells_, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bgRgb) ==
    GHOSTTY_SUCCESS;

  TermCell cell;
  cell.fg = pack(hasFg ? fgRgb : colors_.foreground);
  cell.bg = pack(hasBg ? bgRgb : colors_.background);
  if (style.inverse) {
    std::swap(cell.fg, cell.bg);
  }
  cell.bold = style.bold;
  cell.underline = style.underline != GHOSTTY_SGR_UNDERLINE_NONE;

  const int cellWidth = wide == GHOSTTY_CELL_WIDE_WIDE ? 2 : 1;
  resolveGlyphs(codepoint, cellWidth, cell.glyph, cell.boldGlyph);
  cell.width = cellWidth == 2 ? CellWidth::Wide : CellWidth::Half;
  return cell;
}

TermCell
Terminal::blankCell() {
  TermCell cell;
  resolveGlyphs(kDefaultChar, 1, cell.glyph, cell.boldGlyph);
  cell.fg = pack(colors_.foreground);
  cell.bg = pack(colors_.background);
  return cell;
}

TermCell
Terminal::cellAt(int col) {
  const auto wide = wideAt(col);

  // Tail of a wide char: ghostty says "do not render"; reuse the head
  // cell's glyph/colors (mirrors libYaft's copy_cell() into NEXT_TO_WIDE).
  if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL && col > 0) {
    auto cell = resolveCell(col - 1, wideAt(col - 1));
    cell.width = CellWidth::NextToWide;
    return cell;
  }

  // Spacer at the end of a soft-wrapped line for a wide char that didn't
  // fit: not visible content.
  if (wide == GHOSTTY_CELL_WIDE_SPACER_HEAD) {
    return blankCell();
  }

  return resolveCell(col, wide);
}
