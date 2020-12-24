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

#include "x1.h"

static const char* hwregnames[NUM_HW_REGS] = HW_REG_NAMES;

static const char* hwtimernames[NUM_HW_TIMERS] = HW_TIMER_NAMES;

static const char* keynames[64] = {
	"Down", "Left", "Right", "Up", 0, 0, 0, 0,
	"Enter", "Add", "Sub", "Mul", "Div", "Power", "Clear", 0,
	"Chs", "3", "6", "9", "RParen", "Tan", "Vars", 0,
	"DecPnt", "2", "5", "8", "LParen", "Cos", "Prgm", "Mode",
	"0", "1", "4", "7", "EE", "Sin", "Matrix", "Graphvar",
	"On", "Store", "Ln", "Log", "Square", "Recip", "Math", "Alpha",
	"Graph", "Trace", "Zoom", "Range", "YEqu", "2nd", "Ins", "Del",
	0, 0, 0, 0, 0, 0, 0, 0};

const TilemHardware hardware_ti81 = {
	'1', "ti81", "TI-81", TILEM_CALC_HAS_T6A04,
	96, 64,	0x8000, 0x2000, 15 * 64, 0x40,
	0, NULL, 0,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	keynames,
	x1_reset, NULL,
	x1_z80_in, x1_z80_out,
	x1_z80_wrmem, x1_z80_rdmem, x1_z80_rdmem, NULL,
	x1_z80_ptimer, x1_get_lcd,
	x1_mem_ltop, x1_mem_ptol };
