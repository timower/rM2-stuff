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
#include <tilem.h>

#include "x5.h"

void x5_reset(TilemCalc* calc)
{
	calc->hwregs[PORT3] = 0x0B;
	calc->hwregs[PORT4] = 0x16;
	calc->hwregs[PORT5] = 0x00;
	calc->hwregs[PORT6] = 0x40;

	calc->mempagemap[0] = 0x00;
	calc->mempagemap[1] = 0x00;
	calc->mempagemap[2] = 0x08;
	calc->mempagemap[3] = 0x08;

	calc->lcd.addr = 0x3c00;

	tilem_z80_set_speed(calc, 6000);

	/* FIXME: measure actual frequency */
	tilem_z80_set_timer(calc, TIMER_INT, 1000, 5000, 1);
}
