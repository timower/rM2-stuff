/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2011 Benjamin Moody
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
#include <string.h>
#include "tilem.h"

TilemLCDBuffer* tilem_lcd_buffer_new()
{
	return tilem_new0(TilemLCDBuffer, 1);
}

void tilem_lcd_buffer_free(TilemLCDBuffer *buf)
{
	tilem_free(buf->data);
	tilem_free(buf->tmpbuf);
	tilem_free(buf);
}

void tilem_lcd_get_frame(TilemCalc * restrict calc,
                         TilemLCDBuffer * restrict buf)
{
	byte * restrict bp;
	byte * restrict op;
	int dwidth = calc->hw.lcdwidth;
	int dheight = calc->hw.lcdheight;
	unsigned int size;
	int bwidth = ((calc->hw.lcdwidth + 7) / 8);
	int i, j;

	if (TILEM_UNLIKELY(buf->height != dheight
	                   || buf->rowstride != bwidth * 8)) {
		/* reallocate data buffer */
		tilem_free(buf->data);
		buf->data = tilem_new_atomic(byte, dwidth * bwidth * 8);
		buf->rowstride = bwidth * 8;
		buf->height = dheight;
	}

	size = bwidth * dheight * sizeof(byte);
	if (TILEM_UNLIKELY(buf->tmpbufsize < size)) {
		/* reallocate temp buffer */
		tilem_free(buf->tmpbuf);
		buf->tmpbuf = tilem_malloc_atomic(size);
		buf->tmpbufsize = size;
	}

	buf->width = dwidth;

	buf->stamp = calc->z80.lastlcdwrite;

	if (!calc->lcd.active || (calc->z80.halted && !calc->poweronhalt)) {
		/* screen is turned off */
		buf->contrast = 0;
		return;
	}

	buf->contrast = calc->lcd.contrast;

	bp = buf->tmpbuf;
	op = buf->data;
	(*calc->hw.get_lcd)(calc, bp);

	for (i = 0; i < bwidth * dheight; i++) {
		for (j = 0; j < 8; j++) {
			*op = (*bp << j) & 0x80;
			op++;
		}
		bp++;
	}
}

/* Do the same thing as tilem_lcd_get_frame, but output is only 0 and 1 */
void tilem_lcd_get_frame1(TilemCalc * restrict calc,
                         TilemLCDBuffer * restrict buf)
{
	byte * restrict bp;
	byte * restrict op;
	int dwidth = calc->hw.lcdwidth;
	int dheight = calc->hw.lcdheight;
	unsigned int size;
	int bwidth = ((calc->hw.lcdwidth + 7) / 8);
	int i, j;

	if (TILEM_UNLIKELY(buf->height != dheight
	                   || buf->rowstride != bwidth * 8)) {
		/* reallocate data buffer */
		tilem_free(buf->data);
		buf->data = tilem_new_atomic(byte, dwidth * bwidth * 8);
		buf->rowstride = bwidth * 8;
		buf->height = dheight;
	}

	size = bwidth * dheight * sizeof(byte);
	if (TILEM_UNLIKELY(buf->tmpbufsize < size)) {
		/* reallocate temp buffer */
		tilem_free(buf->tmpbuf);
		buf->tmpbuf = tilem_malloc_atomic(size);
		buf->tmpbufsize = size;
	}

	buf->width = dwidth;

	buf->stamp = calc->z80.lastlcdwrite;

	if (!calc->lcd.active || (calc->z80.halted && !calc->poweronhalt)) {
		/* screen is turned off */
		buf->contrast = 0;
		return;
	}

	buf->contrast = calc->lcd.contrast;

	bp = buf->tmpbuf;
	op = buf->data;
	(*calc->hw.get_lcd)(calc, bp);

	for (i = 0; i < bwidth * dheight; i++) {
		for (j = 0; j < 8; j++) {
			*op = (*bp << j) & 0x80;
			if(*op != 0)
				*op = 1;
			op++;
		}
		bp++;
	}
}
