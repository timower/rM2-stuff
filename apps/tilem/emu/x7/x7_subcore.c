/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2001 Solignac Julien
 * Copyright (C) 2004-2012 Benjamin Moody
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

#include "x7.h"

static const TilemFlashSector flashsectors[] = {
	{0x000000, 0x10000, 0},
	{0x010000, 0x10000, 0},
	{0x020000, 0x10000, 0},
	{0x030000, 0x10000, 0},
	{0x040000, 0x10000, 0},
	{0x050000, 0x10000, 0},
	{0x060000, 0x10000, 0},
	{0x070000, 0x08000, 0},
	{0x078000, 0x02000, 0},
	{0x07A000, 0x02000, 0},
	{0x07C000, 0x04000, 1}};

#define NUM_FLASH_SECTORS (sizeof(flashsectors) / sizeof(TilemFlashSector))

static const char* hwregnames[NUM_HW_REGS] = HW_REG_NAMES;

static const char* hwtimernames[NUM_HW_TIMERS] = HW_TIMER_NAMES;

static const char* keynames[64] = {
	"Down", "Left", "Right", "Up", 0, 0, 0, 0,
	"Enter", "Add", "Sub", "Mul", "Div", "Const", "Clear", 0,
	"Chs", "3", "6", "9", "RParen", "MixSimp", "AppsMenu", 0,
	"DecPnt", "2", "5", "8", "LParen", "FracDec", "Prgm", "StatEd",
	"0", "1", "4", "7", "Percent", "FracSlash", "Expon", "Draw",
	"On", "Store", "Comma", "VarX", "Simp", "Unit", "Square", "Math",
	"Graph", "Trace", "Zoom", "Window", "YEqu", "2nd", "Mode", "Del",
	0, 0, 0, 0, 0, 0, 0, 0};

TilemHardware hardware_ti73 = {
	'7', "ti73", "TI-73",
	(TILEM_CALC_HAS_LINK | TILEM_CALC_HAS_T6A04 | TILEM_CALC_HAS_FLASH),
	96, 64, 32 * 0x4000, 2 * 0x4000, 15 * 64, 0x40,
	NUM_FLASH_SECTORS, flashsectors, 0,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	keynames,
	x7_reset, x7_stateloaded,
	x7_z80_in, x7_z80_out,
	x7_z80_wrmem, x7_z80_rdmem, x7_z80_rdmem_m1, NULL,
	x7_z80_ptimer, tilem_lcd_t6a04_get_data,
	x7_mem_ltop, x7_mem_ptol };
