/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2001 Solignac Julien
 * Copyright (C) 2004-2009 Benjamin Moody
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <tilem.h>

#include "x3.h"

static const char* hwregnames[NUM_HW_REGS] = HW_REG_NAMES;

static const char* hwtimernames[NUM_HW_TIMERS] = HW_TIMER_NAMES;

static const char* keynames[64] = {
	"Down", "Left", "Right", "Up", 0, 0, 0, 0,
	"Enter", "Add", "Sub", "Mul", "Div", "Power", "Clear", 0,
	"Chs", "3", "6", "9", "RParen", "Tan", "Vars", 0,
	"DecPnt", "2", "5", "8", "LParen", "Cos", "Prgm", "Stat",
	"0", "1", "4", "7", "Comma", "Sin", "Matrix", "Graphvar",
	"On", "Store", "Ln", "Log", "Square", "Recip", "Math", "Alpha",
	"Graph", "Trace", "Zoom", "Window", "YEqu", "2nd", "Mode", "Del",
	0, 0, 0, 0, 0, 0, 0, 0};

const TilemHardware hardware_ti83 = {
	'3', "ti83", "TI-83 / TI-82 STATS",
	(TILEM_CALC_HAS_LINK | TILEM_CALC_HAS_T6A04),
	96, 64, 16 * 0x4000, 0x8000, 15 * 64, 0x40,
	0, NULL, 0,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	keynames,
	x3_reset, NULL,
	x3_z80_in, x3_z80_out,
	x3_z80_wrmem, x3_z80_rdmem, x3_z80_rdmem, NULL,
	x3_z80_ptimer, tilem_lcd_t6a04_get_data,
	x3_mem_ltop, x3_mem_ptol };

const TilemHardware hardware_ti76 = {
	'f', "ti76", "TI-76.fr",
	(TILEM_CALC_HAS_LINK | TILEM_CALC_HAS_T6A04),
	96, 64, 16 * 0x4000, 0x8000, 15 * 64, 0x40,
	0, NULL, 0,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	keynames,
	x3_reset, NULL,
	x3_z80_in, x3_z80_out,
	x3_z80_wrmem, x3_z80_rdmem, x3_z80_rdmem, NULL,
	x3_z80_ptimer, tilem_lcd_t6a04_get_data,
	x3_mem_ltop, x3_mem_ptol };
