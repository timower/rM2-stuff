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

#include "xp.h"

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

const char* xp_keynames[64] = {
	"Down", "Left", "Right", "Up", 0, 0, 0, 0,
	"Enter", "Add", "Sub", "Mul", "Div", "Power", "Clear", 0,
	"Chs", "3", "6", "9", "RParen", "Tan", "Vars", 0,
	"DecPnt", "2", "5", "8", "LParen", "Cos", "Prgm", "Stat",
	"0", "1", "4", "7", "Comma", "Sin", "Apps", "Graphvar",
	"On", "Store", "Ln", "Log", "Square", "Recip", "Math", "Alpha",
	"Graph", "Trace", "Zoom", "Window", "YEqu", "2nd", "Mode", "Del",
	0, 0, 0, 0, 0, 0, 0, 0};

TilemHardware hardware_ti83p = {
	'p', "ti83p", "TI-83 Plus",
	(TILEM_CALC_HAS_LINK | TILEM_CALC_HAS_LINK_ASSIST
	 | TILEM_CALC_HAS_T6A04 | TILEM_CALC_HAS_FLASH),
	96, 64, 32 * 0x4000, 2 * 0x4000, 15 * 64, 0x40,
	NUM_FLASH_SECTORS, flashsectors, 0,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	xp_keynames,
	xp_reset, xp_stateloaded,
	xp_z80_in, xp_z80_out,
	xp_z80_wrmem, xp_z80_rdmem, xp_z80_rdmem_m1, NULL,
	xp_z80_ptimer, tilem_lcd_t6a04_get_data,
	xp_mem_ltop, xp_mem_ptol };
