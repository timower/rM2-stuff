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

#include "x5.h"

static const char* hwregnames[NUM_HW_REGS] = HW_REG_NAMES;

static const char* hwtimernames[NUM_HW_TIMERS] = HW_TIMER_NAMES;

static const char* keynames[64] = {
	"Down", "Left", "Right", "Up", 0, 0, 0, 0,
	"Enter", "Add", "Sub", "Mul", "Div", "Power", "Clear", 0,
	"Chs", "3", "6", "9", "RParen", "Tan", "Custom", 0,
	"DecPnt", "2", "5", "8", "LParen", "Cos", "Prgm", "Del",
	"0", "1", "4", "7", "EE", "Sin", "Stat", "XVar",
	"On", "Store", "Comma", "Square", "Ln", "Log", "Graph", "Alpha",
	"F5", "F4", "F3", "F2", "F1", "2nd", "Exit", "More",
	0, 0, 0, 0, 0, 0, 0, 0};

const TilemHardware hardware_ti85 = {
	'5', "ti85", "TI-85",
	TILEM_CALC_HAS_LINK,
	128, 64, 8 * 0x4000, 0x8000, 0, 0x40,
	0, NULL, 0,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	keynames,
	x5_reset, NULL,
	x5_z80_in, x5_z80_out,
	x5_z80_wrmem, x5_z80_rdmem, x5_z80_rdmem, NULL,
	x5_z80_ptimer, tilem_lcd_t6a43_get_data,
	x5_mem_ltop, x5_mem_ptol };

