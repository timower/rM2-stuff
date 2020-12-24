/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2010 Benjamin Moody
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
#include <math.h>
#include "tilem.h"

dword* tilem_color_palette_new(int rlight, int glight, int blight,
			       int rdark, int gdark, int bdark,
			       double gamma)
{
	dword* pal = tilem_new_atomic(dword, 256);
	double r0, g0, b0, dr, dg, db;
	double igamma = 1.0 / gamma;
	double s = (1.0 / 255.0);
	int r, g, b, i;

	r0 = pow(rlight * s, gamma);
	g0 = pow(glight * s, gamma);
	b0 = pow(blight * s, gamma);
	dr = (pow(rdark * s, gamma) - r0) * s;
	dg = (pow(gdark * s, gamma) - g0) * s;
	db = (pow(bdark * s, gamma) - b0) * s;

	pal[0] = (rlight << 16) | (glight << 8) | blight;

	for (i = 1; i < 255; i++) {
		r = pow(r0 + i * dr, igamma) * 255.0 + 0.5;
		if (r < 0) r = 0;
		if (r > 255) r = 255;

		g = pow(g0 + i * dg, igamma) * 255.0 + 0.5;
		if (g < 0) g = 0;
		if (g > 255) g = 255;

		b = pow(b0 + i * db, igamma) * 255.0 + 0.5;
		if (b < 0) b = 0;
		if (b > 255) b = 255;

		pal[i] = (r << 16) | (g << 8) | b;
	}

	pal[255] = (rdark << 16) | (gdark << 8) | bdark;

	return pal;
}

byte* tilem_color_palette_new_packed(int rlight, int glight, int blight,
                                     int rdark, int gdark, int bdark,
                                     double gamma)
{
	dword* palette;
	byte* packed;
	int i;

	palette = tilem_color_palette_new(rlight, glight, blight,
	                                  rdark, gdark, bdark, gamma);

	packed = tilem_new_atomic(byte, 256 * 3);
	for (i = 0; i < 256; i++) {
		packed[i * 3] = palette[i] >> 16;
		packed[i * 3 + 1] = palette[i] >> 8;
		packed[i * 3 + 2] = palette[i];
	}

	tilem_free(palette);

	return packed;
}
