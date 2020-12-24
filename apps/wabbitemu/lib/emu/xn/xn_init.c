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

#include "xn.h"

void xn_reset(TilemCalc* calc)
{
	calc->hwregs[PORT3] = 0x0B;
	calc->hwregs[PORT4] = 0x07;
	calc->hwregs[PORT6] = 0x7F;
	calc->hwregs[PORT7] = 0x7F;

	calc->mempagemap[0] = 0x00;
	calc->mempagemap[1] = 0x7E;
	calc->mempagemap[2] = 0x7F;
	calc->mempagemap[3] = 0x7F;

	calc->z80.r.pc.d = 0x8000;

	calc->hwregs[PORT8] = 0x80;

	calc->hwregs[PORT20] = 0;
	calc->hwregs[PORT21] = 0;
	calc->hwregs[PORT22] = 0x08;
	calc->hwregs[PORT23] = 0x69;
	calc->hwregs[PORT25] = 0x10;
	calc->hwregs[PORT26] = 0x20;
	calc->hwregs[PORT27] = 0;
	calc->hwregs[PORT28] = 0;
	calc->hwregs[PORT29] = 0x14;
	calc->hwregs[PORT2A] = 0x27;
	calc->hwregs[PORT2B] = 0x2F;
	calc->hwregs[PORT2C] = 0x3B;
	calc->hwregs[PORT2D] = 0x01;
	calc->hwregs[PORT2E] = 0x44;
	calc->hwregs[PORT2F] = 0x4A;

	calc->hwregs[FLASH_READ_DELAY] = 0;
	calc->hwregs[FLASH_WRITE_DELAY] = 0;
	calc->hwregs[FLASH_EXEC_DELAY] = 0;
	calc->hwregs[RAM_READ_DELAY] = 0;
	calc->hwregs[RAM_WRITE_DELAY] = 0;
	calc->hwregs[RAM_EXEC_DELAY] = 0;
	calc->hwregs[LCD_PORT_DELAY] = 5;
	calc->hwregs[NO_EXEC_RAM] = 0x5555;

	calc->hwregs[PROTECTSTATE] = 0;

	tilem_z80_set_speed(calc, 6000);
	calc->z80.emuflags |= TILEM_Z80_RESET_UNDOCUMENTED;

	calc->lcd.emuflags &= ~TILEM_LCD_REQUIRE_DELAY;

	tilem_z80_set_timer(calc, TIMER_INT1, 1600, 9277, 1);
	tilem_z80_set_timer(calc, TIMER_INT2A, 1300, 9277, 1);
	tilem_z80_set_timer(calc, TIMER_INT2B, 1000, 9277, 1);
}

void xn_stateloaded(TilemCalc* calc, int savtype TILEM_ATTR_UNUSED)
{
	tilem_calc_fix_certificate(calc, calc->mem + (0x7E * 0x4000L),
	                           0x69, 0x0c, 0x1e50);
}
