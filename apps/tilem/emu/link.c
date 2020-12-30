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

/* Internal link port control */

static inline void dbus_interrupt(TilemCalc* calc, dword inttype,
				  dword mask)
{
	if (!(calc->linkport.mode & mask))
		return;

	calc->z80.interrupts |= inttype;
}

static inline void dbus_set_lines(TilemCalc* calc, byte lines)
{
	if (lines != calc->linkport.lines) {
		calc->linkport.lines = lines;
		if (calc->linkport.linkemu == TILEM_LINK_EMULATOR_BLACK) {
			tilem_z80_stop(calc, TILEM_STOP_LINK_STATE);
		}
	}
}

static inline void dbus_set_extlines(TilemCalc* calc, byte lines)
{
	if ((lines ^ calc->linkport.extlines) & ~calc->linkport.lines) {
		dbus_interrupt(calc, TILEM_INTERRUPT_LINK_ACTIVE,
			       TILEM_LINK_MODE_INT_ON_ACTIVE);
	}
	calc->linkport.extlines = lines;
}

void tilem_linkport_assist_timer(TilemCalc* calc,
				 void* data TILEM_ATTR_UNUSED)
{
	TilemLinkport* lp = &calc->linkport;

	if (lp->assistflags & TILEM_LINK_ASSIST_WRITE_BUSY) {
		lp->assistflags &= ~TILEM_LINK_ASSIST_WRITE_BUSY;
		lp->assistflags |= TILEM_LINK_ASSIST_WRITE_ERROR;
	}
	else if (lp->assistflags & TILEM_LINK_ASSIST_READ_BUSY) {
		lp->assistflags &= ~TILEM_LINK_ASSIST_READ_BUSY;
		lp->assistflags |= TILEM_LINK_ASSIST_READ_ERROR;
	}
	else
		return;

	dbus_interrupt(calc, TILEM_INTERRUPT_LINK_ERROR,
		       TILEM_LINK_MODE_INT_ON_ERROR);
}

static inline void assist_set_timeout(TilemCalc* calc)
{
	if (calc->linkport.mode & TILEM_LINK_MODE_NO_TIMEOUT)
		return;

	tilem_z80_set_timer(calc, TILEM_TIMER_LINK_ASSIST, 2000000, 0, 1);
}

static inline void assist_clear_timeout(TilemCalc* calc)
{
	tilem_z80_set_timer(calc, TILEM_TIMER_LINK_ASSIST, 0, 0, 0);
}

static void assist_update_write(TilemCalc* calc)
{
	switch (calc->linkport.extlines) {
	case 0:
		if (calc->linkport.lines == 0 && calc->linkport.assistoutbits > 0) {
			/* Ready to send next bit */
			if (calc->linkport.assistout & 1)
				dbus_set_lines(calc, 2);
			else
				dbus_set_lines(calc, 1);
			calc->linkport.assistout >>= 1;
			calc->linkport.assistoutbits--;
			assist_set_timeout(calc); /* other device must
						     respond within 2
						     seconds */
		}
		else if (calc->linkport.lines == 0) {
			/* Finished sending a byte */
			calc->linkport.assistflags &= ~TILEM_LINK_ASSIST_WRITE_BUSY;
			assist_clear_timeout(calc);
		}
		break;

	case 1:
	case 2:
		if (calc->linkport.extlines == (calc->linkport.lines ^ 3)) {
			/* Other device acknowledged our bit.  Note
			   that the timeout is NOT set at this point.
			   My experiments indicate that the assist
			   will wait, apparently indefinitely, for the
			   other device to bring its lines high. */
			dbus_set_lines(calc, 0);
			assist_clear_timeout(calc);
		}
		break;

	case 3:
		/* illegal line state; flag error */
		calc->linkport.assistflags &= ~TILEM_LINK_ASSIST_WRITE_BUSY;
		calc->linkport.assistflags |= TILEM_LINK_ASSIST_WRITE_ERROR;
		dbus_set_lines(calc, 0);
		assist_clear_timeout(calc);
		dbus_interrupt(calc, TILEM_INTERRUPT_LINK_ERROR,
			       TILEM_LINK_MODE_INT_ON_ERROR);
		break;
	}
}

static void assist_update_read(TilemCalc* calc)
{
	switch (calc->linkport.extlines) {
	case 0:
		/* Finished receiving a bit */
		if (calc->linkport.lines == 1) {
			calc->linkport.assistin >>= 1;
			calc->linkport.assistin |= 0x80;
			calc->linkport.assistinbits++;
		}
		else if (calc->linkport.lines == 2) {
			calc->linkport.assistin >>= 1;
			calc->linkport.assistinbits++;
		}

		if (calc->linkport.assistinbits >= 8) {
			/* finished receiving a byte */
			calc->linkport.assistlastbyte = calc->linkport.assistin;
			calc->linkport.assistflags &= ~TILEM_LINK_ASSIST_READ_BUSY;
			calc->linkport.assistflags |= TILEM_LINK_ASSIST_READ_BYTE;
			assist_clear_timeout(calc);
		}
		else {
			assist_set_timeout(calc); /* other device must
						     send next bit
						     within 2
						     seconds */
		}

		dbus_set_lines(calc, 0); /* indicate we're ready to
					    receive */
		break;

	case 1:
		/* other device sent a zero; acknowledge it */
		calc->linkport.assistflags |= TILEM_LINK_ASSIST_READ_BUSY;
		dbus_set_lines(calc, 2);
		assist_set_timeout(calc); /* other device must bring
					     both lines high again
					     within 2 seconds */
		break;

	case 2:
		/* same as above, but other device sent a one */
		calc->linkport.assistflags |= TILEM_LINK_ASSIST_READ_BUSY;
		dbus_set_lines(calc, 1);
		assist_set_timeout(calc); /* other device must bring
					     both lines high again
					     within 2 seconds */
		break;

	case 3:
		/* illegal line state; flag error */
		calc->linkport.assistflags &= ~TILEM_LINK_ASSIST_READ_BUSY;
		calc->linkport.assistflags |= TILEM_LINK_ASSIST_READ_ERROR;
		dbus_set_lines(calc, 0);
		assist_clear_timeout(calc);
		dbus_interrupt(calc, TILEM_INTERRUPT_LINK_ERROR,
			       TILEM_LINK_MODE_INT_ON_ERROR);
		break;
	}
}

static void graylink_update_write(TilemCalc* calc)
{
	switch (calc->linkport.lines) {
	case 0:
		if (calc->linkport.extlines == 0 && calc->linkport.graylinkoutbits > 1) {
			/* Ready to send next bit */
			if (calc->linkport.graylinkout & 1)
				dbus_set_extlines(calc, 2);
			else
				dbus_set_extlines(calc, 1);
			calc->linkport.graylinkout >>= 1;
			calc->linkport.graylinkoutbits--;
		}
		else if (calc->linkport.extlines == 0) {
			/* Finished sending a byte */
			calc->linkport.graylinkoutbits = 0;
			tilem_z80_stop(calc, TILEM_STOP_LINK_WRITE_BYTE);
		}
		break;

	case 1:
	case 2:
		if (calc->linkport.extlines == (calc->linkport.lines ^ 3))
			/* Other device acknowledged our bit */
			dbus_set_extlines(calc, 0);
		break;

	case 3:
		/* illegal line state; flag error */
		dbus_set_extlines(calc, 0);
		calc->linkport.graylinkoutbits = 0;
		tilem_z80_stop(calc, TILEM_STOP_LINK_ERROR);
		break;
	}
}

static void graylink_update_read(TilemCalc* calc)
{
	switch (calc->linkport.lines) {
	case 0:
		/* Finished receiving a bit */
		if (calc->linkport.extlines == 1) {
			calc->linkport.graylinkin >>= 1;
			calc->linkport.graylinkin |= 0x80;
			calc->linkport.graylinkinbits++;
		}
		else if (calc->linkport.extlines == 2) {
			calc->linkport.graylinkin >>= 1;
			calc->linkport.graylinkinbits++;
		}

		if (calc->linkport.graylinkinbits >= 8) {
			/* finished receiving a byte */
			tilem_z80_stop(calc, TILEM_STOP_LINK_READ_BYTE);
		}

		dbus_set_extlines(calc, 0);
		break;

	case 1:
		/* other device sent a zero; acknowledge it */
		dbus_set_extlines(calc, 2);
		break;

	case 2:
		/* same as above, but other device sent a one */
		dbus_set_extlines(calc, 1);
		break;

	case 3:
		/* illegal line state; flag error */
		dbus_set_extlines(calc, 0);
		calc->linkport.graylinkinbits = 0;
		tilem_z80_stop(calc, TILEM_STOP_LINK_ERROR);
		break;
	}
}

static void dbus_update(TilemCalc* calc)
{
	byte oldlines;

	do {
		if (calc->linkport.linkemu == TILEM_LINK_EMULATOR_GRAY) {
			if (calc->linkport.graylinkoutbits) {
				graylink_update_write(calc);
			}
			else if (calc->linkport.graylinkinbits != 8) {
				graylink_update_read(calc);
			}
		}

		oldlines = calc->linkport.lines;
		if (calc->linkport.assistflags & TILEM_LINK_ASSIST_WRITE_BUSY) {
			assist_update_write(calc);
		}
		else if (calc->linkport.mode & TILEM_LINK_MODE_ASSIST
			 && !(calc->linkport.assistflags & TILEM_LINK_ASSIST_READ_BYTE)) {
			assist_update_read(calc);
		}
	} while (oldlines != calc->linkport.lines);

	if ((calc->linkport.assistflags & TILEM_LINK_ASSIST_READ_BYTE)
	    && (calc->linkport.mode & TILEM_LINK_MODE_INT_ON_READ))
		calc->z80.interrupts |= TILEM_INTERRUPT_LINK_READ;
	else
		calc->z80.interrupts &= ~TILEM_INTERRUPT_LINK_READ;

	if (!(calc->linkport.assistflags & (TILEM_LINK_ASSIST_READ_BUSY
					    | TILEM_LINK_ASSIST_WRITE_BUSY))
	    && (calc->linkport.mode & TILEM_LINK_MODE_INT_ON_IDLE))
		calc->z80.interrupts |= TILEM_INTERRUPT_LINK_IDLE;
	else
		calc->z80.interrupts &= ~TILEM_INTERRUPT_LINK_IDLE;
}

void tilem_linkport_reset(TilemCalc* calc)
{
	dbus_set_lines(calc, 0);
	assist_clear_timeout(calc);
	calc->linkport.mode = 0;
	calc->linkport.assistflags = 0;
	calc->linkport.assistin = 0;
	calc->linkport.assistinbits = 0;
	calc->linkport.assistout = 0;
	calc->linkport.assistoutbits = 0;
	calc->linkport.assistlastbyte = 0;
}

byte tilem_linkport_get_lines(TilemCalc* calc)
{
	//dbus_update(calc);
	return (~calc->linkport.lines & ~calc->linkport.extlines & 3);
}

void tilem_linkport_set_lines(TilemCalc* calc, byte lines)
{
	if (!(calc->linkport.mode & TILEM_LINK_MODE_ASSIST)
	    && !(calc->linkport.assistflags & TILEM_LINK_ASSIST_WRITE_BUSY))
		dbus_set_lines(calc, lines & 3);

	dbus_update(calc);
}

byte tilem_linkport_read_byte(TilemCalc* calc)
{
	byte value = calc->linkport.assistin;
	calc->linkport.assistflags &= ~(TILEM_LINK_ASSIST_READ_BYTE
			     | TILEM_LINK_ASSIST_READ_BUSY);
	calc->linkport.assistinbits = 0;
	dbus_update(calc);
	return value;
}

void tilem_linkport_write_byte(TilemCalc* calc, byte data)
{
	if (calc->linkport.assistflags & (TILEM_LINK_ASSIST_READ_BUSY
			       | TILEM_LINK_ASSIST_WRITE_BUSY))
		return;

	dbus_set_lines(calc, 0);
	calc->linkport.assistout = data;
	calc->linkport.assistoutbits = 8;
	calc->linkport.assistflags |= TILEM_LINK_ASSIST_WRITE_BUSY;
	dbus_update(calc);
}

unsigned int tilem_linkport_get_assist_flags(TilemCalc* calc)
{
	//dbus_update(calc);
	return calc->linkport.assistflags;
}

void tilem_linkport_set_mode(TilemCalc* calc, unsigned int mode)
{
	if ((mode ^ calc->linkport.mode) & TILEM_LINK_MODE_ASSIST) {
		dbus_set_lines(calc, 0);
		calc->linkport.assistflags &= ~(TILEM_LINK_ASSIST_READ_BUSY
				     | TILEM_LINK_ASSIST_WRITE_BUSY);
		assist_clear_timeout(calc);
	}

	if (!(mode & TILEM_LINK_MODE_INT_ON_ACTIVE))
		calc->z80.interrupts &= ~TILEM_INTERRUPT_LINK_ACTIVE;
	if (!(mode & TILEM_LINK_MODE_INT_ON_ERROR))
		calc->z80.interrupts &= ~TILEM_INTERRUPT_LINK_ERROR;

	calc->linkport.mode = mode;

	dbus_update(calc);
}


/* External BlackLink emulation */

void tilem_linkport_blacklink_set_lines(TilemCalc* calc, byte lines)
{
	dbus_set_extlines(calc, lines & 3);
	dbus_update(calc);
}

byte tilem_linkport_blacklink_get_lines(TilemCalc* calc)
{
	dbus_update(calc);
	return (~calc->linkport.lines & ~calc->linkport.extlines & 3);
}


/* External GrayLink emulation */

void tilem_linkport_graylink_reset(TilemCalc* calc)
{
	calc->linkport.graylinkin = 0;
	calc->linkport.graylinkinbits = 0;
	calc->linkport.graylinkout = 0;
	calc->linkport.graylinkoutbits = 0;
	dbus_set_extlines(calc, 0);
	dbus_update(calc);
}

int tilem_linkport_graylink_ready(TilemCalc* calc)
{
	if (calc->linkport.graylinkoutbits
	    || calc->linkport.graylinkinbits)
		return 0;
	else
		return 1;
}

int tilem_linkport_graylink_send_byte(TilemCalc* calc, byte value)
{
	if (!tilem_linkport_graylink_ready(calc))
		return -1;

	dbus_set_extlines(calc, 0);

	/* set to 9 because we want to wait for the calc to bring both
	   link lines low before we send the first bit, and also after
	   we send the last bit */
	calc->linkport.graylinkoutbits = 9;

	calc->linkport.graylinkout = value;
	dbus_update(calc);
	return 0;
}

int tilem_linkport_graylink_get_byte(TilemCalc* calc)
{
	dbus_update(calc);
	if (calc->linkport.graylinkinbits != 8)
		return -1;

	calc->linkport.graylinkinbits = 0;
	return calc->linkport.graylinkin;
}
