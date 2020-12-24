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
#include "tilem.h"

#ifdef DISABLE_LCD_DRIVER_DELAY
# define BUSY 0
# define SET_BUSY 0
#else
# define BUSY check_delay_timer(calc)
# define SET_BUSY set_delay_timer(calc)

static inline int check_delay_timer(TilemCalc* calc)
{
	int t;

	if (!calc->lcd.busy)
		return 0;
	
	t = tilem_z80_get_timer_clocks(calc, TILEM_TIMER_LCD_DELAY);
	return (t > 0);
}

static inline void set_delay_timer(TilemCalc* calc)
{
	int delay;

	if (!(calc->lcd.emuflags & TILEM_LCD_REQUIRE_DELAY))
		return;

	if (calc->lcd.emuflags & TILEM_LCD_REQUIRE_LONG_DELAY)
		delay = 70;
	else
		delay = 50;

	calc->lcd.busy = 1;

	tilem_z80_set_timer(calc, TILEM_TIMER_LCD_DELAY, delay, 0, 0);
}
#endif

void tilem_lcd_reset(TilemCalc* calc)
{
	calc->lcd.active = 0;
	calc->lcd.contrast = 32;
	calc->lcd.addr = 0;
	calc->lcd.mode = 1;
	calc->lcd.nextbyte = 0;
	calc->lcd.x = calc->lcd.y = 0;
	calc->lcd.inc = 7;
	calc->lcd.rowshift = 0;
	calc->lcd.busy = 0;

	if (calc->hw.lcdmemsize)
		calc->lcd.rowstride = (calc->hw.lcdmemsize
				       / calc->hw.lcdheight);
	else
		calc->lcd.rowstride = (calc->hw.lcdwidth / 8);
}

void tilem_lcd_delay_timer(TilemCalc* calc, void* data TILEM_ATTR_UNUSED)
{
	calc->lcd.busy = 0;
}

byte tilem_lcd_t6a04_status(TilemCalc* calc)
{
	return (calc->lcd.busy << 7
		| calc->lcd.mode << 6
		| calc->lcd.active << 5
		| (calc->lcd.inc & 3));
}

void tilem_lcd_t6a04_control(TilemCalc* calc, byte val)
{
	if (BUSY) return;

	if (val <= 1) {
		calc->lcd.mode = val;

	} else if (val == 2) {
		calc->lcd.active = 0;

	} else if (val == 3) {
		calc->lcd.active = 1;

	} else if (val <= 7) {
		calc->lcd.inc = val;

	} else if ((val >= 0x20) && (val <= 0x3F)){
		calc->lcd.x = val - 0x20;

	} else if ((val >= 0x80) && (val <= 0xBF)) {
		calc->lcd.y = val - 0x80;

	} else if ((val >= 0x40) && (val <= 0x7F)) {
		calc->lcd.rowshift = val - 0x40;

	} else if (val >= 0xc0) {
		calc->lcd.contrast = val - 0xc0;

	}

	calc->z80.lastlcdwrite = calc->z80.clock;
	SET_BUSY;
}


byte tilem_lcd_t6a04_read(TilemCalc* calc)
{
	byte retv = calc->lcd.nextbyte;
	byte* lcdbuf = calc->lcdmem;
	int stride = calc->lcd.rowstride;
	int xlimit;

	if (BUSY) return(0);

	if (calc->lcd.mode)
		xlimit = stride;
	else
		xlimit = (stride * 8 + 5) / 6;

	if (calc->lcd.x >= xlimit)
		calc->lcd.x = 0;
	else if (calc->lcd.x < 0)
		calc->lcd.x = xlimit - 1;

	if (calc->lcd.y >= 0x40)
		calc->lcd.y = 0;
	else if (calc->lcd.y < 0)
		calc->lcd.y = 0x3F;

	if (calc->lcd.mode) {
		calc->lcd.nextbyte = *(lcdbuf + calc->lcd.x + stride * calc->lcd.y);

	} else {
		int col = 0x06 * calc->lcd.x;
		int ofs = calc->lcd.y * stride + (col >> 3);
		int shift = 0x0A - (col & 0x07);

		calc->lcd.nextbyte = ((*(lcdbuf + ofs) << 8) | *(lcdbuf + ofs + 1)) >> shift;
	}

	switch (calc->lcd.inc) {
		case 4: calc->lcd.y--; break;
		case 5: calc->lcd.y++; break;
		case 6: calc->lcd.x--; break;
		case 7: calc->lcd.x++; break;
	}

	SET_BUSY;
	return(retv);
}


void tilem_lcd_t6a04_write(TilemCalc* calc, byte sprite)
{
	byte* lcdbuf = calc->lcdmem;
	int stride = calc->lcd.rowstride;
	int xlimit;

	if (BUSY) return;

	if (calc->lcd.mode)
		xlimit = stride;
	else
		xlimit = (stride * 8 + 5) / 6;

	if (calc->lcd.x >= xlimit)
		calc->lcd.x = 0;
	else if (calc->lcd.x < 0)
		calc->lcd.x = xlimit - 1;

	if (calc->lcd.y >= 0x40)
		calc->lcd.y = 0;
	else if (calc->lcd.y < 0)
		calc->lcd.y = 0x3F;

	if (calc->lcd.mode) {
		*(lcdbuf + calc->lcd.x + stride * calc->lcd.y) = sprite;

	} else {
		int col = 0x06 * calc->lcd.x;
		int ofs = calc->lcd.y * stride + (col >> 3);
		int shift = col & 0x07;
		int mask;

		sprite <<= 2;
		mask = ~(0xFC >> shift);
		*(lcdbuf + ofs) = (*(lcdbuf + ofs) & mask) | (sprite >> shift);
		if (shift > 2 && (col >> 3) < (stride - 1)) {
			ofs++;
			shift = 8 - shift;
			mask = ~(0xFC << shift);
			*(lcdbuf + ofs) = (*(lcdbuf + ofs) & mask) | (sprite << shift);
		}
	}

	switch (calc->lcd.inc) {
		case 4: calc->lcd.y--; break;
		case 5: calc->lcd.y++; break;
		case 6: calc->lcd.x--; break;
		case 7: calc->lcd.x++; break;
	}

	calc->z80.lastlcdwrite = calc->z80.clock;
	SET_BUSY;
	return;
}


void tilem_lcd_t6a04_get_data(TilemCalc* calc, byte* data)
{
	int width = calc->hw.lcdwidth / 8;
	byte* lcdbuf = calc->lcdmem;
	int stride = calc->lcd.rowstride;
	int i, j, k;

	for (i = 0; i < calc->hw.lcdheight; i++) {
		j = (i + calc->lcd.rowshift) % 64;
		for (k = 0; k < width; k++)
			data[k] = lcdbuf[j * stride + k];
		data += width;
	}
}

void tilem_lcd_t6a43_get_data(TilemCalc* calc, byte* data)
{
	int width = calc->hw.lcdwidth / 8;
	byte* lcdbuf = calc->ram + calc->lcd.addr;
	int stride = calc->lcd.rowstride;
	int i, j;

	for (i = 0; i < calc->hw.lcdheight; i++) {
		for (j = 0; j < 10; j++)
			data[j] = lcdbuf[j];
		for (; j < 10 + width - stride; j++)
			data[j] = 0;
		for (; j < width; j++)
			data[j] = lcdbuf[j + stride - width];

		data += width;
		lcdbuf += stride;
	}
}
