#include "keymap.h"
#include "layout.h"

#ifdef __APPLE__
#include "event-codes.h"
#else
#include <linux/input-event-codes.h>
#endif

const KeyMap qwerty_keymap = {
  { KEY_ESC, { Escape } },
  { KEY_1, { '1', '!' } },
  { KEY_2, { '2', '@' } },
  { KEY_3, { '3', '#' } },
  { KEY_4, { '4', '$' } },
  { KEY_5, { '5', '%' } },
  { KEY_6, { '6', '^' } },
  { KEY_7, { '7', '&' } },
  { KEY_8, { '8', '*' } },
  { KEY_9, { '9', '(' } },
  { KEY_0, { '0', ')' } },
  { KEY_MINUS, { '-', '_' } },
  { KEY_EQUAL, { '=', '+' } },
  { KEY_BACKSPACE, { Backspace } },
  { KEY_TAB, { Tab } },
  { KEY_Q, { 'Q' } },
  { KEY_W, { 'W' } },
  { KEY_E, { 'E' } },
  { KEY_R, { 'R' } },
  { KEY_T, { 'T' } },
  { KEY_Y, { 'Y' } },
  { KEY_U, { 'U' } },
  { KEY_I, { 'I' } },
  { KEY_O, { 'O' } },
  { KEY_P, { 'P' } },
  { KEY_LEFTBRACE, { '[', '{' } },
  { KEY_RIGHTBRACE, { ']', '}' } },
  { KEY_ENTER, { Enter } },
  { KEY_LEFTCTRL, { Ctrl } },
  { KEY_A, { 'A' } },
  { KEY_S, { 'S' } },
  { KEY_D, { 'D' } },
  { KEY_F, { 'F' } },
  { KEY_G, { 'G' } },
  { KEY_H, { 'H' } },
  { KEY_J, { 'J' } },
  { KEY_K, { 'K' } },
  { KEY_L, { 'L' } },
  { KEY_SEMICOLON, { ';', ':' } },
  { KEY_APOSTROPHE, { '\'', '"' } },
  { KEY_GRAVE, { '`', '~' } },
  { KEY_LEFTSHIFT, { Shift } },
  { KEY_BACKSLASH, { '\\', '|' } },
  { KEY_Z, { 'Z' } },
  { KEY_X, { 'X' } },
  { KEY_C, { 'C' } },
  { KEY_V, { 'V' } },
  { KEY_B, { 'B' } },
  { KEY_N, { 'N' } },
  { KEY_M, { 'M' } },
  { KEY_COMMA, { ',', '<' } },
  { KEY_DOT, { '.', '>' } },
  { KEY_SLASH, { '/', '?' } },
  { KEY_RIGHTSHIFT, { Shift } },
  { KEY_KPASTERISK, { '*' } },
  { KEY_LEFTALT, { Alt } },
  { KEY_SPACE, { ' ' } },
  // { KEY_CAPSLOCK
  // { KEY_F1 59
  // { KEY_F2 60
  // { KEY_F3 61
  // { KEY_F4 62
  // { KEY_F5 63
  // { KEY_F6 64
  // { KEY_F7 65
  // { KEY_F8 66
  // { KEY_F9 67
  // { KEY_F10 68
  // { KEY_NUMLOCK 69
  // { KEY_SCROLLLOCK 70
  { KEY_KP7, { '7' } },
  { KEY_KP8, { '8' } },
  { KEY_KP9, { '9' } },
  { KEY_KPMINUS, { '-' } },
  { KEY_KP4, { '4' } },
  { KEY_KP5, { '5' } },
  { KEY_KP6, { '6' } },
  { KEY_KPPLUS, { '+' } },
  { KEY_KP1, { '1' } },
  { KEY_KP2, { '2' } },
  { KEY_KP3, { '3' } },
  { KEY_KP0, { '0' } },
  { KEY_KPDOT, { '.' } },

  // { KEY_ZENKAKUHANKAKU 85
  // { KEY_102ND 86
  // { KEY_F11 87
  // { KEY_F12 88
  // { KEY_RO 89
  // { KEY_KATAKANA 90
  // { KEY_HIRAGANA 91
  // { KEY_HENKAN 92
  // { KEY_KATAKANAHIRAGANA 93
  // { KEY_MUHENKAN 94
  // { KEY_KPJPCOMMA 95
  { KEY_KPENTER, { Enter } },
  { KEY_RIGHTCTRL, { Ctrl } },
  { KEY_KPSLASH, { '/' } },
  // { KEY_SYSRQ 99
  { KEY_RIGHTALT, { Alt } },
  { KEY_LINEFEED, { Enter } },
  { KEY_HOME, { Home } },
  { KEY_UP, { Up } },
  { KEY_PAGEUP, { PageUp } },
  { KEY_LEFT, { Left } },
  { KEY_RIGHT, { Right } },
  { KEY_END, { End } },
  { KEY_DOWN, { Down } },
  { KEY_PAGEDOWN, { PageDown } },
  // { KEY_INSERT 110
  { KEY_DELETE, { Del } },
  // { KEY_MACRO 112
  // { KEY_MUTE 113
  // { KEY_VOLUMEDOWN 114
  // { KEY_VOLUMEUP 115
  // { KEY_POWER 116 /* SC System Power Down */
  { KEY_KPEQUAL, { '=' } },
  { KEY_KPPLUSMINUS, { '+', '-' } },
  // { KEY_PAUSE 119
  // { KEY_SCALE 120 /* AL Compiz Scale (Expose) */

};

const KeyMap rm_qwerty_keymap = {
  { KEY_1, { '1', '!' } },
  { KEY_2, { '2', '@' } },
  { KEY_3, { '3', '#' } },
  { KEY_4, { '4', '$' } },
  { KEY_5, { '5', '%' } },
  { KEY_6, { '6', '^' } },
  { KEY_7, { '7', '&' } },
  { KEY_8, { '8', '*' } },
  { KEY_9, { '9', '(' } },
  { KEY_0, { '0', ')', '+' } },
  { KEY_EQUAL, { '-', '_', '=' } },
  { KEY_BACKSPACE, { Backspace } },
  { KEY_TAB, { Tab } },
  { KEY_Q, { 'Q' } },
  { KEY_W, { 'W' } },
  { KEY_E, { 'E' } },
  { KEY_R, { 'R' } },
  { KEY_T, { 'T' } },
  { KEY_Y, { 'Y' } },
  { KEY_U, { 'U' } },
  { KEY_I, { 'I' } },
  { KEY_O, { 'O' } },
  { KEY_P, { 'P' } },
  { KEY_ENTER, { Enter } },
  { KEY_LEFTCTRL, { Ctrl } },
  { KEY_A, { 'A' } },
  { KEY_S, { 'S' } },
  { KEY_D, { 'D' } },
  { KEY_F, { 'F' } },
  { KEY_G, { 'G' } },
  { KEY_H, { 'H' } },
  { KEY_J, { 'J' } },
  { KEY_K, { 'K' } },
  { KEY_L, { 'L' } },
  { KEY_SEMICOLON, { ';', ':' } },
  { KEY_APOSTROPHE, { '\'', '"' } },
  { KEY_GRAVE, { '`', '~' } },
  { KEY_LEFTSHIFT, { Shift } },
  { KEY_BACKSLASH, { '\\', '|' } },
  { KEY_Z, { 'Z' } },
  { KEY_X, { 'X' } },
  { KEY_C, { 'C' } },
  { KEY_V, { 'V' } },
  { KEY_B, { 'B' } },
  { KEY_N, { 'N' } },
  { KEY_M, { 'M' } },
  { KEY_COMMA, { ',', '<' } },
  { KEY_DOT, { '.', '>' } },
  { KEY_SLASH, { '/', '?' } },
  { KEY_RIGHTSHIFT, { Shift } },
  { KEY_LEFTALT, { Alt } },
  { KEY_SPACE, { ' ' } },
  { KEY_CAPSLOCK, { Escape } },
  { KEY_RIGHTALT, { Alt } },
  { KEY_UP, { Up } },
  { KEY_LEFT, { Left } },
  { KEY_RIGHT, { Right } },
  { KEY_END, { Mod1 } },
  { KEY_DOWN, { Down } },
};

const KeyMap timower_keymap = {
  { KEY_1, { '1', '!' } },
  { KEY_2, { '2', '@' } },
  { KEY_3, { '3', '#' } },
  { KEY_4, { '4', '$' } },
  { KEY_5, { '5', '%' } },
  { KEY_6, { '6', '^' } },
  { KEY_7, { '7', '&' } },
  { KEY_8, { '8', '*' } },
  { KEY_9, { '9', '(' } },
  { KEY_0, { '0', ')', '+' } },
  { KEY_EQUAL, { '-', '_', '=' } },
  { KEY_BACKSPACE, { Backspace } },
  { KEY_TAB, { Tab } },
  { KEY_Q, { 'Q' } },
  { KEY_W, { 'W' } },
  { KEY_E, { 'E' } },
  { KEY_R, { 'R' } },
  { KEY_T, { 'T' } },
  { KEY_Y, { 'Y' } },
  { KEY_U, { 'U' } },
  { KEY_I, { 'I' } },
  { KEY_O, { 'O' } },
  { KEY_P, { 'P' } },
  { KEY_ENTER, { Enter } },
  { KEY_LEFTCTRL, { Ctrl } },
  { KEY_A, { 'A' } },
  { KEY_S, { 'S' } },
  { KEY_D, { 'D' } },
  { KEY_F, { 'F' } },
  { KEY_G, { 'G' } },
  { KEY_H, { 'H' } },
  { KEY_J, { 'J' } },
  { KEY_K, { 'K' } },
  { KEY_L, { 'L' } },
  { KEY_SEMICOLON, { ';', ':' } },
  { KEY_APOSTROPHE, { '\'', '"' } },
  { KEY_GRAVE, { '`', '~', '{', '[' } },
  { KEY_LEFTSHIFT, { Shift } },
  { KEY_BACKSLASH, { '\\', '|', '}', ']' } },
  { KEY_Z, { 'Z' } },
  { KEY_X, { 'X' } },
  { KEY_C, { 'C' } },
  { KEY_V, { 'V' } },
  { KEY_B, { 'B' } },
  { KEY_N, { 'N' } },
  { KEY_M, { 'M' } },
  { KEY_COMMA, { ',', '<' } },
  { KEY_DOT, { '.', '>' } },
  { KEY_SLASH, { '/', '?' } },
  { KEY_RIGHTSHIFT, { Shift } },
  { KEY_LEFTALT, { Mod1 } },
  { KEY_SPACE, { ' ' } },
  { KEY_CAPSLOCK, { Escape, 0, 0, 0, Ctrl } },
  { KEY_RIGHTALT, { Mod2 } },
  { KEY_UP, { Up } },
  { KEY_LEFT, { Left } },
  { KEY_RIGHT, { Right } },
  { KEY_END, { Alt } },
  { KEY_DOWN, { Down } },
};
