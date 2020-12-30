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

#ifndef _TILEM_GRAYLCD_H
#define _TILEM_GRAYLCD_H

typedef struct _TilemGrayLCDPixel {
	word ndark;		/* Sum of lengths of dark intervals */
	word nlight;		/* Sum of lengths of light intervals */
	word ndarkseg;		/* Number of dark intervals */
	word nlightseg;		/* Number of light intervals */
} TilemGrayLCDPixel;

struct _TilemGrayLCD {
	TilemCalc *calc;	/* Calculator */
	int timer_id;		/* Screen update timer */
	dword lcdupdatetime;	/* CPU time of last known LCD update */

	dword t;		/* Time counter */
	int windowsize;		/* Number of frames in the sampling
				   window */
	int framenum;		/* Current frame number */
	int sampleint;		/* Microseconds per sample */ 

	int bwidth;		/* Width of LCD, bytes */
	int height;		/* Height of LCD, pixels */
	byte *oldbits;		/* Original pixel values (current buffer) */
	byte *newbits;		/* Original pixel values (alternate buffer) */

	dword *tchange;		/* Time when pixels changed */
	dword *tframestart;	/* Time at start of frame */
	dword *framestamp;	/* LCD update time at start of frame */

	TilemGrayLCDPixel *curpixels; /* Current pixel counters */
	TilemGrayLCDPixel *framebasepixels; /* Pixel counters as of
					       the start of each
					       frame */
};

#endif
