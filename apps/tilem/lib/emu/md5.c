/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009 Benjamin Moody
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

void tilem_md5_assist_reset(TilemCalc* calc)
{
	int i;

	for (i = 0; i < 6; i++)
		calc->md5assist.regs[i] = 0;
	calc->md5assist.shift = 0;
	calc->md5assist.mode = 0;
}

dword tilem_md5_assist_get_value(TilemCalc* calc)
{
	/* Return the result of a complete MD5 operation:
	   b + ((a + f(b,c,d) + X + T) <<< s) */
	dword a, b, c, d, x, t, result;
	byte mode, s;

	mode = calc->md5assist.mode;
	a = calc->md5assist.regs[TILEM_MD5_REG_A];
	b = calc->md5assist.regs[TILEM_MD5_REG_B];
	c = calc->md5assist.regs[TILEM_MD5_REG_C];
	d = calc->md5assist.regs[TILEM_MD5_REG_D];
	x = calc->md5assist.regs[TILEM_MD5_REG_X];
	t = calc->md5assist.regs[TILEM_MD5_REG_T];
	s = calc->md5assist.shift;

	switch (mode) {
	case TILEM_MD5_FUNC_FF:
		/* F(X,Y,Z) = XY v not(X) Z */
		result = (b & c) | ((~b) & d);
		break;

	case TILEM_MD5_FUNC_GG:
		/* G(X,Y,Z) = XZ v Y not(Z) */
		result = (b & d) | (c & (~d));
		break;

	case TILEM_MD5_FUNC_HH:
		/* H(X,Y,Z) = X xor Y xor Z */
		result = b ^ c ^ d;
		break;

	case TILEM_MD5_FUNC_II:
		/* I(X,Y,Z) = Y xor (X v not(Z)) */
		result = c ^ (b | (~d));
		break;

	default:
		tilem_internal(calc, "Invalid MD5 mode %d", mode);
		return 0;
	}

	result += a + x + t;
	result &= 0xffffffff;
	result = (result << s) | (result >> (32 - s));
	result += b;

	return result;
}
