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

#include "x5.h"

byte x5_z80_in(TilemCalc* calc, dword port)
{
	byte v, b;
	
	switch(port&0xff) {
	case 0x01:
		return(tilem_keypad_read_keys(calc));

	case 0x03:
		v = (calc->keypad.onkeydown ? 0x00 : 0x08);

		if (calc->z80.interrupts & TILEM_INTERRUPT_ON_KEY)
			v |= 0x01;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER1)
			v |= 0x02;

		return(v);

	case 0x04:
		return(0x01);

	case 0x05:
		return(calc->hwregs[PORT5]);

	case 0x06:
		return(calc->hwregs[PORT6]);

	case 0x07:
		v = tilem_linkport_get_lines(calc);
		b = (calc->hwregs[PORT7] >> 4) | 0xf0;
		return ((calc->hwregs[PORT7] & b) | (v & ~b));
	}

	tilem_warning(calc, "Input from port %x", port);
	return(0x00);
}


static void setup_mapping(TilemCalc* calc)
{
	unsigned int pageA, pageB;

	if (calc->hwregs[PORT5] & 0x40) {
		pageA = (0x08 | (calc->hwregs[PORT5] & 1));
	}
	else {
		pageA = (calc->hwregs[PORT5] & 7);
	}

	if (calc->hwregs[PORT6] & 0x40) {
		pageB = (0x08 | (calc->hwregs[PORT6] & 1));
	}
	else {
		pageB = (calc->hwregs[PORT6] & 7);
	}

	calc->mempagemap[1] = pageA;
	calc->mempagemap[2] = pageB;
	calc->mempagemap[3] = 0x08;	
}

void x5_z80_out(TilemCalc* calc, dword port, byte value)
{
	switch(port&0xff) {
	case 0x00:
		calc->lcd.addr = ((value & 0x3f) << 8);
		calc->z80.lastlcdwrite = calc->z80.clock;
		break;

	case 0x01:
		tilem_keypad_set_group(calc, value);
		break;

	case 0x02:
		/* Contrast */
		calc->lcd.contrast = 16 + (value & 0x1f);
		break;

	case 0x03:
		/* Interrupts / LCD power */
		if (value & 0x01) {
			calc->keypad.onkeyint = 1;
		}
		else {
			calc->z80.interrupts &= ~TILEM_INTERRUPT_ON_KEY;
			calc->keypad.onkeyint = 0;
		}

		if (!(value & 0x02)) {
			calc->z80.interrupts &= ~TILEM_INTERRUPT_TIMER1;
		}

		calc->hwregs[PORT3] = value;
		calc->lcd.active = calc->poweronhalt = ((value & 8) >> 3);
		break;

	case 0x04:
		/* LCD control */
		calc->hwregs[PORT4] = value;

		switch (value & 0x18) {
		case 0x00:
			calc->lcd.rowstride = 10;
			break;

		case 0x08:
			calc->lcd.rowstride = 12;
			break;

		case 0x10:
			calc->lcd.rowstride = 16;
			break;

		case 0x18:
			calc->lcd.rowstride = 20;
			break;
		}
		calc->z80.lastlcdwrite = calc->z80.clock;
		break;

	case 0x05:
		calc->hwregs[PORT5] = value;
		setup_mapping(calc);
		break;

	case 0x06:
		calc->hwregs[PORT6] = value;
		setup_mapping(calc);
		break;

	case 0x07:
		/* Link */
		calc->hwregs[PORT7] = value;

		value = (((value >> 6) & (value >> 2))
			 | ((value >> 4) & ~value));

		tilem_linkport_set_lines(calc, value);
		break;
	}

	return;
}

void x5_z80_ptimer(TilemCalc* calc, int id)
{
	switch (id) {
	case TIMER_INT:
		if (calc->hwregs[PORT3] & 0x02)
			calc->z80.interrupts |= TILEM_INTERRUPT_TIMER1;
		break;
	}
}
