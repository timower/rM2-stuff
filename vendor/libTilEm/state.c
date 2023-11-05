/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009-2012 Benjamin Moody
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
#include <stdlib.h>
#include <string.h>
#include "tilem.h"
#include "z80.h"

static void set_hw_reg(TilemCalc* calc, const char* name, dword value)
{
	int i;

	for (i = 0; i < calc->hw.nhwregs; i++) {
		if (!strcmp(name, calc->hw.hwregnames[i])) {
			calc->hwregs[i] = value;
			return;
		}
	}

	tilem_warning(calc, "Unknown hwreg %s", name);
}

static const char* get_timer_name(TilemCalc* calc, int id)
{
	if (id == TILEM_TIMER_LCD_DELAY)
		return "lcddelay";
	else if (id == TILEM_TIMER_FLASH_DELAY)
		return "flashdelay";
	else if (id == TILEM_TIMER_LINK_ASSIST)
		return "linkassist";
	else if (id == TILEM_TIMER_USER1)
		return "user1";
	else if (id == TILEM_TIMER_USER2)
		return "user2";
	else if (id == TILEM_TIMER_USER3)
		return "user3";
	else if (id <= TILEM_NUM_SYS_TIMERS)
		abort();

	id -= TILEM_NUM_SYS_TIMERS + 1;
	if (id < calc->hw.nhwtimers)
		return calc->hw.hwtimernames[id];
	else
		return NULL;
}

static void set_ptimer(TilemCalc* calc, const char* name, dword value,
		       dword period, int rt)
{
	int i;
	const char* tname;

	for (i = 1; i <= calc->z80.ntimers; i++) {
		tname = get_timer_name(calc, i);
		if (tname && !strcmp(name, tname)) {
			tilem_z80_set_timer(calc, i, value, period, rt);
			return;
		}
	}

	tilem_warning(calc, "Unknown timer %s", name);
}

static int load_old_sav_file(TilemCalc* calc, FILE* savfile)
{
	byte b[76];
	dword regs[19];
	int i, le, be, c;
	unsigned int pageA, pageB;

	/* Read memory mapping */

	if (fread(calc->mempagemap, 1, 4, savfile) < 4)
		return 1;

	/* Read CPU registers */

	if (fread(b, 1, 76, savfile) < 76)
		return 1;

	be = le = 0;

	/* determine if file is in big-endian or little-endian
	   format */

	for (i = 0; i < 19; i++) {
		if (b[i * 4] || b[i * 4 + 1])
			le++;
		if (b[i * 4 + 2] || b[i * 4 + 3])
			be++;
	}

	if (le > be) {
		for (i = 0; i < 19; i++) {
			regs[i] = b[i * 4] + (b[i * 4 + 1] << 8);
		}
	}
	else {
		for (i = 0; i < 19; i++) {
			regs[i] = b[i * 4 + 3] + (b[i * 4 + 2] << 8);
		}
	}

	calc->z80.r.af.d = regs[0];
	calc->z80.r.bc.d = regs[1];
	calc->z80.r.de.d = regs[2];
	calc->z80.r.hl.d = regs[3];
	calc->z80.r.ix.d = regs[4];
	calc->z80.r.iy.d = regs[5];
	calc->z80.r.pc.d = regs[6];
	calc->z80.r.sp.d = regs[7];
	calc->z80.r.af2.d = regs[8];
	calc->z80.r.bc2.d = regs[9];
	calc->z80.r.de2.d = regs[10];
	calc->z80.r.hl2.d = regs[11];
	calc->z80.r.iff1 = regs[12] ? 1 : 0;
	calc->z80.r.iff2 = regs[13] ? 1 : 0;
	calc->z80.r.im = regs[15];
	calc->z80.r.ir.b.h = regs[16];
	calc->z80.r.ir.b.l = regs[17];
	calc->z80.r.r7 = regs[18] & 0x80;

	if (calc->hw.model_id == '2' || calc->hw.model_id == '3') {
		if (fread(b, 1, 5, savfile) < 5)
			return 1;

		if (calc->hw.model_id == '3')
			set_hw_reg(calc, "rom_bank", calc->mempagemap[1] & 0x08);
		calc->hw.z80_out(calc, 0x02, b[4]);
	}

	/* Read RAM contents: old save files for TI-82/83/85 store RAM
	   pages in logical rather than physical order */

	if (calc->hw.model_id == '2' || calc->hw.model_id == '3'
	    || calc->hw.model_id == '5') {
		if (fread(calc->mem + calc->hw.romsize + 0x4000, 1,
			  0x4000, savfile) < 0x4000)
			return 1;
		if (fread(calc->mem + calc->hw.romsize, 1,
			  0x4000, savfile) < 0x4000)
			return 1;
	}
	else {
		if (fread(calc->mem + calc->hw.romsize, 1,
			  calc->hw.ramsize, savfile) < calc->hw.ramsize)
			return 1;
	}

	/* Read LCD contents */

	if (calc->hw.flags & TILEM_CALC_HAS_T6A04) {
		calc->lcd.rowstride = 12; /* old save files only
					     support the visible
					     portion of the screen */
		if (fread(calc->lcdmem, 1, 768, savfile) < 768)
			return 1;
	}

	/* Read additional HW state */

	switch (calc->hw.model_id) {
	case '1':
		break;

	case '2':
	case '3':
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.mode = c;
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.x = c;
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.y = c;
		break;

	case '5':
		pageA = calc->mempagemap[1];
		if (pageA >= 0x08)
			pageA += 0x38;
		calc->hw.z80_out(calc, 0x05, pageA);

		if ((c = fgetc(savfile)) != EOF)
			calc->hw.z80_out(calc, 0x06, c);
		break;

	case '6':
		pageA = calc->mempagemap[1];
		pageB = calc->mempagemap[2];
		if (pageA >= 0x10)
			pageA += 0x30;
		if (pageB >= 0x10)
			pageB += 0x30;

		calc->hw.z80_out(calc, 0x05, pageA);
		calc->hw.z80_out(calc, 0x06, pageB);
		break;

	default:		/* TI-73/83+ series */
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.mode = c;
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.x = c;
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.y = c;
		if ((c = fgetc(savfile)) != EOF)
			calc->lcd.inc = c;

		if ((c = fgetc(savfile)) == EOF)
			c = 0;
		if (c) {
			pageA = calc->mempagemap[2];
			pageB = calc->mempagemap[3];
			calc->hw.z80_out(calc, 0x04, 0x77);
		}
		else {
			pageA = calc->mempagemap[1];
			pageB = calc->mempagemap[2];
			calc->hw.z80_out(calc, 0x04, 0x76);
		}

		if (pageA >= (calc->hw.romsize >> 14))
			pageA = ((pageA & 0x1f) | calc->hw.rampagemask);
		if (pageB >= (calc->hw.romsize >> 14))
			pageB = ((pageB & 0x1f) | calc->hw.rampagemask);

		calc->hw.z80_out(calc, 0x06, pageA);
		calc->hw.z80_out(calc, 0x07, pageB);

		if ((c = fgetc(savfile)) != EOF)
			calc->flash.state = c;
		if ((c = fgetc(savfile)) != EOF)
			calc->flash.unlock = c;

		if ((c = fgetc(savfile)) != EOF)
			calc->hw.z80_out(calc, 0x20, c);
		if ((c = fgetc(savfile)) != EOF)
			set_hw_reg(calc, "port21", c);
		if ((c = fgetc(savfile)) != EOF)
			set_hw_reg(calc, "port22", c);
		if ((c = fgetc(savfile)) != EOF)
			set_hw_reg(calc, "port23", c);
		if ((c = fgetc(savfile)) != EOF)
			calc->hw.z80_out(calc, 0x27, c);
		if ((c = fgetc(savfile)) != EOF)
			calc->hw.z80_out(calc, 0x28, c);
		break;
	}

	calc->poweronhalt = calc->lcd.active = 1;

	return 0;
}

static int read_sav_line(FILE* savfile, char **buf)
{
	int c, n, na;

	tilem_free(*buf);

	na = 100;
	*buf = tilem_malloc_atomic(na);
	n = 0;

	while ((c = fgetc(savfile)) != EOF) {
		if (c == '\r' || c == '\n')
			break;

		n++;
		if (n >= na) {
			na = n * 2;
			*buf = tilem_realloc(*buf, na);
		}

		if (c == '#')
			c = 0;
		(*buf)[n - 1] = c;
	}

	if (n == 0 && c == EOF) {
		tilem_free(*buf);
		*buf = NULL;
		return 0;
	}
	else {
		(*buf)[n] = 0;
		return 1;
	}
}

static int parse_sav_definition(char* line, char** value)
{
	char *p;

	p = strchr(line, '=');
	if (!p)
		return 0;

	while (p != line && p[-1] == ' ')
		p--;
	*p = 0;
	p++;
	while (*p == ' ' || *p == '=')
		p++;
	*value = p;
	return 1;
}

static int load_new_sav_file(TilemCalc* calc, FILE* savfile)
{
	char *buf = NULL;
	char *p, *q;
	dword value, length;
	byte *data;
	int ok = 0;
	byte digit;
	int firstdigit;
	dword period;
	int rt;

	while (read_sav_line(savfile, &buf)) {
		if (!parse_sav_definition(buf, &p))
			continue;

		if (*p == '{') {
			p++;
			if (!strcmp(buf, "RAM")) {
				length = calc->hw.ramsize;
				data = calc->ram;
			}
			else if (!strcmp(buf, "LCD")) {
				length = calc->hw.lcdmemsize;
				data = calc->lcdmem;
			}
			else {
				length = 0;
				data = NULL;
			}

			value = 0;
			firstdigit = 1;

			while (*p != '}') {
				if (*p == 0 || *p == '#') {
					if (!read_sav_line(savfile, &buf))
						return 1;
					p = buf;
					continue;
				}

				if (*p >= '0' && *p <= '9') {
					digit = *p - '0';
					p++;
				}
				else if (*p >= 'A' && *p <= 'F') {
					digit = *p + 10 - 'A';
					p++;
				}
				else if (*p >= 'a' && *p <= 'f') {
					digit = *p + 10 - 'a';
					p++;
				}
				else {
					p++;
					continue;
				}

				if (firstdigit) {
					value = digit << 4;
					firstdigit = 0;
				}
				else {
					value |= digit;
					if (length != 0) {
						*data = value;
						data++;
						length--;
					}
					firstdigit = 1;
				}
			}

			continue;
		}

		if (!strcmp(buf, "MODEL")) {
			q = p;
			while (*q >= ' ')
				q++;
			*q = 0;
			if (strcmp(p, calc->hw.name)) {
				tilem_free(buf);
				return 1;
			}
			ok = 1;
			continue;
		}

		value = strtol(p, &q, 16);

		/* Persistent timers */
		if (!strncmp(buf, "timer:", 6)) {
			while (*q == ' ')
				q++;
			if (*q != ',')
				continue;
			q++;
			while (*q == ' ')
				q++;
			period = strtol(q, &q, 16);

			while (*q == ' ')
				q++;
			if (*q != ',')
				continue;
			q++;
			while (*q == ' ')
				q++;
			rt = strtol(q, &q, 16);

			set_ptimer(calc, buf + 6, value, period, rt);
			continue;
		}

		/* Z80 */
		if (!strcmp(buf, "af")) calc->z80.r.af.d = value;
		else if (!strcmp(buf, "bc")) calc->z80.r.bc.d = value;
		else if (!strcmp(buf, "de")) calc->z80.r.de.d = value;
		else if (!strcmp(buf, "hl")) calc->z80.r.hl.d = value;
		else if (!strcmp(buf, "af'")) calc->z80.r.af2.d = value;
		else if (!strcmp(buf, "bc'")) calc->z80.r.bc2.d = value;
		else if (!strcmp(buf, "de'")) calc->z80.r.de2.d = value;
		else if (!strcmp(buf, "hl'")) calc->z80.r.hl2.d = value;
		else if (!strcmp(buf, "ix")) calc->z80.r.ix.d = value;
		else if (!strcmp(buf, "iy")) calc->z80.r.iy.d = value;
		else if (!strcmp(buf, "pc")) calc->z80.r.pc.d = value;
		else if (!strcmp(buf, "sp")) calc->z80.r.sp.d = value;
		else if (!strcmp(buf, "ir")) {
			calc->z80.r.ir.d = value;
			calc->z80.r.r7 = value & 0x80;
		}
		else if (!strcmp(buf, "wz")) calc->z80.r.wz.d = value;
		else if (!strcmp(buf, "wz'")) calc->z80.r.wz2.d = value;
		else if (!strcmp(buf, "iff1")) calc->z80.r.iff1 = value;
		else if (!strcmp(buf, "iff2")) calc->z80.r.iff2 = value;
		else if (!strcmp(buf, "im")) calc->z80.r.im = value;
		else if (!strcmp(buf, "interrupts"))
			calc->z80.interrupts = value;
		else if (!strcmp(buf, "clockspeed"))
			calc->z80.clockspeed = value;
		else if (!strcmp(buf, "halted")) calc->z80.halted = value;

		/* LCD */
		else if (!strcmp(buf, "lcd.active"))
			calc->lcd.active = value;
		else if (!strcmp(buf, "lcd.addr"))
			calc->lcd.addr = value;
		else if (!strcmp(buf, "lcd.rowshift"))
			calc->lcd.rowshift = value;
		else if (!strcmp(buf, "lcd.contrast"))
			calc->lcd.contrast = value;
		else if (!strcmp(buf, "lcd.inc"))
			calc->lcd.inc = value;
		else if (!strcmp(buf, "lcd.mode"))
			calc->lcd.mode = value;
		else if (!strcmp(buf, "lcd.x"))
			calc->lcd.x = value;
		else if (!strcmp(buf, "lcd.y"))
			calc->lcd.y = value;
		else if (!strcmp(buf, "lcd.nextbyte"))
			calc->lcd.nextbyte = value;
		else if (!strcmp(buf, "lcd.rowstride"))
			calc->lcd.rowstride = value;
		else if (!strcmp(buf, "lcd.busy"))
			calc->lcd.busy = value;

		/* Link port */
		else if (!strcmp(buf, "linkport.lines"))
			calc->linkport.lines = value;
		else if (!strcmp(buf, "linkport.mode"))
			calc->linkport.mode = value;
		else if (!strcmp(buf, "linkport.assistflags"))
			calc->linkport.assistflags = value;
		else if (!strcmp(buf, "linkport.assistin"))
			calc->linkport.assistin = value;
		else if (!strcmp(buf, "linkport.assistinbits"))
			calc->linkport.assistinbits = value;
		else if (!strcmp(buf, "linkport.assistout"))
			calc->linkport.assistout = value;
		else if (!strcmp(buf, "linkport.assistoutbits"))
			calc->linkport.assistoutbits = value;
		else if (!strcmp(buf, "linkport.assistlastbyte"))
			calc->linkport.assistlastbyte = value;

		/* Keypad */
		else if (!strcmp(buf, "keypad.group"))
			calc->keypad.group = value;
		else if (!strcmp(buf, "keypad.onkeyint"))
			calc->keypad.onkeyint = value;

		/* MD5 assist */
		else if (!strcmp(buf, "md5assist.a"))
			calc->md5assist.regs[0] = value;
		else if (!strcmp(buf, "md5assist.b"))
			calc->md5assist.regs[1] = value;
		else if (!strcmp(buf, "md5assist.c"))
			calc->md5assist.regs[2] = value;
		else if (!strcmp(buf, "md5assist.d"))
			calc->md5assist.regs[3] = value;
		else if (!strcmp(buf, "md5assist.x"))
			calc->md5assist.regs[4] = value;
		else if (!strcmp(buf, "md5assist.t"))
			calc->md5assist.regs[5] = value;
		else if (!strcmp(buf, "md5assist.shift"))
			calc->md5assist.shift = value;
		else if (!strcmp(buf, "md5assist.mode"))
			calc->md5assist.mode = value;

		/* Programmable timers */
		else if (!strcmp(buf, "usertimer0.frequency"))
			calc->usertimers[0].frequency = value;
		else if (!strcmp(buf, "usertimer0.loopvalue"))
			calc->usertimers[0].loopvalue = value;
		else if (!strcmp(buf, "usertimer0.status"))
			calc->usertimers[0].status = value;
		else if (!strcmp(buf, "usertimer1.frequency"))
			calc->usertimers[1].frequency = value;
		else if (!strcmp(buf, "usertimer1.loopvalue"))
			calc->usertimers[1].loopvalue = value;
		else if (!strcmp(buf, "usertimer1.status"))
			calc->usertimers[1].status = value;
		else if (!strcmp(buf, "usertimer2.frequency"))
			calc->usertimers[2].frequency = value;
		else if (!strcmp(buf, "usertimer2.loopvalue"))
			calc->usertimers[2].loopvalue = value;
		else if (!strcmp(buf, "usertimer2.status"))
			calc->usertimers[2].status = value;

		/* Main power */
		else if (!strcmp(buf, "poweronhalt"))
			calc->poweronhalt = value;

		/* Battery */
		else if (!strcmp(buf, "battery"))
			calc->battery = value;

		/* Memory */
		else if (!strcmp(buf, "mempagemap0"))
			calc->mempagemap[0] = value;
		else if (!strcmp(buf, "mempagemap1"))
			calc->mempagemap[1] = value;
		else if (!strcmp(buf, "mempagemap2"))
			calc->mempagemap[2] = value;
		else if (!strcmp(buf, "mempagemap3"))
			calc->mempagemap[3] = value;
		else if (!strcmp(buf, "flash.unlock"))
			calc->flash.unlock = value;
		else if (!strcmp(buf, "flash.state"))
			calc->flash.state = value;
		else if (!strcmp(buf, "flash.busy"))
			calc->flash.busy = value;
		else if (!strcmp(buf, "flash.progaddr"))
			calc->flash.progaddr = value;
		else if (!strcmp(buf, "flash.progbyte"))
			calc->flash.progbyte = value;
		else if (!strcmp(buf, "flash.toggles"))
			calc->flash.toggles = value;
		else if (!strcmp(buf, "flash.overridegroup"))
			calc->flash.overridegroup = value;

		else
			set_hw_reg(calc, buf, value);
	}

	tilem_free(buf);

	return !ok;
}

int tilem_calc_load_state(TilemCalc* calc, FILE* romfile, FILE* savfile)
{
	int b;
	int savtype = 0;

	if (romfile) {
		if (fread(calc->mem, 1, calc->hw.romsize, romfile)
		    != calc->hw.romsize)
			return 1;
	}

	tilem_calc_reset(calc);

	if (savfile) {
		/* first byte of old save files is always zero */
		b = fgetc(savfile);
		fseek(savfile, 0L, SEEK_SET);

		if (b == 0) {
			if (load_old_sav_file(calc, savfile)) {
				tilem_calc_reset(calc);
				return 1;
			}
			else
				savtype = 1;
		}
		else {
			if (load_new_sav_file(calc, savfile)) {
				tilem_calc_reset(calc);
				return 1;
			}
			else
				savtype = 2;
		}
	}

	if (calc->hw.stateloaded)
		(*calc->hw.stateloaded)(calc, savtype);

	return 0;
}

char tilem_get_sav_type(FILE* savfile)
{
	int b;
	char *buf = NULL, *p, *q;
	const TilemHardware **models;
	int nmodels, i;
	char id = 0;

	tilem_get_supported_hardware(&models, &nmodels);

	/* first byte of old save files is always zero */
	b = fgetc(savfile);
	fseek(savfile, 0L, SEEK_SET);
	if (b == 0)
		return 0; /* old files give no way to detect model */

	while (read_sav_line(savfile, &buf)) {
		if (parse_sav_definition(buf, &p)
		    && !strcmp(buf, "MODEL")) {
			q = p;
			while (*q >= ' ')
				q++;
			*q = 0;

			for (i = 0; i < nmodels; i++)
				if (!strcmp(p, models[i]->name))
					id = models[i]->model_id;

			break;
		}
	}

	fseek(savfile, 0L, SEEK_SET);
	tilem_free(buf);
	return id;
}

int tilem_calc_save_state(TilemCalc* calc, FILE* romfile, FILE* savfile)
{
	dword i;
	dword t;
	int j;
	const char* tname;
	unsigned int rowstride;

	if (romfile) {
		if (fwrite(calc->mem, 1, calc->hw.romsize, romfile)
		    != calc->hw.romsize)
			return 1;
	}

	if (savfile) {
		fprintf(savfile, "# Tilem II State File\n# Version: %s\n",
			PACKAGE_VERSION);
		fprintf(savfile, "MODEL = %s\n", calc->hw.name);

		fprintf(savfile, "\n## CPU ##\n");
		fprintf(savfile, "af = %04X\n", calc->z80.r.af.w.l);
		fprintf(savfile, "bc = %04X\n", calc->z80.r.bc.w.l);
		fprintf(savfile, "de = %04X\n", calc->z80.r.de.w.l);
		fprintf(savfile, "hl = %04X\n", calc->z80.r.hl.w.l);
		fprintf(savfile, "af' = %04X\n", calc->z80.r.af2.w.l);
		fprintf(savfile, "bc' = %04X\n", calc->z80.r.bc2.w.l);
		fprintf(savfile, "de' = %04X\n", calc->z80.r.de2.w.l);
		fprintf(savfile, "hl' = %04X\n", calc->z80.r.hl2.w.l);
		fprintf(savfile, "ix = %04X\n", calc->z80.r.ix.w.l);
		fprintf(savfile, "iy = %04X\n", calc->z80.r.iy.w.l);
		fprintf(savfile, "pc = %04X\n", calc->z80.r.pc.w.l);
		fprintf(savfile, "sp = %04X\n", calc->z80.r.sp.w.l);
		fprintf(savfile, "ir = %04X\n",
			((calc->z80.r.ir.w.l & ~0x80) | calc->z80.r.r7));
		fprintf(savfile, "wz = %04X\n", calc->z80.r.wz.w.l);
		fprintf(savfile, "wz' = %04X\n", calc->z80.r.wz2.w.l);
		fprintf(savfile, "iff1 = %X\n", calc->z80.r.iff1);
		fprintf(savfile, "iff2 = %X\n", calc->z80.r.iff2);
		fprintf(savfile, "im = %X\n", calc->z80.r.im);
		fprintf(savfile, "interrupts = %08X\n", calc->z80.interrupts);
		fprintf(savfile, "clockspeed = %X\n", calc->z80.clockspeed);
		fprintf(savfile, "halted = %X\n", calc->z80.halted);

		fprintf(savfile, "\n## LCD Driver ##\n");
		fprintf(savfile, "lcd.active = %X\n",
			calc->lcd.active);
		fprintf(savfile, "lcd.contrast = %X\n",
			calc->lcd.contrast);
		fprintf(savfile, "lcd.rowstride = %X\n",
			calc->lcd.rowstride);
		if (calc->hw.flags & TILEM_CALC_HAS_T6A04) {
			fprintf(savfile, "lcd.rowshift = %X\n",
				calc->lcd.rowshift);
			fprintf(savfile, "lcd.inc = %X\n",
				calc->lcd.inc);
			fprintf(savfile, "lcd.mode = %X\n",
				calc->lcd.mode);
			fprintf(savfile, "lcd.x = %02X\n",
				calc->lcd.x);
			fprintf(savfile, "lcd.y = %02X\n",
				calc->lcd.y);
			fprintf(savfile, "lcd.nextbyte = %02X\n",
				calc->lcd.nextbyte);
			fprintf(savfile, "lcd.busy = %X\n",
				calc->lcd.busy);
		}
		fprintf(savfile, "lcd.addr = %X\n", calc->lcd.addr);

		if (calc->hw.flags & TILEM_CALC_HAS_LINK) {
			fprintf(savfile, "\n## Link Port ##\n");
			fprintf(savfile, "linkport.lines = %X\n",
				calc->linkport.lines);
			fprintf(savfile, "linkport.mode = %08X\n",
				calc->linkport.mode);
		}
		if (calc->hw.flags & TILEM_CALC_HAS_LINK_ASSIST) {
			fprintf(savfile, "linkport.assistflags = %08X\n",
				calc->linkport.assistflags);
			fprintf(savfile, "linkport.assistin = %02X\n",
				calc->linkport.assistin);
			fprintf(savfile, "linkport.assistinbits = %X\n",
				calc->linkport.assistinbits);
			fprintf(savfile, "linkport.assistout = %02X\n",
				calc->linkport.assistout);
			fprintf(savfile, "linkport.assistoutbits = %X\n",
				calc->linkport.assistoutbits);
			fprintf(savfile, "linkport.assistlastbyte = %02X\n",
				calc->linkport.assistlastbyte);
		}

		fprintf(savfile, "\n## Keypad ##\n");
		fprintf(savfile, "keypad.group = %X\n", calc->keypad.group);
		fprintf(savfile, "keypad.onkeyint = %X\n",
			calc->keypad.onkeyint);

		fprintf(savfile, "\n## Memory mapping ##\n");
		fprintf(savfile, "mempagemap0 = %X\n", calc->mempagemap[0]);
		fprintf(savfile, "mempagemap1 = %X\n", calc->mempagemap[1]);
		fprintf(savfile, "mempagemap2 = %X\n", calc->mempagemap[2]);
		fprintf(savfile, "mempagemap3 = %X\n", calc->mempagemap[3]);

		fprintf(savfile, "\n## Power ##\n");
		fprintf(savfile, "poweronhalt = %X\n", calc->poweronhalt);
		fprintf(savfile, "battery = %X\n", calc->battery);

		if (calc->hw.flags & TILEM_CALC_HAS_FLASH) {
			fprintf(savfile, "\n## Flash ##\n");
			fprintf(savfile, "flash.unlock = %X\n",
				calc->flash.unlock);
			fprintf(savfile, "flash.state = %X\n",
				calc->flash.state);
			fprintf(savfile, "flash.busy = %X\n",
				calc->flash.busy);
			fprintf(savfile, "flash.progaddr = %X\n",
				calc->flash.progaddr);
			fprintf(savfile, "flash.progbyte = %X\n",
				calc->flash.progbyte);
			fprintf(savfile, "flash.toggles = %X\n",
				calc->flash.toggles);
			fprintf(savfile, "flash.overridegroup = %X\n",
				calc->flash.overridegroup);
		}

		if (calc->hw.flags & TILEM_CALC_HAS_MD5_ASSIST) {
			fprintf(savfile, "\n## MD5 assist ##\n");
			fprintf(savfile, "md5assist.a = %X\n",
				calc->md5assist.regs[0]);
			fprintf(savfile, "md5assist.b = %X\n",
				calc->md5assist.regs[1]);
			fprintf(savfile, "md5assist.c = %X\n",
				calc->md5assist.regs[2]);
			fprintf(savfile, "md5assist.d = %X\n",
				calc->md5assist.regs[3]);
			fprintf(savfile, "md5assist.x = %X\n",
				calc->md5assist.regs[4]);
			fprintf(savfile, "md5assist.t = %X\n",
				calc->md5assist.regs[5]);
			fprintf(savfile, "md5assist.shift = %X\n",
				calc->md5assist.shift);
			fprintf(savfile, "md5assist.mode = %X\n",
				calc->md5assist.mode);
		}

		for (j = 0; j < calc->hw.nusertimers; j++) {
			fprintf(savfile,
				"\n## Programmable timer %d ##\n", j);
			fprintf(savfile, "usertimer%d.frequency = %X\n",
				j, calc->usertimers[j].frequency);
			fprintf(savfile, "usertimer%d.loopvalue = %X\n",
				j, calc->usertimers[j].loopvalue);
			fprintf(savfile, "usertimer%d.status = %X\n",
				j, calc->usertimers[j].status);
		}

		fprintf(savfile, "\n## Model-specific ##\n");
		for (j = 0; j < calc->hw.nhwregs; j++) {
			fprintf(savfile, "%s = %X\n", calc->hw.hwregnames[j],
				calc->hwregs[j]);
		}

		fprintf(savfile, "\n## Timers ##\n");
		for (j = calc->z80.timer_cpu; j;
		     j = calc->z80.timers[j].next) {
			tname = get_timer_name(calc, j);
			if (tname) {
				t = tilem_z80_get_timer_clocks(calc, j);
				fprintf(savfile, "timer:%s = %X, %X, 0\n",
					tname, t, calc->z80.timers[j].period);
			}
		}
		for (j = calc->z80.timer_rt; j;
		     j = calc->z80.timers[j].next) {
			tname = get_timer_name(calc, j);
			if (tname) {
				t = tilem_z80_get_timer_microseconds(calc, j);
				fprintf(savfile, "timer:%s = %X, %X, 1\n",
					tname, t, calc->z80.timers[j].period);
			}
		}

		fprintf(savfile, "\n## RAM contents ##\n");
		fprintf(savfile, "RAM = {\n");
		for (i = 0; i < calc->hw.ramsize; i++) {
			if (i % 256 == 0) {
				fprintf(savfile, "# %02X:%04X\n",
					(i >> 14), (i & 0x3fff));
			}

			fprintf(savfile, "%02X",
				calc->mem[i + calc->hw.romsize]);
			if (i % 32 == 31)
				fprintf(savfile, "\n");
		}
		fprintf(savfile, "}\n## End of RAM contents ##\n");

		if (calc->hw.lcdmemsize) {
			fprintf(savfile, "\n## LCD contents ##\n");
			fprintf(savfile, "LCD = {\n");
			rowstride = calc->lcd.rowstride;
			if (rowstride == 0)
				rowstride = 32;

			for (i = 0; i < calc->hw.lcdmemsize; i++) {
				fprintf(savfile, "%02X", calc->lcdmem[i]);
				if (i % rowstride == (rowstride - 1))
					fprintf(savfile, "\n");
			}
			fprintf(savfile, "}\n## End of LCD contents ##\n");
		}
	}

	return 0;
}
