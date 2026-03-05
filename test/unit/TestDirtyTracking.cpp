#include <catch2/catch_test_macros.hpp>

#include "terminal.h"
#include "yaft.h"

// Minimal stub for term_init dependencies
static const int TEST_WIDTH = 200;
static const int TEST_HEIGHT = 200;

static bool
initTestTerm(struct terminal_t& term) {
  memset(&term, 0, sizeof(term));
  return term_init(&term, TEST_WIDTH, TEST_HEIGHT);
}

TEST_CASE("Dirty tracking: initial state after term_init", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  // After term_init -> reset(), all lines should be full-width dirty
  for (int line = 0; line < term.lines; line++) {
    CHECK(term.line_dirty[line] == true);
    CHECK(term.col_dirty_min[line] == 0);
    CHECK(term.col_dirty_max[line] == term.cols - 1);
  }

  term_die(&term);
}

TEST_CASE("Dirty tracking: erase_cell marks specific column", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int testLine = 1;
  int testCol = 5;
  clear_line_dirty(&term, testLine);

  erase_cell(&term, testLine, testCol);

  CHECK(term.line_dirty[testLine] == true);
  CHECK(term.col_dirty_min[testLine] == testCol);
  CHECK(term.col_dirty_max[testLine] == testCol);

  term_die(&term);
}

TEST_CASE("Dirty tracking: copy_cell marks destination", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int srcY = 0, srcX = 3, dstY = 2, dstX = 7;
  // Set up a HALF-width source cell
  erase_cell(&term, srcY, srcX);
  clear_line_dirty(&term, dstY);

  copy_cell(&term, dstY, dstX, srcY, srcX);

  CHECK(term.line_dirty[dstY] == true);
  CHECK(term.col_dirty_min[dstY] == dstX);
  CHECK(term.col_dirty_max[dstY] == dstX);

  term_die(&term);
}

TEST_CASE("Dirty tracking: addch marks cursor column", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  // Place cursor at known position
  set_cursor(&term, 0, 0);
  clear_line_dirty(&term, 0);

  addch(&term, 'A');

  CHECK(term.line_dirty[0] == true);
  // addch at col 0 -> set_cell marks col 0, then cursor moves to col 1
  CHECK(term.col_dirty_min[0] == 0);
  CHECK(term.col_dirty_max[0] >= 0);

  term_die(&term);
}

TEST_CASE("Dirty tracking: scroll marks full width", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  // Clear all dirty state
  for (int i = 0; i < term.lines; i++)
    clear_line_dirty(&term, i);

  int from = 2, to = 5;
  scroll(&term, from, to, 1);

  for (int y = from; y <= to; y++) {
    CHECK(term.line_dirty[y] == true);
    CHECK(term.col_dirty_min[y] == 0);
    CHECK(term.col_dirty_max[y] == term.cols - 1);
  }

  term_die(&term);
}

TEST_CASE("Dirty tracking: redraw marks full width", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  for (int i = 0; i < term.lines; i++)
    clear_line_dirty(&term, i);

  redraw(&term);

  for (int i = 0; i < term.lines; i++) {
    CHECK(term.line_dirty[i] == true);
    CHECK(term.col_dirty_min[i] == 0);
    CHECK(term.col_dirty_max[i] == term.cols - 1);
  }

  term_die(&term);
}

TEST_CASE("Dirty tracking: reset marks full width", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  for (int i = 0; i < term.lines; i++)
    clear_line_dirty(&term, i);

  reset(&term);

  for (int line = 0; line < term.lines; line++) {
    CHECK(term.line_dirty[line] == true);
    CHECK(term.col_dirty_min[line] == 0);
    CHECK(term.col_dirty_max[line] == term.cols - 1);
  }

  term_die(&term);
}

TEST_CASE("Dirty tracking: multiple modifications expand range", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int testLine = 3;
  clear_line_dirty(&term, testLine);

  erase_cell(&term, testLine, 2);
  CHECK(term.col_dirty_min[testLine] == 2);
  CHECK(term.col_dirty_max[testLine] == 2);

  erase_cell(&term, testLine, 8);
  CHECK(term.col_dirty_min[testLine] == 2);
  CHECK(term.col_dirty_max[testLine] == 8);

  erase_cell(&term, testLine, 1);
  CHECK(term.col_dirty_min[testLine] == 1);
  CHECK(term.col_dirty_max[testLine] == 8);

  term_die(&term);
}

TEST_CASE("Dirty tracking: clearing dirty resets range", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int testLine = 2;
  erase_cell(&term, testLine, 5);
  clear_line_dirty(&term, testLine);

  CHECK(term.line_dirty[testLine] == false);
  CHECK(term.col_dirty_min[testLine] == -1);
  CHECK(term.col_dirty_max[testLine] == -1);

  // Re-dirtying starts fresh
  erase_cell(&term, testLine, 3);
  CHECK(term.col_dirty_min[testLine] == 3);
  CHECK(term.col_dirty_max[testLine] == 3);

  term_die(&term);
}

TEST_CASE("Dirty tracking: cursor column marking", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int testLine = 4;
  int testCol = 6;
  clear_line_dirty(&term, testLine);

  mark_col_dirty(&term, testLine, testCol);

  CHECK(term.line_dirty[testLine] == true);
  CHECK(term.col_dirty_min[testLine] == testCol);
  CHECK(term.col_dirty_max[testLine] == testCol);

  term_die(&term);
}

TEST_CASE("Dirty tracking: edge columns 0 and cols-1", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int testLine = 1;
  clear_line_dirty(&term, testLine);

  erase_cell(&term, testLine, 0);
  CHECK(term.col_dirty_min[testLine] == 0);
  CHECK(term.col_dirty_max[testLine] == 0);

  erase_cell(&term, testLine, term.cols - 1);
  CHECK(term.col_dirty_min[testLine] == 0);
  CHECK(term.col_dirty_max[testLine] == term.cols - 1);

  term_die(&term);
}

TEST_CASE("Dirty tracking: partial scroll only dirties scroll region",
          "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  // Clear all lines
  for (int i = 0; i < term.lines; i++)
    clear_line_dirty(&term, i);

  int from = 3, to = 6;
  REQUIRE(to < term.lines - 1); // ensure lines outside scroll region exist

  scroll(&term, from, to, 1);

  // Lines in scroll region should be dirty
  for (int y = from; y <= to; y++) {
    CHECK(term.line_dirty[y] == true);
    CHECK(term.col_dirty_min[y] == 0);
    CHECK(term.col_dirty_max[y] == term.cols - 1);
  }

  // Lines outside scroll region should remain clean
  for (int y = 0; y < from; y++) {
    CHECK(term.line_dirty[y] == false);
    CHECK(term.col_dirty_min[y] == -1);
  }
  for (int y = to + 1; y < term.lines; y++) {
    CHECK(term.line_dirty[y] == false);
    CHECK(term.col_dirty_min[y] == -1);
  }

  term_die(&term);
}

TEST_CASE("Dirty tracking: mark_line_full_dirty helper", "[dirty]") {
  struct terminal_t term;
  REQUIRE(initTestTerm(term));

  int testLine = 0;
  clear_line_dirty(&term, testLine);

  mark_line_full_dirty(&term, testLine);

  CHECK(term.line_dirty[testLine] == true);
  CHECK(term.col_dirty_min[testLine] == 0);
  CHECK(term.col_dirty_max[testLine] == term.cols - 1);

  term_die(&term);
}
