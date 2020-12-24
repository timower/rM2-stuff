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
#include <string.h>
#include "tilem.h"
#include "z80.h"

extern const TilemHardware hardware_ti73, hardware_ti76,
	hardware_ti81, hardware_ti82, hardware_ti83,
	hardware_ti83p, hardware_ti83pse, hardware_ti84p,
	hardware_ti84pse, hardware_ti84pns,
	hardware_ti85, hardware_ti86;

const TilemHardware* hwmodels[] = {
	&hardware_ti73,
	&hardware_ti76,
	&hardware_ti81,
	&hardware_ti82,
	&hardware_ti83,
	&hardware_ti83p,
	&hardware_ti83pse,
	&hardware_ti84p,
	&hardware_ti84pse,
	&hardware_ti84pns,
	&hardware_ti85,
	&hardware_ti86 };

#define NUM_MODELS (sizeof(hwmodels) / sizeof(TilemHardware*))

void tilem_get_supported_hardware(const TilemHardware*** models,
				  int* nmodels)
{
	*models = hwmodels;
	*nmodels = NUM_MODELS;
}

void tilem_calc_reset(TilemCalc* calc)
{
	tilem_z80_reset(calc);
	tilem_lcd_reset(calc);
	tilem_linkport_reset(calc);
	tilem_keypad_reset(calc);
	tilem_flash_reset(calc);
	tilem_md5_assist_reset(calc);
	tilem_user_timers_reset(calc);
	if (calc->hw.reset)
		(*calc->hw.reset)(calc);
}

TilemCalc* tilem_calc_new(char id)
{
	int i;
	TilemCalc* calc;
	dword msize;

	for (i = 0; i < (int) NUM_MODELS; i++) {
		if (hwmodels[i]->model_id == id) {
			calc = tilem_try_new0(TilemCalc, 1);
			if (!calc) {
				return NULL;
			}

			calc->hw = *hwmodels[i];

			calc->poweronhalt = 1;
			calc->battery = 60;
			calc->hwregs = tilem_try_new_atomic(dword, calc->hw.nhwregs);
			if (!calc->hwregs) {
				tilem_free(calc);
				return NULL;
			}

			memset(calc->hwregs, 0, calc->hw.nhwregs * sizeof(dword));

			msize = (calc->hw.romsize + calc->hw.ramsize
				 + calc->hw.lcdmemsize);

			calc->mem = tilem_try_new_atomic(byte, msize);
			if (!calc->mem) {
				tilem_free(calc->hwregs);
				tilem_free(calc);
				return NULL;
			}

			calc->ram = calc->mem + calc->hw.romsize;
			calc->lcdmem = calc->ram + calc->hw.ramsize;

			memset(calc->ram, 0, msize - calc->hw.romsize);

			calc->lcd.emuflags = TILEM_LCD_REQUIRE_DELAY;
			calc->flash.emuflags = TILEM_FLASH_REQUIRE_DELAY;

			tilem_calc_reset(calc);
			return calc;
		}
	}

	fprintf(stderr, "INTERNAL ERROR: invalid model ID '%c'\n", id);
	return NULL;
}

TilemCalc* tilem_calc_copy(TilemCalc* calc)
{
	TilemCalc* newcalc;
	dword msize;

	newcalc = tilem_try_new(TilemCalc, 1);
	if (!newcalc)
		return NULL;
	memcpy(newcalc, calc, sizeof(TilemCalc));

	newcalc->hwregs = tilem_try_new_atomic(dword, calc->hw.nhwregs);
	if (!newcalc->hwregs) {
		tilem_free(newcalc);
		return NULL;
	}
	memcpy(newcalc->hwregs, calc->hwregs, calc->hw.nhwregs * sizeof(dword));

	newcalc->z80.timers = tilem_try_new(TilemZ80Timer,
					    newcalc->z80.ntimers);
	if (!newcalc->z80.timers) {
		tilem_free(newcalc->hwregs);
		tilem_free(newcalc);
		return NULL;
	}
	memcpy(newcalc->z80.timers, calc->z80.timers,
	       newcalc->z80.ntimers * sizeof(TilemZ80Timer));

	newcalc->z80.breakpoints = tilem_try_new(TilemZ80Breakpoint,
					     newcalc->z80.nbreakpoints);
	if (!newcalc->z80.breakpoints) {
		tilem_free(newcalc->z80.timers);
		tilem_free(newcalc->hwregs);
		tilem_free(newcalc);
		return NULL;
	}
	memcpy(newcalc->z80.breakpoints, calc->z80.breakpoints,
	       newcalc->z80.nbreakpoints * sizeof(TilemZ80Breakpoint));

	msize = (calc->hw.romsize + calc->hw.ramsize + calc->hw.lcdmemsize);
	newcalc->mem = tilem_try_new_atomic(byte, msize);
	if (!newcalc->mem) {
		tilem_free(newcalc->z80.breakpoints);
		tilem_free(newcalc->z80.timers);
		tilem_free(newcalc->hwregs);
		tilem_free(newcalc);
		return NULL;
	}
	memcpy(newcalc->mem, calc->mem, msize * sizeof(byte));

	newcalc->ram = newcalc->mem + calc->hw.romsize;
	newcalc->lcdmem = newcalc->ram + calc->hw.ramsize;

	return newcalc;
}

void tilem_calc_free(TilemCalc* calc)
{
	tilem_free(calc->mem);
	tilem_free(calc->hwregs);
	tilem_free(calc->z80.breakpoints);
	tilem_free(calc->z80.timers);
	tilem_free(calc);
}
