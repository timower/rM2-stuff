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

#include "x1.h"

byte x1_z80_in(TilemCalc* calc, dword port)
{
	byte v;

	if (calc->hwregs[HW_VERSION] == 1)
		port &= 7;
	
	switch(port&0x1f) {
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

	case 0x05:
		return(calc->hwregs[PORT5]);

	case 0x06:
		return(calc->hwregs[PORT6]);

	case 0x10:
		return(tilem_lcd_t6a04_status(calc));

	case 0x11:
		return(tilem_lcd_t6a04_read(calc));
	}

	tilem_warning(calc, "Input from port %x", port);
	return(0x00);
}


static void setup_mapping(TilemCalc* calc)
{
	int pageA, pageB;

	switch (calc->hwregs[HW_VERSION]) {
	case 1:
		if (calc->hwregs[PORT5] & 0x40)
			pageA = 2;
		else if (calc->hwregs[PORT5] & 1)
			pageA = 1;
		else
			pageA = 0;

		if (calc->hwregs[PORT6] & 0x40)
			pageB = 2;
		else if (calc->hwregs[PORT6] & 1)
			pageB = 1;
		else
			pageB = 0;
		break;

	case 2:
		if (calc->hwregs[PORT2] & 0x40)
			pageA = 2;
		else if (calc->hwregs[PORT2] & 1)
			pageA = 1;
		else
			pageA = 0;

		if (calc->hwregs[PORT2] & 0x80)
			pageB = 2;
		else if (calc->hwregs[PORT2] & 8)
			pageB = 1;
		else
			pageB = 0;
		break;

	default:
		/* unknown HW version - ignore all output values and
		   use standard mapping */
		pageA = 1;
		pageB = 0;
	}

	calc->mempagemap[1] = pageA;
	calc->mempagemap[2] = pageB;
	calc->mempagemap[3] = 2;
}

void x1_z80_out(TilemCalc* calc, dword port, byte value)
{
	if (!calc->hwregs[HW_VERSION] && calc->z80.r.pc.d < 0x8000) {
		/* detect version */
		if (port == 0x05 || port == 0x06) {
			calc->hwregs[HW_VERSION] = 1;
			setup_mapping(calc);
		}
		else if (port == 0x10 || port == 0x11) {
			calc->lcd.rowstride = 15;
			calc->hwregs[HW_VERSION] = 2;
			setup_mapping(calc);
		}
	}

	if (calc->hwregs[HW_VERSION] == 1)
		port &= 7;

	switch(port&0x1f) {
	case 0x00:
		calc->lcd.addr = ((value & 0x1f) << 8);
		calc->z80.lastlcdwrite = calc->z80.clock;
		break;

	case 0x01:
		tilem_keypad_set_group(calc, value);
		break;

	case 0x02:
		calc->hwregs[PORT2] = value;
		if (calc->hwregs[HW_VERSION] != 2)
			calc->lcd.contrast = 16 + (value & 0x1f);
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

		calc->hwregs[PORT3] = value;
		calc->lcd.active = calc->poweronhalt = ((value & 8) >> 3);
		break;

	case 0x04:
		calc->hwregs[PORT4] = value;

		if (calc->hwregs[HW_VERSION] == 1) {
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
		}
		break;

	case 0x05:
		calc->hwregs[PORT5] = value;
		setup_mapping(calc);
		break;

	case 0x06:
		calc->hwregs[PORT6] = value;
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

void x1_z80_ptimer(TilemCalc* calc, int id)
{
	switch (id) {
	case TIMER_INT:
		if (calc->hwregs[PORT3] & 0x02)
			calc->z80.interrupts |= TILEM_INTERRUPT_TIMER1;
		break;
	}
}

void x1_get_lcd(TilemCalc* calc, byte* data)
{
	if (calc->hwregs[HW_VERSION] == 2)
		tilem_lcd_t6a04_get_data(calc, data);
	else
		tilem_lcd_t6a43_get_data(calc, data);
}
