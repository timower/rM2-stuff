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

#include "x3.h"

byte x3_z80_in(TilemCalc* calc, dword port)
{
	static const byte battlevel[4] = { 33, 39, 36, 43 };
	byte v, b;
	
	switch(port&0x1f) {
	case 0x00:
	case 0x04:
	case 0x08:
	case 0x0C:
		v = tilem_linkport_get_lines(calc);
		return((calc->hwregs[ROM_BANK] << 1) | (v * 5));

	case 0x01:
	case 0x05:
	case 0x09:
	case 0x0D:
		return(tilem_keypad_read_keys(calc));

	case 0x02:
	case 0x06:
	case 0x0A:
	case 0x0E:
	case 0x16:
	case 0x1E:
		return(calc->hwregs[PORT2]);

	case 0x03:
	case 0x07:
	case 0x0B:
	case 0x0F:
	case 0x17:
	case 0x1F:
		v = (calc->keypad.onkeydown ? 0x00 : 0x08);

		if (calc->z80.interrupts & TILEM_INTERRUPT_ON_KEY)
			v |= 0x01;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER1)
			v |= 0x02;

		return(v);

	case 0x10:
	case 0x12:
	case 0x18:
	case 0x1A:
		return(tilem_lcd_t6a04_status(calc));

	case 0x11:
	case 0x13:
	case 0x19:
	case 0x1B:
		return(tilem_lcd_t6a04_read(calc));

	case 0x14:
	case 0x1C:
		b = battlevel[calc->hwregs[PORT4] >> 6];
		v = tilem_linkport_get_lines(calc);
		return((calc->battery >= b ? 1 : 0)
		       | (calc->hwregs[ROM_BANK] << 1) | (v << 2));

	case 0x15:
	case 0x1D:
		return(tilem_keypad_read_keys(calc) & ~1);
	}

	tilem_warning(calc, "Input from port %x", port);
	return(0x00);
}


static void setup_mapping(TilemCalc* calc)
{
	unsigned int pageA, pageB;

	if (calc->hwregs[PORT2] & 0x40) {
		pageA = (0x10 | (calc->hwregs[PORT2] & 1));
	}
	else {
		pageA = (calc->hwregs[ROM_BANK] | (calc->hwregs[PORT2] & 7));
	}

	if (calc->hwregs[PORT2] & 0x80) {
		pageB = (0x10 | ((calc->hwregs[PORT2] >> 3) & 1));
	}
	else {
		pageB = (calc->hwregs[ROM_BANK] | ((calc->hwregs[PORT2] >> 3) & 7));
	}

	if (calc->hwregs[PORT4] & 1) {
		calc->mempagemap[1] = (pageA & ~1);
		calc->mempagemap[2] = (pageA | 1);
		calc->mempagemap[3] = pageB;
	}
	else {
		calc->mempagemap[1] = pageA;
		calc->mempagemap[2] = pageB;
		calc->mempagemap[3] = 0x10;
	}
}

void x3_z80_out(TilemCalc* calc, dword port, byte value)
{
	static const int tmrvalues[8] = { 1667, 1852, 3889, 4321,
					  6111, 6790, 8333, 9259 };
	int t;

	switch(port&0x1f) {
	case 0x00:
		calc->hwregs[ROM_BANK] = ((value & 0x10) >> 1);
		tilem_linkport_set_lines(calc, value);
		setup_mapping(calc);
		break;

	case 0x01:
		tilem_keypad_set_group(calc, value);
		break;

	case 0x02:
		calc->hwregs[PORT2] = value;
		setup_mapping(calc);
		break;

	case 0x03:
		if (value & 0x01) {
			calc->keypad.onkeyint = 1;
		}
		else {
			calc->z80.interrupts &= ~TILEM_INTERRUPT_ON_KEY;
			calc->keypad.onkeyint = 0;
		}

		if (!(value & 0x02))
			calc->z80.interrupts &= ~TILEM_INTERRUPT_TIMER1;

		if (!(value & 0x04))
			calc->z80.interrupts &= ~TILEM_INTERRUPT_TIMER2;

		calc->poweronhalt = ((value & 8) >> 3);
		calc->hwregs[PORT3] = value;
		break;

	case 0x04:
		calc->hwregs[PORT4] = value;

		t = tmrvalues[((value >> 4) & 1) | (value & 6)];
		tilem_z80_set_timer_period(calc, TIMER_INT1, t);
		tilem_z80_set_timer_period(calc, TIMER_INT2A, t);
		tilem_z80_set_timer_period(calc, TIMER_INT2B, t);

		setup_mapping(calc);
		break;

	case 0x10:
		tilem_lcd_t6a04_control(calc, value);
		break;

	case 0x11:
		tilem_lcd_t6a04_write(calc, value);
		break;
	}

	return;
}

void x3_z80_ptimer(TilemCalc* calc, int id)
{
	switch (id) {
	case TIMER_INT1:
		if (calc->hwregs[PORT3] & 0x02)
			calc->z80.interrupts |= TILEM_INTERRUPT_TIMER1;
		break;

	case TIMER_INT2A:
	case TIMER_INT2B:
		if (calc->hwregs[PORT3] & 0x04)
			calc->z80.interrupts |= TILEM_INTERRUPT_TIMER2;
		break;
	}
}
