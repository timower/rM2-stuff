/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2010-2011 Benjamin Moody
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
#include "graylcd.h"

/* Read screen contents and update pixels that have changed */
static void tmr_screen_update(TilemCalc *calc, void *data)
{
	TilemGrayLCD *glcd = data;
	byte *np, *op, nb, ob, d;
	int i, j, n;
	dword delta;

	glcd->t++;

	if (calc->z80.lastlcdwrite == glcd->lcdupdatetime)
		return;
	glcd->lcdupdatetime = calc->z80.lastlcdwrite;

	(*calc->hw.get_lcd)(calc, glcd->newbits);

	np = glcd->newbits;
	op = glcd->oldbits;
	glcd->oldbits = np;
	glcd->newbits = op;
	n = 0;

	for (i = 0; i < glcd->bwidth * glcd->height; i++) {
		nb = *np;
		ob = *op;
		d = nb ^ ob;
		for (j = 0; j < 8; j++) {
			if (d & (0x80 >> j)) {
				delta = glcd->t - glcd->tchange[n];
				glcd->tchange[n] = glcd->t;

				if (ob & (0x80 >> j)) {
					glcd->curpixels[n].ndark += delta;
					glcd->curpixels[n].ndarkseg++;
				}
				else {
					glcd->curpixels[n].nlight += delta;
					glcd->curpixels[n].nlightseg++;
				}
			}
			n++;
		}

		np++;
		op++;
	}
}

TilemGrayLCD* tilem_gray_lcd_new(TilemCalc *calc, int windowsize, int sampleint)
{
	TilemGrayLCD *glcd = tilem_new(TilemGrayLCD, 1);
	int nbytes, npixels, i;

	glcd->bwidth = (calc->hw.lcdwidth + 7) / 8;
	glcd->height = calc->hw.lcdheight;
	nbytes = glcd->bwidth * glcd->height;
	npixels = nbytes * 8;

	glcd->oldbits = tilem_new_atomic(byte, nbytes);
	glcd->newbits = tilem_new_atomic(byte, nbytes);
	glcd->tchange = tilem_new_atomic(dword, npixels);
	glcd->tframestart = tilem_new_atomic(dword, windowsize);
	glcd->framestamp = tilem_new_atomic(dword, windowsize);
	glcd->curpixels = tilem_new_atomic(TilemGrayLCDPixel, npixels);
	glcd->framebasepixels = tilem_new_atomic(TilemGrayLCDPixel,
						 npixels * windowsize);

	memset(glcd->oldbits, 0, nbytes);
	memset(glcd->tchange, 0, npixels * sizeof(dword));
	memset(glcd->tframestart, 0, windowsize * sizeof(dword));
	memset(glcd->curpixels, 0, npixels * sizeof(TilemGrayLCDPixel));
	memset(glcd->framebasepixels, 0, (npixels * windowsize
					  * sizeof(TilemGrayLCDPixel)));

	glcd->calc = calc;
	glcd->timer_id = tilem_z80_add_timer(calc, sampleint / 2, sampleint, 1,
					     &tmr_screen_update, glcd);

	/* assign arbitrary but unique timestamps to the initial n
	   frames */
	for (i = 0; i < windowsize; i++)
		glcd->framestamp[i] = calc->z80.lastlcdwrite - i;

	glcd->lcdupdatetime = calc->z80.lastlcdwrite - 1;
	glcd->t = 0;
	glcd->windowsize = windowsize;
	glcd->sampleint = sampleint;
	glcd->framenum = 0;

	return glcd;
}

void tilem_gray_lcd_free(TilemGrayLCD *glcd)
{
	tilem_z80_remove_timer(glcd->calc, glcd->timer_id);

	tilem_free(glcd->oldbits);
	tilem_free(glcd->newbits);
	tilem_free(glcd->tchange);
	tilem_free(glcd->tframestart);
	tilem_free(glcd->framestamp);
	tilem_free(glcd->curpixels);
	tilem_free(glcd->framebasepixels);
	tilem_free(glcd);
}

/* Update levelbuf with values based on the accumulated grayscale
   data */
void tilem_gray_lcd_get_frame(TilemGrayLCD * restrict glcd,
                              TilemLCDBuffer * restrict buf)
{
	int i, j, n;
	unsigned int current, delta, fd, fl;
	word ndark, nlight, ndarkseg, nlightseg;
	dword tbase, tlimit;
	dword lastwrite;
	byte * restrict bp;
	byte * restrict op;
	TilemGrayLCDPixel * restrict pix;
	TilemGrayLCDPixel * restrict basepix;
	dword * restrict tchange;

	if (TILEM_UNLIKELY(buf->height != glcd->height
	                   || buf->rowstride != glcd->bwidth * 8)) {
		/* reallocate data buffer */
		tilem_free(buf->data);
		buf->data = tilem_new_atomic(byte,
		                             glcd->height * glcd->bwidth * 8);
		buf->rowstride = glcd->bwidth * 8;
		buf->height = glcd->height;
	}

	buf->width = glcd->calc->hw.lcdwidth;

	if (!glcd->calc->lcd.active
	    || (glcd->calc->z80.halted && !glcd->calc->poweronhalt)) {
		/* screen is turned off */
		buf->stamp = glcd->calc->z80.lastlcdwrite;
		buf->contrast = 0;
		return;
	}

	buf->contrast = glcd->calc->lcd.contrast;

	/* If LCD remains unchanged throughout the window, set
	   timestamp to the time when the LCD was last changed, so
	   that consecutive frames have the same timestamp.  If LCD
	   has changed during the window, values of gray pixels will
	   vary from one frame to another, so use a unique timestamp
	   for each frame */
	lastwrite = glcd->calc->z80.lastlcdwrite;
	if (glcd->framestamp[glcd->framenum] == lastwrite)
		buf->stamp = lastwrite;
	else
		buf->stamp = glcd->calc->z80.clock + 0x80000000;
	glcd->framestamp[glcd->framenum] = lastwrite;

	/* set tbase to the sample number where the window began; this
	   is used to limit the weight of unchanging pixels */
	tbase = glcd->tframestart[glcd->framenum];
	glcd->tframestart[glcd->framenum] = glcd->t;
	tlimit = glcd->t - tbase; /* number of samples per window */

	bp = glcd->newbits;
	op = buf->data;
	pix = glcd->curpixels;
	basepix = glcd->framebasepixels + (glcd->framenum * glcd->height
					   * glcd->bwidth * 8);
	tchange = glcd->tchange;

	(*glcd->calc->hw.get_lcd)(glcd->calc, bp);

	n = 0;

	for (i = 0; i < glcd->bwidth * glcd->height; i++) {
		for (j = 0; j < 8; j++) {
			/* check if pixel is currently set */
			current = *bp & (0x80 >> j);

			/* compute number of dark and light samples
			   within the window */
			ndark = pix[n].ndark - basepix[n].ndark;
			nlight = pix[n].nlight - basepix[n].nlight;

			/* compute number of dark and light segments
			   within the window */
			ndarkseg = pix[n].ndarkseg - basepix[n].ndarkseg;
			nlightseg = pix[n].nlightseg - basepix[n].nlightseg;

			/* average light segment in this window is
			   (nlight / nlightseg); average dark segment
			   is (ndark / ndarkseg) */

			/* ensure tchange is later than or equal to tbase */
			if (tchange[n] - tbase > tlimit) {
				tchange[n] = tbase;
			}

			/* if current segment is longer than average,
			   count it as well */
			delta = glcd->t - tchange[n];

			if (current) {
				if (delta * ndarkseg >= ndark) {
					ndark += delta;
					ndarkseg++;
				}
			}
			else {
				if (delta * nlightseg >= nlight) {
					nlight += delta;
					nlightseg++;
				}
			}

			fd = ndark * nlightseg;
			fl = nlight * ndarkseg;

			if (fd + fl == 0)
				*op = (ndark ? 128 : 0);
			else
				*op = ((fd * 128) / (fd + fl));

			n++;
			op++;
		}
		bp++;
	}

	memcpy(basepix, pix, (glcd->height * glcd->bwidth * 8
			      * sizeof(TilemGrayLCDPixel)));

	glcd->framenum = (glcd->framenum + 1) % glcd->windowsize;
}
