/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2001 Solignac Julien
 * Copyright (C) 2004-2011 Benjamin Moody
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

#include "x7.h"

void x7_reset(TilemCalc* calc)
{
	calc->hwregs[PORT3] = 0x0B;
	calc->hwregs[PORT4] = 0x77;
	calc->hwregs[PORT6] = 0x1F;
	calc->hwregs[PORT7] = 0x1F;

	calc->mempagemap[0] = 0x00;
	calc->mempagemap[1] = 0x1E;
	calc->mempagemap[2] = 0x1F;
	calc->mempagemap[3] = 0x1F;

	calc->z80.r.pc.d = 0x8000;

	calc->hwregs[NOEXEC] = 0;
	calc->hwregs[PROTECTSTATE] = 0;

	tilem_z80_set_speed(calc, 6000);

	tilem_z80_set_timer(calc, TIMER_INT1, 1600, 8474, 1);
	tilem_z80_set_timer(calc, TIMER_INT2A, 1300, 8474, 1);
	tilem_z80_set_timer(calc, TIMER_INT2B, 1000, 8474, 1);
}

void x7_stateloaded(TilemCalc* calc, int savtype TILEM_ATTR_UNUSED)
{
	tilem_calc_fix_certificate(calc, calc->mem + (0x1E * 0x4000L),
	                           0x08, 0x13, 0x1f18);
}
