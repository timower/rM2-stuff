// Standalone smoke test for the libghostty-vt Nix packaging (Phase 1 of
// synthetic-prancing-pony.md). Not part of the apps/yaft migration itself --
// just proves the vendored .a + headers link and run correctly.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ghostty/vt.h>

int
main() {
  GhosttyTerminal terminal = nullptr;
  GhosttyResult r = ghostty_terminal_new(nullptr, &terminal, 80, 24);
  assert(r == GHOSTTY_SUCCESS);

  const char* content = "hi";
  ghostty_terminal_vt_write(
    terminal, reinterpret_cast<const uint8_t*>(content), strlen(content));

  GhosttyRenderState render_state = nullptr;
  r = ghostty_render_state_new(nullptr, &render_state);
  assert(r == GHOSTTY_SUCCESS);
  r = ghostty_render_state_update(render_state, terminal);
  assert(r == GHOSTTY_SUCCESS);

  GhosttyRenderStateRowIterator row_iter = nullptr;
  r = ghostty_render_state_row_iterator_new(nullptr, &row_iter);
  assert(r == GHOSTTY_SUCCESS);
  r = ghostty_render_state_get(
    render_state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &row_iter);
  assert(r == GHOSTTY_SUCCESS);

  GhosttyRenderStateRowCells cells = nullptr;
  r = ghostty_render_state_row_cells_new(nullptr, &cells);
  assert(r == GHOSTTY_SUCCESS);

  // Row 0 should be "hi..." -- check first two cells.
  int ok = ghostty_render_state_row_iterator_next(row_iter);
  assert(ok);
  r = ghostty_render_state_row_get(
    row_iter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells);
  assert(r == GHOSTTY_SUCCESS);

  const char expected[] = "hi";
  int col = 0;
  while (ghostty_render_state_row_cells_next(cells) && col < 2) {
    uint32_t grapheme_len = 0;
    ghostty_render_state_row_cells_get(
      cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN, &grapheme_len);
    assert(grapheme_len == 1);

    uint32_t codepoints[4] = { 0 };
    ghostty_render_state_row_cells_get(
      cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, codepoints);

    printf("cell[%d] codepoint = %c (0x%x), expected %c\n",
           col,
           static_cast<char>(codepoints[0]),
           codepoints[0],
           expected[col]);
    assert(codepoints[0] == static_cast<uint32_t>(expected[col]));
    col++;
  }
  assert(col == 2);

  ghostty_render_state_row_cells_free(cells);
  ghostty_render_state_row_iterator_free(row_iter);
  ghostty_render_state_free(render_state);
  ghostty_terminal_free(terminal);

  printf("SMOKE TEST PASSED\n");
  return 0;
}
