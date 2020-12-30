/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009 Benjamin Moody
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _TILEM_SCANCODES_H
#define _TILEM_SCANCODES_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	TILEM_KEY_DOWN     = 0x01,
	TILEM_KEY_LEFT     = 0x02,
	TILEM_KEY_RIGHT    = 0x03,
	TILEM_KEY_UP       = 0x04,
	TILEM_KEY_ENTER    = 0x09,
	TILEM_KEY_ADD      = 0x0A,
	TILEM_KEY_SUB      = 0x0B,
	TILEM_KEY_MUL      = 0x0C,
	TILEM_KEY_DIV      = 0x0D,
	TILEM_KEY_POWER    = 0x0E,
	TILEM_KEY_CLEAR    = 0x0F,
	TILEM_KEY_CHS      = 0x11,
	TILEM_KEY_3        = 0x12,
	TILEM_KEY_6        = 0x13,
	TILEM_KEY_9        = 0x14,
	TILEM_KEY_RPAREN   = 0x15,
	TILEM_KEY_TAN      = 0x16,
	TILEM_KEY_VARS     = 0x17,
	TILEM_KEY_DECPNT   = 0x19,
	TILEM_KEY_2        = 0x1A,
	TILEM_KEY_5        = 0x1B,
	TILEM_KEY_8        = 0x1C,
	TILEM_KEY_LPAREN   = 0x1D,
	TILEM_KEY_COS      = 0x1E,
	TILEM_KEY_PRGM     = 0x1F,
	TILEM_KEY_STAT     = 0x20,
	TILEM_KEY_0        = 0x21,
	TILEM_KEY_1        = 0x22,
	TILEM_KEY_4        = 0x23,
	TILEM_KEY_7        = 0x24,
	TILEM_KEY_COMMA    = 0x25,
	TILEM_KEY_SIN      = 0x26,
	TILEM_KEY_MATRIX   = 0x27,
	TILEM_KEY_GRAPHVAR = 0x28,
	TILEM_KEY_ON       = 0x29,
	TILEM_KEY_STORE    = 0x2A,
	TILEM_KEY_LN       = 0x2B,
	TILEM_KEY_LOG      = 0x2C,
	TILEM_KEY_SQUARE   = 0x2D,
	TILEM_KEY_RECIP    = 0x2E,
	TILEM_KEY_MATH     = 0x2F,
	TILEM_KEY_ALPHA    = 0x30,
	TILEM_KEY_GRAPH    = 0x31,
	TILEM_KEY_TRACE    = 0x32,
	TILEM_KEY_ZOOM     = 0x33,
	TILEM_KEY_WINDOW   = 0x34,
	TILEM_KEY_YEQU     = 0x35,
	TILEM_KEY_2ND      = 0x36,
	TILEM_KEY_MODE     = 0x37,
	TILEM_KEY_DEL      = 0x38
};

#ifdef __cplusplus
}
#endif

#endif
