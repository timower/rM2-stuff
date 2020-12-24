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

#include "x2.h"

byte x2_z80_in(TilemCalc* calc, dword port)
{
	static const byte battlevel[4] = { 33, 39, 36, 43 };
	byte v, b;

	switch(port&0xff) {
	case 0x00:
		v = tilem_linkport_get_lines(calc);

		if (calc->hwregs[HW_VERSION] == 1) {
			/* HW version 1 uses separate PCR/PDR, as the
			   TI-85 does. */
			b = (calc->hwregs[PORT0] >> 4) | 0xf0;
			return ((calc->hwregs[PORT0] & b) | (v & ~b));
		}
		else {
			/* HW version 2 uses a TI-83-like interface.
			   (Do the same if version is unknown, because
			   most code written for HW version 1 will
			   work fine with these values.) */
			return (0xc0 | (v * 5));
		}

	case 0x01:
		return(tilem_keypad_read_keys(calc));

	case 0x02:
		return(calc->hwregs[PORT2]);

	case 0x03:
		v = (calc->keypad.onkeydown ? 0x00: 0x08);

		if (calc->z80.interrupts & TILEM_INTERRUPT_ON_KEY)
			v |= 0x01;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER1)
			v |= 0x02;

		return(v);

	case 0x10:
		return(tilem_lcd_t6a04_status(calc));

	case 0x11:
		return(tilem_lcd_t6a04_read(calc));

	case 0x14:
		/* FIXME: determine value of this port on old
		   hardware, and the values of bits 1-7.  (As on
		   TI-83, probably mirrors port 0 in both cases.) */
		b = battlevel[calc->hwregs[PORT4] >> 6];
		return(calc->battery >= b ? 1 : 0);
	}

	tilem_warning(calc, "Input from port %x", port);
	return(0x00);
}


static void setup_mapping(TilemCalc* calc)
{
	unsigned int pageA, pageB;

	/* FIXME: this is all rather hypothetical and untested, but it
	   makes sense based on how the TI-83 works */

	if (calc->hwregs[PORT2] & 0x40) {
		pageA = (0x08 | (calc->hwregs[PORT2] & 1));
	}
	else {
		pageA = (calc->hwregs[PORT2] & 7);
	}

	if (calc->hwregs[PORT2] & 0x80) {
		pageB = (0x08 | ((calc->hwregs[PORT2] >> 3) & 1));
	}
	else {
		pageB = ((calc->hwregs[PORT2] >> 3) & 7);
	}

	if (calc->hwregs[PORT4] & 1) {
		calc->mempagemap[1] = (pageA & ~1);
		calc->mempagemap[2] = (pageA | 1);
		calc->mempagemap[3] = pageB;
	}
	else {
		calc->mempagemap[1] = pageA;
		calc->mempagemap[2] = pageB;
		calc->mempagemap[3] = 0x08;
	}
}

void x2_z80_out(TilemCalc* calc, dword port, byte value)
{
	switch(port&0xff) {
	case 0x00:
		calc->hwregs[PORT0] = value;

		if (calc->hwregs[HW_VERSION] == 1) {
			/* HW version 1 */
			value = (((value >> 6) & (value >> 2))
				 | ((value >> 4) & ~value));
		}
		else if (calc->hwregs[HW_VERSION] == 0) {
			/* HW version unknown: auto-detect */
			if ((value & 0xc3) == 0xc0) {
				value = ((value >> 2) | (value >> 4));
			}
		}

		tilem_linkport_set_lines(calc, value);
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

		if (!(value & 0x02)) {
			calc->z80.interrupts &= ~TILEM_INTERRUPT_TIMER1;
		}

		calc->poweronhalt = ((value & 8) >> 3);
		calc->hwregs[PORT3] = value;
		break;

	case 0x04:
		calc->hwregs[PORT4] = value;

		/* Detect hardware version */
		if (calc->z80.r.pc.d < 0x4000) {
			if (value & 0x10)
				calc->hwregs[HW_VERSION] = 1;
			else
				calc->hwregs[HW_VERSION] = 2;
		}

		/* FIXME: implement changing interrupt frequencies --
		   somebody needs to measure them.  Also check if bit
		   4 works as on 83. */

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

void x2_z80_ptimer(TilemCalc* calc, int id)
{
	switch (id) {
	case TIMER_INT:
		if (calc->hwregs[PORT3] & 0x02)
			calc->z80.interrupts |= TILEM_INTERRUPT_TIMER1;
		break;
	}
}
