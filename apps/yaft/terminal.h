#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* See LICENSE for licence details. */
void
erase_cell(struct terminal_t* term, int y, int x);

void
copy_cell(struct terminal_t* term, int dst_y, int dst_x, int src_y, int src_x);

void
scroll(struct terminal_t* term, int from, int to, int offset);

/* relative movement: cause scrolling */
void
move_cursor(struct terminal_t* term, int y_offset, int x_offset);

/* absolute movement: never scroll */
void
set_cursor(struct terminal_t* term, int y, int x);

void
addch(struct terminal_t* term, uint32_t code);

void
reset_esc(struct terminal_t* term);

bool
push_esc(struct terminal_t* term, uint8_t ch);

void
reset_charset(struct terminal_t* term);

void
reset(struct terminal_t* term);

void
redraw(struct terminal_t* term);

void
term_die(struct terminal_t* term);

bool
term_init(struct terminal_t* term, int width, int height);

#ifdef __cplusplus
}
#endif
