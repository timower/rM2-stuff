/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009-2011 Benjamin Moody
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

static int certificate_valid(byte* cert)
{
	int i, n;

	if (cert[0] != 0)
		return 0;

	i = 1;

	/* check that the actual certificate area consists of valid
	   certificate fields */
	while (cert[i] <= 0x0F) {
		switch (cert[i + 1] & 0x0F) {
		case 0x0D:
			n = cert[i + 2] + 3;
			break;
		case 0x0E:
			n = (cert[i + 2] << 8) + cert[i + 3] + 4;
			break;
		case 0x0F:
			n = 6;
			break;
		default:
			n = (cert[i + 1] & 0xf) + 2;
		}
		i += n;
		if (i >= 0x2000)
			return 0;
	}

	/* check that the fields end with FF */
	if (cert[i] != 0xFF)
		return 0;

	/* if there are fields present, assume the certificate is OK */
	if (i > 1)
		return 1;

	/* no fields present -> this could be an incompletely-patched
	   certificate from an older version of TilEm; verify that the
	   next 4k bytes are truly empty */
	while (i < 0x1000) {
		if (cert[i] != 0xFF)
			return 0;
		i++;
	}

	return 1;
}

void tilem_calc_fix_certificate(TilemCalc* calc, byte* cert,
                                int app_start, int app_end,
                                unsigned exptab_offset)
{
	int i, base, max_apps, page;
	unsigned insttab_offset = 0x1fe0;

	/* If the ROM was dumped from an unpatched OS, the certificate
	   needs to be patched for some calculator functions to
	   work. */

	/* First, check if the certificate is already valid */

	if (cert[0x2000] == 0)
		base = 0x2000;
	else
		base = 0;

	if (certificate_valid(cert + base)) {
		return;
	}

	tilem_message(calc, "Repairing certificate area...");

	memset(cert, 0xff, 16384);

	cert[0] = 0;

	cert[insttab_offset] = 0xfe;

	if (app_start < app_end)
		max_apps = app_end - app_start + 1;
	else
		max_apps = app_start - app_end + 1;

	for (i = 0; i < max_apps; i++) {
		if (app_start < app_end)
			page = app_start + i;
		else
			page = app_start - i;

		/* Clear installed bit / set expiration count for
		   existing apps.  (If this incorrectly detects pages
		   that aren't really apps, don't worry about it;
		   better to err on the side of caution.) */
		if (calc->mem[page << 14] != 0x80
		    || calc->mem[(page << 14) + 1] != 0x0f)
			continue;

		tilem_message(calc, "Found application at page %02x (index %d)",
		              page, i);

		cert[insttab_offset + ((i + 1) / 8)] &= ~(1 << ((i + 1) % 8));

		cert[exptab_offset + 2 * i] = 0x80;
		cert[exptab_offset + 2 * i + 1] = 0x00;
	}
}
