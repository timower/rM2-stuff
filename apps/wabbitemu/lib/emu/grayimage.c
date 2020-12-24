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

/* Scale the input buffer, multiply by F * INCOUNT, and add to the
   output buffer (which must be an exact multiple of the size of the
   input buffer.) */
static inline void add_scale1d_exact(const byte * restrict in, int incount,
				     unsigned int * restrict out,
				     int outcount, int f)
{
	int i, j;

	for (i = 0; i < incount; i++) {
		for (j = 0; j < outcount / incount; j++) {
			*out += *in * f * incount;
			out++;
		}
		in++;
	}
}

/* Scale a 1-dimensional buffer, multiply by F * INCOUNT, and add to
   the output buffer. */
static inline void add_scale1d_smooth(const byte * restrict in, int incount,
				      unsigned int * restrict out,
				      int outcount, int f)
{
	int in_rem, out_rem;
	unsigned int outv;
	int i;

	in_rem = outcount;
	out_rem = incount;
	outv = 0;
	i = outcount;
	while (i > 0) {
		if (in_rem < out_rem) {
			out_rem -= in_rem;
			outv += in_rem * *in * f;
			in++;
			in_rem = outcount;
		}
		else {
			in_rem -= out_rem;
			outv += out_rem * *in * f;
			*out += outv;
			outv = 0;
			out++;
			out_rem = incount;
			i--;
		}
	}
}

/* Scale a 2-dimensional buffer, multiply by INWIDTH * INHEIGHT, and
   store in the output buffer. */
static void scale2d_smooth(const byte * restrict in,
			   int inwidth, int inheight, int inrowstride,
			   unsigned int * restrict out,
			   int outwidth, int outheight, int outrowstride)
{
	int in_rem, out_rem;
	int i;

	memset(out, 0, outrowstride * outheight * sizeof(int));

	in_rem = outheight;
	out_rem = inheight;
	i = outheight;
	while (i > 0) {
		if (in_rem < out_rem) {
			if (in_rem) {
				if (outwidth % inwidth)
					add_scale1d_smooth(in, inwidth, out,
							   outwidth, in_rem);
				else
					add_scale1d_exact(in, inwidth, out,
							  outwidth, in_rem);
			}
			out_rem -= in_rem;
			in += inrowstride;
			in_rem = outheight;
		}
		else {
			in_rem -= out_rem;
			if (outwidth % inwidth)
				add_scale1d_smooth(in, inwidth, out, outwidth,
						   out_rem);
			else
				add_scale1d_exact(in, inwidth, out, outwidth,
						  out_rem);
			out += outrowstride;
			out_rem = inheight;
			i--;
		}
	}
}

/* Quickly scale a 1-dimensional buffer and store in the output
   buffer. */
static inline void scale1d_fast(const byte * restrict in, int incount,
				byte * restrict out, int outcount)
{
	int i, e;

	e = outcount - incount / 2;
	i = outcount;
	while (i > 0) {
		if (e >= 0) {
			*out = *in;
			out++;
			e -= incount;
			i--;
		}
		else {
			e += outcount;
			in++;
		}
	}
}

/* Quickly scale a 2-dimensional buffer and store in the output
   buffer. */
static void scale2d_fast(const byte * restrict in,
			 int inwidth, int inheight, int inrowstride,
			 byte * restrict out,
			 int outwidth, int outheight, int outrowstride)
{
	int i, e;

	e = outheight - inheight / 2;
	i = outheight;
	while (i > 0) {
		if (e >= 0) {
			scale1d_fast(in, inwidth, out, outwidth);
			out += outrowstride;
			e -= inheight;
			i--;
		}
		else {
			e += outheight;
			in += inrowstride;
		}
	}
}

/* Determine range of linear pixel values corresponding to a given
   contrast level. */
static void get_contrast_settings(unsigned int contrast,
                                  int *cbase, int *cfact)
{
	if (contrast < 32) {
		*cbase = 0;
		*cfact = contrast * 8;
	}
	else {
		*cbase = (contrast - 32) * 8;
		*cfact = 255 - *cbase;
	}
}

#define GETSCALEBUF(ttt, www, hhh) \
	((ttt *) alloc_scalebuf(buf, (www) * (hhh) * sizeof(ttt)))

static void* alloc_scalebuf(TilemLCDBuffer *buf, unsigned int size)
{
	if (TILEM_UNLIKELY(size > buf->tmpbufsize)) {
		buf->tmpbufsize = size;
		tilem_free(buf->tmpbuf);
		buf->tmpbuf = tilem_malloc_atomic(size);
	}

	return buf->tmpbuf;
}

void tilem_draw_lcd_image_indexed(TilemLCDBuffer * restrict buf,
                                  byte * restrict buffer,
                                  int imgwidth, int imgheight,
                                  int rowstride, int scaletype)
{
	int dwidth = buf->width;
	int dheight = buf->height;
	int i, j, v;
	unsigned int * restrict ibuf;
	int cbase, cfact;
	byte cindex[129];

	if (dwidth == 0 || dheight == 0 || buf->contrast == 0) {
		for (i = 0; i < imgheight; i++) {
			for (j = 0; j < imgwidth; j++)
				buffer[j] = 0;
			buffer += rowstride;
		}
		return;
	}

	get_contrast_settings(buf->contrast, &cbase, &cfact);

	for (i = 0; i <= 128; i++)
		cindex[i] = ((i * cfact) >> 7) + cbase;

	if (scaletype == TILEM_SCALE_FAST
	    || (imgwidth % dwidth == 0 && imgheight % dheight == 0)) {
		scale2d_fast(buf->data, dwidth, dheight, buf->rowstride,
		             buffer, imgwidth, imgheight, rowstride);

		for (i = 0; i < imgwidth * imgheight; i++)
			buffer[i] = cindex[buffer[i]];
	}
	else {
		ibuf = GETSCALEBUF(unsigned int, imgwidth, imgheight);

		scale2d_smooth(buf->data, dwidth, dheight, buf->rowstride,
		               ibuf, imgwidth, imgheight, imgwidth);

		for (i = 0; i < imgheight; i++) {
			for (j = 0; j < imgwidth; j++) {
				v = ibuf[j] / (dwidth * dheight);
				buffer[j] = cindex[v];
			}
			ibuf += imgwidth;
			buffer += rowstride;
		}
	}
}

void tilem_draw_lcd_image_rgb(TilemLCDBuffer * restrict buf,
                              byte * restrict buffer,
                              int imgwidth, int imgheight, int rowstride,
                              int pixbytes, const dword * restrict palette,
                              int scaletype)
{
	int dwidth = buf->width;
	int dheight = buf->height;
	int i, j, v;
	int padbytes = rowstride - (imgwidth * pixbytes);
	byte * restrict bbuf;
	unsigned int * restrict ibuf;
	int cbase, cfact;
	dword cpalette[129];

	if (dwidth == 0 || dheight == 0 || buf->contrast == 0) {
		for (i = 0; i < imgheight; i++) {
			for (j = 0; j < imgwidth; j++) {
				buffer[0] = palette[0] >> 16;
				buffer[1] = palette[0] >> 8;
				buffer[2] = palette[0];
				buffer += pixbytes;
			}
			buffer += padbytes;
		}
		return;
	}

	get_contrast_settings(buf->contrast, &cbase, &cfact);

	for (i = 0; i <= 128; i++) {
		v = ((i * cfact) >> 7) + cbase;
		cpalette[i] = palette[v];
	}

	if (scaletype == TILEM_SCALE_FAST
	    || (imgwidth % dwidth == 0 && imgheight % dheight == 0)) {
		bbuf = GETSCALEBUF(byte, imgwidth, imgheight);

		scale2d_fast(buf->data, dwidth, dheight, buf->rowstride,
		             bbuf, imgwidth, imgheight, imgwidth);

		for (i = 0; i < imgheight; i++) {
			for (j = 0; j < imgwidth; j++) {
				v = bbuf[j];
				buffer[0] = cpalette[v] >> 16;
				buffer[1] = cpalette[v] >> 8;
				buffer[2] = cpalette[v];
				buffer += pixbytes;
			}
			bbuf += imgwidth;
			buffer += padbytes;
		}
	}
	else {
		ibuf = GETSCALEBUF(unsigned int, imgwidth, imgheight);

		scale2d_smooth(buf->data, dwidth, dheight, buf->rowstride,
			       ibuf, imgwidth, imgheight, imgwidth);

		for (i = 0; i < imgheight; i++) {
			for (j = 0; j < imgwidth; j++) {
				v = (ibuf[j] / (dwidth * dheight));
				buffer[0] = cpalette[v] >> 16;
				buffer[1] = cpalette[v] >> 8;
				buffer[2] = cpalette[v];
				buffer += pixbytes;
			}
			ibuf += imgwidth;
			buffer += padbytes;
		}
	}
}
