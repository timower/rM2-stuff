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

#include "x4.h"

static const TilemFlashSector flashsectors[] = {
	{0x000000, 0x10000, 0}, {0x010000, 0x10000, 0},
	{0x020000, 0x10000, 0}, {0x030000, 0x10000, 0},
	{0x040000, 0x10000, 0}, {0x050000, 0x10000, 0},
	{0x060000, 0x10000, 0}, {0x070000, 0x10000, 0},
	{0x080000, 0x10000, 0}, {0x090000, 0x10000, 0},
	{0x0A0000, 0x10000, 0}, {0x0B0000, 0x10000, 1},
	{0x0C0000, 0x10000, 0}, {0x0D0000, 0x10000, 0},
	{0x0E0000, 0x10000, 0}, {0x0F0000, 0x08000, 0},
	{0x0F8000, 0x02000, 0}, {0x0FA000, 0x02000, 0},
	{0x0FC000, 0x04000, 1}};

#define NUM_FLASH_SECTORS (sizeof(flashsectors) / sizeof(TilemFlashSector))

static const char* hwregnames[NUM_HW_REGS] = HW_REG_NAMES;

static const char* hwtimernames[NUM_HW_TIMERS] = HW_TIMER_NAMES;

extern const char* xp_keynames[];

TilemHardware hardware_ti84p = {
	'4', "ti84p", "TI-84 Plus",
	(TILEM_CALC_HAS_LINK | TILEM_CALC_HAS_LINK_ASSIST
	 | TILEM_CALC_HAS_T6A04 | TILEM_CALC_HAS_FLASH
	 | TILEM_CALC_HAS_MD5_ASSIST),
	96, 64, 64 * 0x4000, 8 * 0x4000, 16 * 64, 0x80,
	NUM_FLASH_SECTORS, flashsectors, 3,
	NUM_HW_REGS, hwregnames,
	NUM_HW_TIMERS, hwtimernames,
	xp_keynames,
	x4_reset, x4_stateloaded,
	x4_z80_in, x4_z80_out,
	x4_z80_wrmem, x4_z80_rdmem, x4_z80_rdmem_m1, NULL,
	x4_z80_ptimer, tilem_lcd_t6a04_get_data,
	x4_mem_ltop, x4_mem_ptol };
