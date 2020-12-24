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
#include <time.h>
#include <tilem.h>

#include "xn.h"

static void set_lcd_wait_timer(TilemCalc* calc)
{
	static const int delaytime[8] = { 48, 112, 176, 240,
					  304, 368, 432, 496 };
	int i;

	switch (calc->hwregs[PORT20] & 3) {
	case 0:
		return;
	case 1:
		i = (calc->hwregs[PORT2F] & 3);
		break;
	case 2:
		i = ((calc->hwregs[PORT2F] >> 2) & 7);
		break;
	default:
		i = ((calc->hwregs[PORT2F] >> 5) & 7);
		break;
	}

	tilem_z80_set_timer(calc, TIMER_LCD_WAIT, delaytime[i], 0, 0);
	calc->hwregs[LCD_WAIT] = 1;
}

byte xn_z80_in(TilemCalc* calc, dword port)
{
	/* FIXME: measure actual levels */
	static const byte battlevel[4] = { 33, 39, 36, 43 };
	byte v;
	unsigned int f;
	time_t curtime;

	switch(port&0xff) {
	case 0x00:
		if (tilem_z80_timer_running(calc, TIMER_FREEZE_LINK_PORT))
			return(3);

		v = tilem_linkport_get_lines(calc);
		v |= (calc->linkport.lines << 4);
		return(v);

	case 0x01:
		return(tilem_keypad_read_keys(calc));

	case 0x02:
		v = battlevel[calc->hwregs[PORT4] >> 6];
		return ((calc->battery >= v ? 0xe1 : 0xe0)
			| (calc->hwregs[LCD_WAIT] ? 0 : 2)
			| (calc->flash.unlock << 2));

	case 0x03:
		return(calc->hwregs[PORT3]);

	case 0x04:
		v = (calc->keypad.onkeydown ? 0x00 : 0x08);

		if (calc->z80.interrupts & TILEM_INTERRUPT_ON_KEY)
			v |= 0x01;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER1)
			v |= 0x02;
		if (calc->z80.interrupts & TILEM_INTERRUPT_TIMER2)
			v |= 0x04;
		if (calc->z80.interrupts & TILEM_INTERRUPT_LINK_ACTIVE)
			v |= 0x10;

		if (calc->usertimers[0].status & TILEM_USER_TIMER_FINISHED)
			v |= 0x20;
		if (calc->usertimers[1].status & TILEM_USER_TIMER_FINISHED)
			v |= 0x40;
		if (calc->usertimers[2].status & TILEM_USER_TIMER_FINISHED)
			v |= 0x80;

		return(v);

	case 0x05:
		return(calc->hwregs[PORT5] & 0x0f);

	case 0x06:
		return(calc->hwregs[PORT6]);

	case 0x07:
		return(calc->hwregs[PORT7]);

	case 0x08:
		return(calc->hwregs[PORT8]);

	case 0x09:
		f = tilem_linkport_get_assist_flags(calc);

		if (f & (TILEM_LINK_ASSIST_READ_BUSY
			 | TILEM_LINK_ASSIST_WRITE_BUSY))
			v = 0x00;
		else
			v = 0x20;

		if (calc->z80.interrupts & TILEM_INTERRUPT_LINK_READ)
			v |= 0x01;
		if (calc->z80.interrupts & TILEM_INTERRUPT_LINK_IDLE)
			v |= 0x02;
		if (calc->z80.interrupts & TILEM_INTERRUPT_LINK_ERROR)
			v |= 0x04;
		if (f & TILEM_LINK_ASSIST_READ_BUSY)
			v |= 0x08;
		if (f & TILEM_LINK_ASSIST_READ_BYTE)
			v |= 0x10;
		if (f & (TILEM_LINK_ASSIST_READ_ERROR
			 | TILEM_LINK_ASSIST_WRITE_ERROR))
			v |= 0x40;
		if (f & TILEM_LINK_ASSIST_WRITE_BUSY)
			v |= 0x80;

		calc->z80.interrupts &= ~TILEM_INTERRUPT_LINK_ERROR;

		return(v);

	case 0x0A:
		v = calc->linkport.assistlastbyte;
		tilem_linkport_read_byte(calc);
		return(v);

	case 0x0E:
		return(calc->hwregs[PORTE] & 3);

	case 0x0F:
		return(calc->hwregs[PORTF] & 3);

	case 0x10:
	case 0x12:
		calc->z80.clock += calc->hwregs[LCD_PORT_DELAY];
		set_lcd_wait_timer(calc);
		return(tilem_lcd_t6a04_status(calc));

	case 0x11:
	case 0x13:
		calc->z80.clock += calc->hwregs[LCD_PORT_DELAY];
		set_lcd_wait_timer(calc);
		return(tilem_lcd_t6a04_read(calc));

	case 0x15:
		return(0x45);	/* ??? */

	case 0x1C:
		return(tilem_md5_assist_get_value(calc));

	case 0x1D:
		return(tilem_md5_assist_get_value(calc) >> 8);

	case 0x1E:
		return(tilem_md5_assist_get_value(calc) >> 16);

	case 0x1F:
		return(tilem_md5_assist_get_value(calc) >> 24);

	case 0x20:
		return(calc->hwregs[PORT20] & 3);

	case 0x21:
		return(calc->hwregs[PORT21] & 0x33);

	case 0x22:
		return(calc->hwregs[PORT22]);

	case 0x23:
		return(calc->hwregs[PORT23]);

	case 0x25:
		return(calc->hwregs[PORT25]);

	case 0x26:
		return(calc->hwregs[PORT26]);

	case 0x27:
		return(calc->hwregs[PORT27]);

	case 0x28:
		return(calc->hwregs[PORT28]);

	case 0x29:
		return(calc->hwregs[PORT29]);

	case 0x2A:
		return(calc->hwregs[PORT2A]);

	case 0x2B:
		return(calc->hwregs[PORT2B]);

	case 0x2C:
		return(calc->hwregs[PORT2C]);

	case 0x2D:
		return(calc->hwregs[PORT2D] & 3);

	case 0x2E:
		return(calc->hwregs[PORT2E]);

	case 0x2F:
		return(calc->hwregs[PORT2F]);

	case 0x30:
		return(calc->usertimers[0].frequency);
	case 0x31:
		return(calc->usertimers[0].status);
	case 0x32:
		return(tilem_user_timer_get_value(calc, 0));

	case 0x33:
		return(calc->usertimers[1].frequency);
	case 0x34:
		return(calc->usertimers[1].status);
	case 0x35:
		return(tilem_user_timer_get_value(calc, 1));

	case 0x36:
		return(calc->usertimers[2].frequency);
	case 0x37:
		return(calc->usertimers[2].status);
	case 0x38:
		return(tilem_user_timer_get_value(calc, 2));

	case 0x39:
		return(0xf0);	/* ??? */

	case 0x40:
		return calc->hwregs[CLOCK_MODE];

	case 0x41:	
		return calc->hwregs[CLOCK_INPUT]&0xff;

	case 0x42:
		return (calc->hwregs[CLOCK_INPUT]>>8)&0xff;

	case 0x43:
		return (calc->hwregs[CLOCK_INPUT]>>16)&0xff;

	case 0x44:
		return (calc->hwregs[CLOCK_INPUT]>>24)&0xff;

	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
		if (calc->hwregs[CLOCK_MODE] & 1) {
			time(&curtime);
		}
		else {
			curtime = 0;
		}
		curtime += calc->hwregs[CLOCK_DIFF];
		return (curtime >> ((port - 0x45) * 8));

	case 0x4C:
		return(0x22);

	case 0x4D:
		/* USB port - not emulated, calculator should
		   recognize that the USB cable is
		   disconnected.

		   Thanks go to Dan Englender for these
		   values. */

		return(0xA5);

	case 0x55:
		return(0x1F);

	case 0x56:
		return(0x00);

	case 0x57:
		return(0x50);

	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x14:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
		return(0);
	}

	tilem_warning(calc, "Input from port %x", port);
	return(0x00);
}


static void setup_mapping(TilemCalc* calc)
{
	unsigned int pageA, pageB, pageC;

	if (calc->hwregs[PORT6] & 0x80)
		pageA = (0x80 | (calc->hwregs[PORT6] & 7));
	else
		pageA = (calc->hwregs[PORT6] & 0x7f);

	if (calc->hwregs[PORT7] & 0x80)
		pageB = (0x80 | (calc->hwregs[PORT7] & 7));
	else
		pageB = (calc->hwregs[PORT7] & 0x7f);

	pageC = (0x80 | (calc->hwregs[PORT5] & 7));

	if (calc->hwregs[PORT4] & 1) {
		calc->mempagemap[1] = (pageA & ~1);
		calc->mempagemap[2] = (pageA | 1);
		calc->mempagemap[3] = pageB;
	}
	else {
		calc->mempagemap[1] = pageA;
		calc->mempagemap[2] = pageB;
		calc->mempagemap[3] = pageC;
	}
}

static void setup_clockdelays(TilemCalc* calc)
{
	byte lcdport = calc->hwregs[PORT29 + (calc->hwregs[PORT20] & 3)];
	byte memport = calc->hwregs[PORT2E];

	if (!(lcdport & 1))
		memport &= ~0x07;
	if (!(lcdport & 2))
		memport &= ~0x70;

	calc->hwregs[FLASH_EXEC_DELAY] = (memport & 1);
	calc->hwregs[FLASH_READ_DELAY] = ((memport >> 1) & 1);
	calc->hwregs[FLASH_WRITE_DELAY] = ((memport >> 2) & 1);

	calc->hwregs[RAM_EXEC_DELAY] = ((memport >> 4) & 1);
	calc->hwregs[RAM_READ_DELAY] = ((memport >> 5) & 1);
	calc->hwregs[RAM_WRITE_DELAY] = ((memport >> 6) & 1);

	calc->hwregs[LCD_PORT_DELAY] = (lcdport >> 2);
}

void xn_z80_out(TilemCalc* calc, dword port, byte value)
{
	static const int tmrvalues[4] = { 1953, 4395, 6836, 9277 };
	int t, r;
	unsigned int mode;
	time_t curtime;

	switch(port&0xff) {
	case 0x00:
		if (value == 0
		    && calc->linkport.lines != 0
		    && calc->linkport.extlines == 0) {
			/* Kludge to work around TI's broken
			   RecAByteIO implementation on 2.46+, which
			   will fail if the sending device is too
			   fast. */
			tilem_z80_set_timer(calc, TIMER_FREEZE_LINK_PORT,
			                    100, 0, 0);
		}

		tilem_linkport_set_lines(calc, value);
		break;

	case 0x01:
		tilem_keypad_set_group(calc, value);
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

		if (value & 0x06) {
			calc->usertimers[0].status &= ~TILEM_USER_TIMER_NO_HALT_INT;
			calc->usertimers[1].status &= ~TILEM_USER_TIMER_NO_HALT_INT;
			calc->usertimers[2].status &= ~TILEM_USER_TIMER_NO_HALT_INT;
		}
		else {
			calc->usertimers[0].status |= TILEM_USER_TIMER_NO_HALT_INT;
			calc->usertimers[1].status |= TILEM_USER_TIMER_NO_HALT_INT;
			calc->usertimers[2].status |= TILEM_USER_TIMER_NO_HALT_INT;
		}

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
		calc->hwregs[PORT4] = value;

		t = tmrvalues[(value & 6) >> 1];
		tilem_z80_set_timer_period(calc, TIMER_INT1, t);
		tilem_z80_set_timer_period(calc, TIMER_INT2A, t);
		tilem_z80_set_timer_period(calc, TIMER_INT2B, t);

		setup_mapping(calc);
		break;
	
	case 0x05:
		calc->hwregs[PORT5] = value & 0x0f;
		setup_mapping(calc);
		break;

	case 0x06:
		calc->hwregs[PORT6] = value;
		setup_mapping(calc);
		break;

	case 0x07:
		calc->hwregs[PORT7] = value;
		setup_mapping(calc);
		break;

	case 0x08:
		calc->hwregs[PORT8] = value;

		mode = calc->linkport.mode;

		if (value & 0x01)
			mode |= TILEM_LINK_MODE_INT_ON_READ;
		else
			mode &= ~TILEM_LINK_MODE_INT_ON_READ;

		if (value & 0x02)
			mode |= TILEM_LINK_MODE_INT_ON_IDLE;
		else
			mode &= ~TILEM_LINK_MODE_INT_ON_IDLE;

		if (value & 0x04)
			mode |= TILEM_LINK_MODE_INT_ON_ERROR;
		else
			mode &= ~TILEM_LINK_MODE_INT_ON_ERROR;

		if (value & 0x80)
			mode &= ~TILEM_LINK_MODE_ASSIST;
		else
			mode |= TILEM_LINK_MODE_ASSIST;

		tilem_linkport_set_mode(calc, mode);
		break;

	case 0x09:
		calc->hwregs[PORT9] = value;
		break;

	case 0x0A:
		calc->hwregs[PORTA] = value;
		break;

	case 0x0B:
		calc->hwregs[PORTB] = value;
		break;

	case 0x0C:
		calc->hwregs[PORTC] = value;
		break;


	case 0x0D:
		if (!(calc->hwregs[PORT8] & 0x80))
			tilem_linkport_write_byte(calc, value);
		break;

	case 0x0E:
		calc->hwregs[PORTE] = value;
		break;

	case 0x0F:
		calc->hwregs[PORTF] = value;
		break;

	case 0x10:
	case 0x12:
		calc->z80.clock += calc->hwregs[LCD_PORT_DELAY];
		set_lcd_wait_timer(calc);
		tilem_lcd_t6a04_control(calc, value);
		break;

	case 0x11:
	case 0x13:
		calc->z80.clock += calc->hwregs[LCD_PORT_DELAY];
		set_lcd_wait_timer(calc);
		tilem_lcd_t6a04_write(calc, value);
		break;

	case 0x14:
		if (calc->hwregs[PROTECTSTATE] == 7) {
			/*
			if (value & 1)
				tilem_message(calc, "Flash unlocked");
			else
				tilem_message(calc, "Flash locked");
			*/
			calc->flash.unlock = value&1;
		}
		break;

	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
		r = (port & 0xff) - 0x18;
		calc->md5assist.regs[r] >>= 8;
		calc->md5assist.regs[r] |= (value << 24);
		break;

	case 0x1E:
		calc->md5assist.shift = value & 0x1f;
		break;

	case 0x1F:
		calc->md5assist.mode = value & 3;
		break;

	case 0x20:
		calc->hwregs[PORT20] = value;

		if (value & 3) {
			tilem_z80_set_speed(calc, 15000);
		}
		else {
			tilem_z80_set_speed(calc, 6000);
		}

		setup_clockdelays(calc);
		break;

	case 0x21:
		if (calc->flash.unlock && calc->hwregs[PROTECTSTATE] == 7) {
			calc->hwregs[PORT21] = value;

			/* FIXME: these restrictions were tested on
			   83+ SE; someone should confirm them for
			   84+ */
			switch (value & 0x30) {
			case 0x00:
				/* restrict pp. 0, 2, 4, 6, 8, A, C, E */
				calc->hwregs[NO_EXEC_RAM] = 0x5555;
				break;

			case 0x10:
				/* restrict pp. 0, 3, 4, 7, 8, B, C, F */
				calc->hwregs[NO_EXEC_RAM] = 0x9999;
				break;

			case 0x20:
				/* restrict pp. 0, 3-7, 8, B-F */
				calc->hwregs[NO_EXEC_RAM] = 0xF9F9;
				break;

			case 0x30:
				/* restrict pp. 0, 3-F */
				calc->hwregs[NO_EXEC_RAM] = 0xFFF9;
				break;
			}
		}
		break;

	case 0x22:
		if (calc->flash.unlock && calc->hwregs[PROTECTSTATE] == 7) {
			calc->hwregs[PORT22] = value;
		}
		break;

	case 0x23:
		if (calc->flash.unlock && calc->hwregs[PROTECTSTATE] == 7) {
			calc->hwregs[PORT23] = value;
		}
		break;

	case 0x25:
		if (calc->flash.unlock && calc->hwregs[PROTECTSTATE] == 7) {
			calc->hwregs[PORT25] = value;
		}
		break;

	case 0x26:
		if (calc->flash.unlock && calc->hwregs[PROTECTSTATE] == 7) {
			calc->hwregs[PORT26] = value;
		}
		break;

	case 0x27:
		calc->hwregs[PORT27] = value;
		break;

	case 0x28:
		calc->hwregs[PORT28] = value;
		break;

	case 0x29:
		calc->hwregs[PORT29] = value;
		setup_clockdelays(calc);
		break;

	case 0x2A:
		calc->hwregs[PORT2A] = value;
		setup_clockdelays(calc);
		break;

	case 0x2B:
		calc->hwregs[PORT2B] = value;
		setup_clockdelays(calc);
		break;

	case 0x2C:
		calc->hwregs[PORT2C] = value;
		setup_clockdelays(calc);
		break;

	case 0x2D:
		calc->hwregs[PORT2D] = value;
		break;

	case 0x2E:
		calc->hwregs[PORT2E] = value;
		setup_clockdelays(calc);
		break;

	case 0x2F:
		calc->hwregs[PORT2F] = value;
		break;

	case 0x30:
		tilem_user_timer_set_frequency(calc, 0, value);
		break;
	case 0x31:
		tilem_user_timer_set_mode(calc, 0, value);
		break;
	case 0x32:
		tilem_user_timer_start(calc, 0, value);
		break;

	case 0x33:
		tilem_user_timer_set_frequency(calc, 1, value);
		break;
	case 0x34:
		tilem_user_timer_set_mode(calc, 1, value);
		break;
	case 0x35:
		tilem_user_timer_start(calc, 1, value);
		break;

	case 0x36:
		tilem_user_timer_set_frequency(calc, 2, value);
		break;
	case 0x37:
		tilem_user_timer_set_mode(calc, 2, value);
		break;
	case 0x38:
		tilem_user_timer_start(calc, 2, value);
		break;

	case 0x40:
		time(&curtime);

		if ((calc->hwregs[CLOCK_MODE] & 1) != (value & 1)) {
			if (value & 1)
				calc->hwregs[CLOCK_DIFF] -= curtime;
			else
				calc->hwregs[CLOCK_DIFF] += curtime;
		}

		if (!(calc->hwregs[CLOCK_MODE] & 2) && (value & 2)) {
			calc->hwregs[CLOCK_DIFF] = calc->hwregs[CLOCK_INPUT];
			if (value & 1)
				calc->hwregs[CLOCK_DIFF] -= curtime;
		}
		calc->hwregs[CLOCK_MODE] = value & 3;
		break;

	case 0x41:
		calc->hwregs[CLOCK_INPUT] &= 0xffffff00;
		calc->hwregs[CLOCK_INPUT] |= value;
		break;

	case 0x42:
		calc->hwregs[CLOCK_INPUT] &= 0xffff00ff;
		calc->hwregs[CLOCK_INPUT] |= (value << 8);
		break;

	case 0x43:
		calc->hwregs[CLOCK_INPUT] &= 0xff00ffff;
		calc->hwregs[CLOCK_INPUT] |= (value << 16);
		break;

	case 0x44:
		calc->hwregs[CLOCK_INPUT] &= 0x00ffffff;
		calc->hwregs[CLOCK_INPUT] |= (value << 24);
		break;
	}

	return;
}

void xn_z80_instr(TilemCalc* calc, dword opcode)
{
	dword pa;
	byte l, h;

	switch (opcode) {
	case 0xeded:
		/* emulator control instruction */
		l = xn_z80_rdmem(calc, calc->z80.r.pc.d);
		h = xn_z80_rdmem(calc, calc->z80.r.pc.d + 1);

		calc->z80.r.pc.d += 2;

		opcode = (l | (h << 8));
		switch (opcode) {
		case 0x1000: /* Power off */
		case 0x1001: /* Power on */
		case 0x1002: /* Prepare for power off */
			break;

		case 0x1003:
			/* Disable Nspire keypad */
			tilem_message(calc, "Keypad locked");
			break;

		case 0x1004:
			/* Enable Nspire keypad */
			tilem_message(calc, "Keypad unlocked");
			break;

		case 0x1005:
			/* ??? */
			break;

		case 0x1008:
			/* check if USB data waiting (?) */
			calc->z80.r.af.d |= 0x40;
			break;

		case 0x100C:
			/* ??? */
			calc->z80.r.af.d &= ~0x40;
			break;

		case 0x100D:
			/* check if USB link should be used (???) */
			calc->z80.r.af.d &= ~0x40;
			break;

		case 0x100E:
		case 0x100F:
			/* ??? */
			break;

		case 0x101C:
			/* disable USB device (???) */
			break;

		case 0x1020:
			/* check for something USB-related */
			calc->z80.r.af.d |= 0x40;
			break;

		case 0x1024:
			/* check if USB data waiting */
			calc->z80.r.af.d |= 0x40;
			break;

		case 0x1029:
			/* check for something USB-related */
			calc->z80.r.af.d |= 0x40;
			break;

		case 0x1027:
			/* check for something USB-related */
			calc->z80.r.af.d |= 0x40;
			break;

		case 0x102E:
			/* ??? */
			break;

		case 0x102F:
			/* ??? */
			break;

		default:
			tilem_warning(calc, "Unknown control instruction %x",
				      opcode);
		}
		break;

	case 0xedee:
		/* erase Flash at address HL */
		if (calc->flash.unlock) {
			pa = xn_mem_ltop(calc, calc->z80.r.hl.w.l);
			if (pa < 0x200000) {
				tilem_flash_erase_address(calc, pa);
			}
		}
		break;

	case 0xedef:
		/* enable Flash writing for following instruction */
		if (calc->flash.unlock) {
			calc->flash.state = 3;
		}
		break;

	default:
		tilem_warning(calc, "Invalid opcode %x", opcode);
		tilem_z80_exception(calc, TILEM_EXC_INSTRUCTION);
		break;
	}
}

void xn_z80_ptimer(TilemCalc* calc, int id)
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

	case TIMER_LCD_WAIT:
		calc->hwregs[LCD_WAIT] = 0;
		break;
	}
}
