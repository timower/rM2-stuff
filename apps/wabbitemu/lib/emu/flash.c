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
#include "tilem.h"

#define FLASH_READ 0
#define FLASH_AA   1
#define FLASH_55   2
#define FLASH_PROG 3
#define FLASH_ERASE 4
#define FLASH_ERAA 5
#define FLASH_ER55 6
#define FLASH_ERROR 7
#define FLASH_FASTMODE 8
#define FLASH_FASTPROG 9
#define FLASH_FASTEXIT 10

#define FLASH_BUSY_PROGRAM 1
#define FLASH_BUSY_ERASE_WAIT 2
#define FLASH_BUSY_ERASE 3

/* Still to do:
   - autoselect
   - erase suspend
   - fast program
   - CFI
 */

#define WARN(xxx) \
	tilem_warning(calc, "Flash error (" xxx ")")
#define WARN2(xxx, yyy, zzz) \
	tilem_warning(calc, "Flash error (" xxx ")", (yyy), (zzz))

void tilem_flash_reset(TilemCalc* calc)
{
	calc->flash.unlock = 0;
	calc->flash.state = FLASH_READ;
	calc->flash.busy = 0;
}

void tilem_flash_delay_timer(TilemCalc* calc, void* data TILEM_ATTR_UNUSED)
{
	if (calc->flash.busy == FLASH_BUSY_ERASE_WAIT) {
		calc->flash.busy = FLASH_BUSY_ERASE;
		tilem_z80_set_timer(calc, TILEM_TIMER_FLASH_DELAY,
				    200000, 0, 1);
	}
	else {
		calc->flash.busy = 0;
	}
}

#ifdef DISABLE_FLASH_DELAY
# define set_busy(fl, bm, t)
#else
static inline void set_busy(TilemCalc* calc, int busymode, int time)
{
	if (!(calc->flash.emuflags & TILEM_FLASH_REQUIRE_DELAY))
		return;

	calc->flash.busy = busymode;
	tilem_z80_set_timer(calc, TILEM_TIMER_FLASH_DELAY,
			    time, 0, 1);
}
#endif

static inline void program_byte(TilemCalc* calc, dword a, byte v)
{
	calc->mem[a] &= v;
	calc->flash.progaddr = a;
	calc->flash.progbyte = v;

	if (calc->mem[a] != v) {
		WARN2("bad program %02x over %02x", v, calc->mem[a]);
		calc->flash.state = FLASH_ERROR;
	}
	else {
		calc->flash.state = FLASH_READ;
	}

	set_busy(calc, FLASH_BUSY_PROGRAM, 7);
}

static inline void erase_sector(TilemCalc* calc, dword a, dword l)
{
	dword i;

	calc->flash.progaddr = a;
	for (i = 0; i < l; i++)
		calc->mem[a + i]=0xFF;
	calc->flash.state = FLASH_READ;

	set_busy(calc, FLASH_BUSY_ERASE_WAIT, 50);
}

static const TilemFlashSector* get_sector(TilemCalc* calc, dword pa)
{
	int i;
	const TilemFlashSector* sec;

	for (i = 0; i < calc->hw.nflashsectors; i++) {
		sec = &calc->hw.flashsectors[i];
		if (pa >= sec->start && pa < sec->start + sec->size)
			return sec;
	}

	return NULL;
}

static int sector_writable(TilemCalc* calc, const TilemFlashSector* sec)
{
	return !(sec->protectgroup & ~calc->flash.overridegroup);
}

byte tilem_flash_read_byte(TilemCalc* calc, dword pa)
{
	byte value;

	if (calc->flash.busy == FLASH_BUSY_PROGRAM) {
		if (pa != calc->flash.progaddr)
			WARN("reading from Flash while programming");
		value = (~calc->flash.progbyte & 0x80);
		value |= calc->flash.toggles;
		calc->flash.toggles ^= 0x40;
		return (value);
	}
	else if (calc->flash.busy == FLASH_BUSY_ERASE) {
		if ((pa >> 16) != (calc->flash.progaddr >> 16))
			WARN("reading from Flash while erasing");
		value = calc->flash.toggles | 0x08;
		calc->flash.toggles ^= 0x44;
		return (value);
	}
	else if (calc->flash.busy == FLASH_BUSY_ERASE_WAIT) {
		if ((pa >> 16) != (calc->flash.progaddr >> 16))
			WARN("reading from Flash while erasing");
		value = calc->flash.toggles;
		calc->flash.toggles ^= 0x44;
		return (value);
	}

        if (calc->flash.state == FLASH_ERROR) {
		value = ((~calc->flash.progbyte & 0x80) | 0x20);
		value |= calc->flash.toggles;
		calc->flash.toggles ^= 0x40;
		return (value);
	}
	else if (calc->flash.state == FLASH_FASTMODE) {
		return (calc->mem[pa]);
	}
	else if (calc->flash.state == FLASH_READ) {
		return (calc->mem[pa]);
	}
	else {
		WARN("reading during program/erase sequence");
		calc->flash.state = FLASH_READ;
		return (calc->mem[pa]);
	}
}

void tilem_flash_erase_address(TilemCalc* calc, dword pa)
{
	const TilemFlashSector* sec = get_sector(calc, pa);

	if (sector_writable(calc, sec)) {
		tilem_message(calc, "Erasing Flash sector at %06x", pa);
		erase_sector(calc, sec->start, sec->size);
	}
	else {
		WARN("erasing protected sector");
	}
}

void tilem_flash_write_byte(TilemCalc* calc, dword pa, byte v)
{
	int oldstate;
	int i;
	const TilemFlashSector* sec;

	if (!calc->flash.unlock)
		return;

#ifndef DISABLE_FLASH_DELAY
	if (calc->flash.busy == FLASH_BUSY_PROGRAM
	    || calc->flash.busy == FLASH_BUSY_ERASE)
		return;
#endif

	oldstate = calc->flash.state;
	calc->flash.state = FLASH_READ;

	switch (oldstate) {
	case FLASH_READ:
		if (((pa&0xFFF) == 0xAAA) && (v == 0xAA))
			calc->flash.state = FLASH_AA;
		return;

	case FLASH_AA:
		if (((pa&0xFFF) == 0x555) && (v == 0x55))
			calc->flash.state = FLASH_55;
		else if (v != 0xF0) {
			WARN2("undefined command %02x->%06x after AA", v, pa);
		}
		return;

	case FLASH_55:
		if ((pa&0xFFF) == 0xAAA) {
			switch (v) {
			case 0x10:
			case 0x30:
				WARN("attempt to erase without pre-erase");
				return;
			case 0x20:
				//WARN("entering fast mode");
				calc->flash.state = FLASH_FASTMODE;
				return;
			case 0x80:
				calc->flash.state = FLASH_ERASE;
				return;
			case 0x90:
				WARN("autoselect is not implemented");
				return;
			case 0xA0:
				calc->flash.state = FLASH_PROG;
				return;
			}
		}
		if (v != 0xF0)
			WARN2("undefined command %02x->%06x after AA,55", v, pa);
		return;

	case FLASH_PROG:
		sec = get_sector(calc, pa);
		if (!sector_writable(calc, sec))
			WARN("programming protected sector");
		else
			program_byte(calc, pa, v);
		return;

	case FLASH_FASTMODE:
		//WARN2("fast mode cmd %02x->%06x", v, pa);
		if ( v == 0x90 )
			calc->flash.state = FLASH_FASTEXIT;
		else if ( v == 0xA0 )
			calc->flash.state = FLASH_FASTPROG;
		else
			// TODO : figure out whether mixing is allowed on real HW
			WARN2("mixing fast programming with regular programming : %02x->%06x", v, pa);
		return;

	case FLASH_FASTPROG:
		//WARN2("fast prog %02x->%06x", v, pa);
		sec = get_sector(calc, pa);
		if (!sector_writable(calc, sec))
			WARN("programming protected sector");
		else
			program_byte(calc, pa, v);
		calc->flash.state = FLASH_FASTMODE;
		return;

	case FLASH_FASTEXIT:
		//WARN("leaving fast mode");
		if ( v != 0xF0 )
		{
			WARN2("undefined command %02x->%06x after fast mode pre-exit 90", v, pa);
			// TODO : figure out whether fast mode remains in such a case
			calc->flash.state = FLASH_FASTMODE;
		}
		return;
		
	case FLASH_ERASE:
		if (((pa&0xFFF) == 0xAAA) && (v == 0xAA))
			calc->flash.state = FLASH_ERAA;
		else if (v != 0xF0)
			WARN2("undefined command %02x->%06x after pre-erase", v, pa);
		return;

	case FLASH_ERAA:
		if (((pa&0xFFF) == 0x555) && (v == 0x55))
			calc->flash.state = FLASH_ER55;
		else if (v != 0xF0)
			WARN2("undefined command %02x->%06x after pre-erase AA", v, pa);
		return;

	case FLASH_ER55:
		if (((pa&0xFFF) == 0xAAA) && v==0x10) {
			tilem_message(calc, "Erasing entire Flash chip");

			for (i = 0; i < calc->hw.nflashsectors; i++) {
				sec = &calc->hw.flashsectors[i];
				if (sector_writable(calc, sec))
					erase_sector(calc, sec->start,
						     sec->size);
			}
		}
		else if (v == 0x30) {
			tilem_flash_erase_address(calc, pa);
		}
		else if (v != 0xF0)
			WARN2("undefined command %02x->%06x after pre-erase AA,55", v, pa);
		return;
	}
}
