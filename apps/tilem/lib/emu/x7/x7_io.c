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

#include "x7.h"

byte x7_z80_in(TilemCalc* calc, dword port)
{
	/* FIXME: measure actual levels */
	static const byte battlevel[4] = { 33, 39, 36, 43 };
	byte v;

	/* FIXME: confirm that mirror ports are the same as on 83+
	   - and figure out what, if anything, port 5 does */

	switch(port&0x1f) {
	case 0x00:
	case 0x08:
		v = tilem_linkport_get_lines(calc);
		v |= (calc->linkport.lines << 4);
		return(v);

	case 0x01:
	case 0x09:
		return(tilem_keypad_read_keys(calc));

	case 0x02:
	case 0x0A:
		v = battlevel[calc->hwregs[PORT4] >> 6];
		return(calc->battery >= v ? 0x05 : 0x04);

	case 0x03:
	case 0x0B:
		return(calc->hwregs[PORT3]);

	case 0x04:
	case 0x0C:
		v = (calc->keypad.onkeydown ? 0x00 : 0x08);

		if (calc->z80.interrupts & TILEM_INTERRUPT_ON_KEY)
			v |= 0x01;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER1)
			v |= 0x02;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER2)
			v |= 0x04;
		if (calc->z80.interrupts & TILEM_INTERRUPT_LINK_ACTIVE)
			v |= 0x10;

		return(v);

	case 0x06:
	case 0x0E:
		return(calc->hwregs[PORT6]);

	case 0x07:
	case 0x0F:
		return(calc->hwregs[PORT7]);

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
	}
	return(0x00);
}


static void setup_mapping(TilemCalc* calc)
{
	unsigned int pageA, pageB;

	if (calc->hwregs[PORT6] & 0x40)
		pageA = (0x20 | (calc->hwregs[PORT6] & 1));
	else
		pageA = (calc->hwregs[PORT6] & 0x1f);

	if (calc->hwregs[PORT7] & 0x40)
		pageB = (0x20 | (calc->hwregs[PORT7] & 1));
	else
		pageB = (calc->hwregs[PORT7] & 0x1f);

	if (calc->hwregs[PORT4] & 1) {
		/* FIXME: confirm this behavior (83+-like rather than
		   83-like) */
		calc->mempagemap[1] = (pageA & ~1);
		calc->mempagemap[2] = pageA;
		calc->mempagemap[3] = pageB;
	}
	else {
		calc->mempagemap[1] = pageA;
		calc->mempagemap[2] = pageB;
		calc->mempagemap[3] = 0x20;
	}
}

void x7_z80_out(TilemCalc* calc, dword port, byte value)
{
	static const int tmrvalues[4] = { 1786, 4032, 5882, 8474 };
	int t;
	unsigned int mode;

	switch(port&0x1f) {
	case 0x00:
	case 0x08:
		tilem_linkport_set_lines(calc, value);
		break;

	case 0x01:
	case 0x09:
		tilem_keypad_set_group(calc, value);
		break;

	case 0x03:
	case 0x0B:
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

		mode = calc->linkport.mode;
		if (value & 0x10)
			mode |= TILEM_LINK_MODE_INT_ON_ACTIVE;
		else
			mode &= ~TILEM_LINK_MODE_INT_ON_ACTIVE;

		tilem_linkport_set_mode(calc, mode);

		calc->poweronhalt = ((value & 8) >> 3);
		calc->hwregs[PORT3] = value;
		break;

	case 0x04:
	case 0x0C:
		calc->hwregs[PORT4] = value;

		t = tmrvalues[(value & 6) >> 1];
		tilem_z80_set_timer_period(calc, TIMER_INT1, t);
		tilem_z80_set_timer_period(calc, TIMER_INT2A, t);
		tilem_z80_set_timer_period(calc, TIMER_INT2B, t);

		setup_mapping(calc);
		break;

	case 0x06:
	case 0x0E:
		calc->hwregs[PORT6] = value & 0x7f;
		setup_mapping(calc);
		break;

	case 0x07:
	case 0x0F:
		calc->hwregs[PORT7] = value & 0x7f;
		setup_mapping(calc);
		break;

	case 0x10:
	case 0x12:
	case 0x18:
	case 0x1A:
		tilem_lcd_t6a04_control(calc, value);
		break;

	case 0x11:
	case 0x13:
	case 0x19:
	case 0x1B:
		tilem_lcd_t6a04_write(calc, value);
		break;

	case 0x14:
	case 0x15:
		if (calc->hwregs[PROTECTSTATE] == 7) {
			if (value & 1)
				tilem_message(calc, "Flash unlocked");
			else
				tilem_message(calc, "Flash locked");
			calc->flash.unlock = value&1;
		}
		break;

	case 0x16:
	case 0x17:
		if (calc->flash.unlock && calc->hwregs[PROTECTSTATE] == 7) {
			tilem_message(calc, "No-exec mask set to %x", value);
			calc->hwregs[NOEXEC] = ((value & 0x0f) << 2);
		}
		break;
	}

	return;
}

void x7_z80_ptimer(TilemCalc* calc, int id)
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
