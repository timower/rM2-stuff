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

void tilem_user_timers_reset(TilemCalc* calc)
{
	int i;

	for (i = 0; i < TILEM_MAX_USER_TIMERS; i++) {
		tilem_z80_set_timer(calc, TILEM_TIMER_USER1 + i, 0, 0, 0);
		calc->usertimers[i].frequency = 0;
		calc->usertimers[i].loopvalue = 0;
		calc->usertimers[i].status = 0;
	}
}

static inline dword get_duration(int fvalue, int nticks)
{
	qword t=0;

	if (fvalue & 0x80) {
		if (fvalue & 0x20)
			return 64 * nticks;
		else if (fvalue & 0x10)
			return 32 * nticks;
		else if (fvalue & 0x08)
			return 16 * nticks;
		else if (fvalue & 0x04)
			return 8 * nticks;
		else if (fvalue & 0x02)
			return 4 * nticks;
		else if (fvalue & 0x01)
			return 2 * nticks;
		else
			return nticks;
	}
	else if (fvalue & 0x40) {
		switch (fvalue & 7) {
		case 0:
			t = 3000000;
			break;
		case 1:
			t = 33000000;
			break;
		case 2:
			t = 328000000;
			break;
		case 3:
			t = 0xC3530D40; //3277000000;
			break;
		case 4:
			t = 1000000;
			break;
		case 5:
			t = 16000000;
			break;
		case 6:
			t = 256000000;
			break;
		case 7:
			t = 0xF4240000; //4096000000;
			break;
		}

		return ((t * nticks + 16384) / 32768);
	}

	return 0;
}

static dword get_normal_duration(const TilemUserTimer* tmr)
{
	if (tmr->loopvalue)
		return get_duration(tmr->frequency, tmr->loopvalue);
	else
		return get_duration(tmr->frequency, 256);
}

static dword get_overflow_duration(const TilemUserTimer* tmr)
{
	return get_duration(tmr->frequency, 256);
}

void tilem_user_timer_set_frequency(TilemCalc* calc, int n, byte value)
{
	TilemUserTimer* tmr = &calc->usertimers[n];

	/* writing to frequency control port stops timer; set
	   loopvalue to the current value of the counter */
	tmr->loopvalue = tilem_user_timer_get_value(calc, n);
	tilem_z80_set_timer(calc, TILEM_TIMER_USER1 + n, 0, 0, 0);
	tmr->frequency = value;
}

void tilem_user_timer_set_mode(TilemCalc* calc, int n, byte mode)
{
	TilemUserTimer* tmr = &calc->usertimers[n];

	/* setting mode clears "finished" and "overflow" flags */
	tmr->status = ((tmr->status & TILEM_USER_TIMER_NO_HALT_INT)
		       | (mode & 3));

	calc->z80.interrupts &= ~(TILEM_INTERRUPT_USER_TIMER1 << n);

	/* if timer is currently running, it will not overflow the
	   next time it expires -> set period to normal */
	if ((mode & TILEM_USER_TIMER_LOOP) || tmr->loopvalue == 0) {
		tilem_z80_set_timer_period(calc, TILEM_TIMER_USER1 + n,
					   get_normal_duration(tmr));
	}
	else {
		tilem_z80_set_timer_period(calc, TILEM_TIMER_USER1 + n, 0);
	}
}

void tilem_user_timer_start(TilemCalc* calc, int n, byte value)
{
	TilemUserTimer* tmr = &calc->usertimers[n];
	dword count, period;

	tmr->loopvalue = value;

	/* if a valid frequency is set, then writing to value port
	   starts timer */

	count = get_normal_duration(tmr);
	if (!count)
		return;

	if (!value) {
		/* input value 0 means loop indefinitely */
		period = get_overflow_duration(tmr);
	}
	else if (tmr->status & TILEM_USER_TIMER_FINISHED) {
		/* timer has already expired once -> it will overflow
		   the next time it expires (note that this happens
		   even if the loop flag isn't set) */
		period = get_overflow_duration(tmr);
	}
	else if (!(tmr->status & TILEM_USER_TIMER_LOOP)) {
		/* don't loop */
		period = 0;
	}
	else {
		/* timer hasn't expired yet; second iteration starts
		   from the same counter value as the first */
		period = count;
	}

	tilem_z80_set_timer(calc, TILEM_TIMER_USER1 + n, count, period,
			    (calc->usertimers[n].frequency & 0x80 ? 0 : 1));
}

byte tilem_user_timer_get_value(TilemCalc* calc, int n)
{
	TilemUserTimer* tmr = &calc->usertimers[n];
	dword period;

	if (!tilem_z80_timer_running(calc, TILEM_TIMER_USER1 + n))
		return tmr->loopvalue;

	period = get_overflow_duration(tmr);

	if (tmr->frequency & 0x80) {
		dword t = tilem_z80_get_timer_clocks
			(calc, TILEM_TIMER_USER1 + n);
		return ((t * 256) / period) % 256;
	}
	else {
		qword t = tilem_z80_get_timer_microseconds
			(calc, TILEM_TIMER_USER1 + n);
		return ((t * 256) / period) % 256;
	}
}

void tilem_user_timer_expired(TilemCalc* calc, void* data)
{
	int n = TILEM_PTR_TO_DWORD(data);
	TilemUserTimer* tmr = &calc->usertimers[n];

	/* input value 0 means loop indefinitely (don't set flags) */
	if (!tmr->loopvalue) {
		return;
	}

	/* if timer has already finished, set "overflow" flag */
	if (tmr->status & TILEM_USER_TIMER_FINISHED) {
		tmr->status |= TILEM_USER_TIMER_OVERFLOW;
	}

	/* set "finished" flag */
	tmr->status |= TILEM_USER_TIMER_FINISHED;

	/* generate interrupt if appropriate */
	if ((tmr->status & TILEM_USER_TIMER_INTERRUPT)
	    && (!(tmr->status & TILEM_USER_TIMER_NO_HALT_INT)
		|| !calc->z80.halted)) {
		calc->z80.interrupts |= (TILEM_INTERRUPT_USER_TIMER1 << n);
	}

	if (tmr->status & TILEM_USER_TIMER_LOOP) {
		/* timer will overflow the next time it expires
		   (unless user writes to the mode port before
		   then) */
		tilem_z80_set_timer_period(calc, TILEM_TIMER_USER1 + n,
					   get_overflow_duration(tmr));
	}
}
