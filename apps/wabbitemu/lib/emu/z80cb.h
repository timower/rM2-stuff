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

#if defined(PREFIX_DD) || defined(PREFIX_FD)

# define CBINST(fnc, reg) do {			\
		UNDOCUMENTED(16);		\
		tmp1 = readb(WZ);		\
		fnc(tmp1);			\
		writeb(WZ, tmp1);		\
		(reg) = tmp1;			\
		delay(19);			\
	} while (0)

# define CBINST_HL(fnc) do {			\
		tmp1 = readb(WZ);		\
		fnc(tmp1);			\
		writeb(WZ, tmp1);		\
		delay(19);			\
	} while (0)

# define CBINST_UNDOC(fnc, reg) CBINST(fnc, reg)

# define CBINST_UNDOC_HL(fnc) do {		\
		UNDOCUMENTED(12);		\
		CBINST_HL(fnc);			\
	} while (0)

# define CB_BIT(b, reg) do {			\
		UNDOCUMENTED(12);		\
		CB_BIT_HL(b);			\
	} while (0)

# define CB_BIT_HL(b) do {					\
		tmp1 = readb(WZ) & (1 << b);			\
		F = ((tmp1 & FLAG_S)	     /* S */		\
		     | (W & FLAG_XY)	     /* X/Y */		\
		     | (tmp1 ? 0 : FLAG_ZP)  /* Z/P */		\
		     | (FLAG_H)		     /* H */		\
		     | (F & FLAG_C));				\
		delay(16);					\
	} while (0)

# define CB_RES(b, reg) do {			\
		UNDOCUMENTED(16);		\
		tmp1 = readb(WZ) & ~(1 << b);	\
		writeb(WZ, tmp1);		\
		(reg) = tmp1;			\
		delay(19);			\
	} while (0)

# define CB_RES_HL(b) do {			\
		tmp1 = readb(WZ) & ~(1 << b);	\
		writeb(WZ, tmp1);		\
		delay(19);			\
	} while (0)

# define CB_SET(b, reg) do {			\
		UNDOCUMENTED(16);		\
		tmp1 = readb(WZ) | (1 << b);	\
		writeb(WZ, tmp1);		\
		(reg) = tmp1;			\
		delay(19);			\
	} while (0)

# define CB_SET_HL(b) do {			\
		tmp1 = readb(WZ) | (1 << b);	\
		writeb(WZ, tmp1);		\
		delay(19);			\
	} while (0)

#else  /* ! PREFIX_DD, ! PREFIX_FD */

# define CBINST(fnc, reg) do {  \
		fnc(reg);	\
		delay(8);	\
	} while (0)

# define CBINST_HL(fnc) do {			\
		tmp1 = readb(HL);		\
		fnc(tmp1);			\
		writeb(HL, tmp1);		\
		delay(15);			\
	} while (0)

# define CBINST_UNDOC(fnc, reg) do {		\
		UNDOCUMENTED(8);		\
		CBINST(fnc, reg);		\
	} while (0)

# define CBINST_UNDOC_HL(fnc) do {		\
		UNDOCUMENTED(8);		\
		CBINST_HL(fnc);			\
	} while (0)

# define CB_BIT(b, reg) do {					\
		tmp1 = (reg) & (1 << b);			\
		F = ((tmp1 & FLAG_SXY)         /* S/X/Y */	\
		     | (tmp1 ? 0 : FLAG_ZP)    /* Z/P */	\
		     | (FLAG_H)		       /* H */		\
		     | (F & FLAG_C));				\
		delay(8);					\
	} while (0)

# define CB_BIT_HL(b) do {					\
		tmp1 = readb(HL) & (1 << b);			\
		F = ((tmp1 & FLAG_S)	       /* S */		\
		     | (W & FLAG_XY)	       /* X/Y */	\
		     | (tmp1 ? 0 : FLAG_ZP)    /* Z/P */	\
		     | (FLAG_H)		       /* H */		\
		     | (F & FLAG_C));				\
		delay(12);					\
	} while (0)

# define CB_RES(b, reg) do {			\
		(reg) &= ~(1 << b);		\
		delay(8);			\
	} while (0)

# define CB_RES_HL(b) do {				\
		writeb(HL, readb(HL) & ~(1 << b));	\
		delay(15);				\
	} while (0)

# define CB_SET(b, reg) do {			\
		(reg) |= (1 << b);		\
		delay(8);			\
	} while (0)

# define CB_SET_HL(b) do {				\
		writeb(HL, readb(HL) | (1 << b));	\
		delay(15);				\
	} while (0)

#endif

switch (op) {
 case 0x00: CBINST(rlc, B); break;
 case 0x01: CBINST(rlc, C); break;
 case 0x02: CBINST(rlc, D); break;
 case 0x03: CBINST(rlc, E); break;
 case 0x04: CBINST(rlc, H); break;
 case 0x05: CBINST(rlc, L); break;
 case 0x06: CBINST_HL(rlc); break;
 case 0x07: CBINST(rlc, A); break;
 case 0x08: CBINST(rrc, B); break;
 case 0x09: CBINST(rrc, C); break;
 case 0x0A: CBINST(rrc, D); break;
 case 0x0B: CBINST(rrc, E); break;
 case 0x0C: CBINST(rrc, H); break;
 case 0x0D: CBINST(rrc, L); break;
 case 0x0E: CBINST_HL(rrc); break;
 case 0x0F: CBINST(rrc, A); break;
 case 0x10: CBINST(rl, B); break;
 case 0x11: CBINST(rl, C); break;
 case 0x12: CBINST(rl, D); break;
 case 0x13: CBINST(rl, E); break;
 case 0x14: CBINST(rl, H); break;
 case 0x15: CBINST(rl, L); break;
 case 0x16: CBINST_HL(rl); break;
 case 0x17: CBINST(rl, A); break;
 case 0x18: CBINST(rr, B); break;
 case 0x19: CBINST(rr, C); break;
 case 0x1A: CBINST(rr, D); break;
 case 0x1B: CBINST(rr, E); break;
 case 0x1C: CBINST(rr, H); break;
 case 0x1D: CBINST(rr, L); break;
 case 0x1E: CBINST_HL(rr); break;
 case 0x1F: CBINST(rr, A); break;
 case 0x20: CBINST(sla, B); break;
 case 0x21: CBINST(sla, C); break;
 case 0x22: CBINST(sla, D); break;
 case 0x23: CBINST(sla, E); break;
 case 0x24: CBINST(sla, H); break;
 case 0x25: CBINST(sla, L); break;
 case 0x26: CBINST_HL(sla); break;
 case 0x27: CBINST(sla, A); break;
 case 0x28: CBINST(sra, B); break;
 case 0x29: CBINST(sra, C); break;
 case 0x2A: CBINST(sra, D); break;
 case 0x2B: CBINST(sra, E); break;
 case 0x2C: CBINST(sra, H); break;
 case 0x2D: CBINST(sra, L); break;
 case 0x2E: CBINST_HL(sra); break;
 case 0x2F: CBINST(sra, A); break;
 case 0x30: CBINST_UNDOC(slia, B); break;
 case 0x31: CBINST_UNDOC(slia, C); break;
 case 0x32: CBINST_UNDOC(slia, D); break;
 case 0x33: CBINST_UNDOC(slia, E); break;
 case 0x34: CBINST_UNDOC(slia, H); break;
 case 0x35: CBINST_UNDOC(slia, L); break;
 case 0x36: CBINST_UNDOC_HL(slia); break;
 case 0x37: CBINST_UNDOC(slia, A); break;
 case 0x38: CBINST(srl, B); break;
 case 0x39: CBINST(srl, C); break;
 case 0x3A: CBINST(srl, D); break;
 case 0x3B: CBINST(srl, E); break;
 case 0x3C: CBINST(srl, H); break;
 case 0x3D: CBINST(srl, L); break;
 case 0x3E: CBINST_HL(srl); break;
 case 0x3F: CBINST(srl, A); break;

 case 0x40: CB_BIT(0, B); break;
 case 0x41: CB_BIT(0, C); break;
 case 0x42: CB_BIT(0, D); break;
 case 0x43: CB_BIT(0, E); break;
 case 0x44: CB_BIT(0, H); break;
 case 0x45: CB_BIT(0, L); break;
 case 0x46: CB_BIT_HL(0); break;
 case 0x47: CB_BIT(0, A); break;
 case 0x48: CB_BIT(1, B); break;
 case 0x49: CB_BIT(1, C); break;
 case 0x4A: CB_BIT(1, D); break;
 case 0x4B: CB_BIT(1, E); break;
 case 0x4C: CB_BIT(1, H); break;
 case 0x4D: CB_BIT(1, L); break;
 case 0x4E: CB_BIT_HL(1); break;
 case 0x4F: CB_BIT(1, A); break;
 case 0x50: CB_BIT(2, B); break;
 case 0x51: CB_BIT(2, C); break;
 case 0x52: CB_BIT(2, D); break;
 case 0x53: CB_BIT(2, E); break;
 case 0x54: CB_BIT(2, H); break;
 case 0x55: CB_BIT(2, L); break;
 case 0x56: CB_BIT_HL(2); break;
 case 0x57: CB_BIT(2, A); break;
 case 0x58: CB_BIT(3, B); break;
 case 0x59: CB_BIT(3, C); break;
 case 0x5A: CB_BIT(3, D); break;
 case 0x5B: CB_BIT(3, E); break;
 case 0x5C: CB_BIT(3, H); break;
 case 0x5D: CB_BIT(3, L); break;
 case 0x5E: CB_BIT_HL(3); break;
 case 0x5F: CB_BIT(3, A); break;
 case 0x60: CB_BIT(4, B); break;
 case 0x61: CB_BIT(4, C); break;
 case 0x62: CB_BIT(4, D); break;
 case 0x63: CB_BIT(4, E); break;
 case 0x64: CB_BIT(4, H); break;
 case 0x65: CB_BIT(4, L); break;
 case 0x66: CB_BIT_HL(4); break;
 case 0x67: CB_BIT(4, A); break;
 case 0x68: CB_BIT(5, B); break;
 case 0x69: CB_BIT(5, C); break;
 case 0x6A: CB_BIT(5, D); break;
 case 0x6B: CB_BIT(5, E); break;
 case 0x6C: CB_BIT(5, H); break;
 case 0x6D: CB_BIT(5, L); break;
 case 0x6E: CB_BIT_HL(5); break;
 case 0x6F: CB_BIT(5, A); break;
 case 0x70: CB_BIT(6, B); break;
 case 0x71: CB_BIT(6, C); break;
 case 0x72: CB_BIT(6, D); break;
 case 0x73: CB_BIT(6, E); break;
 case 0x74: CB_BIT(6, H); break;
 case 0x75: CB_BIT(6, L); break;
 case 0x76: CB_BIT_HL(6); break;
 case 0x77: CB_BIT(6, A); break;
 case 0x78: CB_BIT(7, B); break;
 case 0x79: CB_BIT(7, C); break;
 case 0x7A: CB_BIT(7, D); break;
 case 0x7B: CB_BIT(7, E); break;
 case 0x7C: CB_BIT(7, H); break;
 case 0x7D: CB_BIT(7, L); break;
 case 0x7E: CB_BIT_HL(7); break;
 case 0x7F: CB_BIT(7, A); break;

 case 0x80: CB_RES(0, B); break;
 case 0x81: CB_RES(0, C); break;
 case 0x82: CB_RES(0, D); break;
 case 0x83: CB_RES(0, E); break;
 case 0x84: CB_RES(0, H); break;
 case 0x85: CB_RES(0, L); break;
 case 0x86: CB_RES_HL(0); break;
 case 0x87: CB_RES(0, A); break;
 case 0x88: CB_RES(1, B); break;
 case 0x89: CB_RES(1, C); break;
 case 0x8A: CB_RES(1, D); break;
 case 0x8B: CB_RES(1, E); break;
 case 0x8C: CB_RES(1, H); break;
 case 0x8D: CB_RES(1, L); break;
 case 0x8E: CB_RES_HL(1); break;
 case 0x8F: CB_RES(1, A); break;
 case 0x90: CB_RES(2, B); break;
 case 0x91: CB_RES(2, C); break;
 case 0x92: CB_RES(2, D); break;
 case 0x93: CB_RES(2, E); break;
 case 0x94: CB_RES(2, H); break;
 case 0x95: CB_RES(2, L); break;
 case 0x96: CB_RES_HL(2); break;
 case 0x97: CB_RES(2, A); break;
 case 0x98: CB_RES(3, B); break;
 case 0x99: CB_RES(3, C); break;
 case 0x9A: CB_RES(3, D); break;
 case 0x9B: CB_RES(3, E); break;
 case 0x9C: CB_RES(3, H); break;
 case 0x9D: CB_RES(3, L); break;
 case 0x9E: CB_RES_HL(3); break;
 case 0x9F: CB_RES(3, A); break;
 case 0xA0: CB_RES(4, B); break;
 case 0xA1: CB_RES(4, C); break;
 case 0xA2: CB_RES(4, D); break;
 case 0xA3: CB_RES(4, E); break;
 case 0xA4: CB_RES(4, H); break;
 case 0xA5: CB_RES(4, L); break;
 case 0xA6: CB_RES_HL(4); break;
 case 0xA7: CB_RES(4, A); break;
 case 0xA8: CB_RES(5, B); break;
 case 0xA9: CB_RES(5, C); break;
 case 0xAA: CB_RES(5, D); break;
 case 0xAB: CB_RES(5, E); break;
 case 0xAC: CB_RES(5, H); break;
 case 0xAD: CB_RES(5, L); break;
 case 0xAE: CB_RES_HL(5); break;
 case 0xAF: CB_RES(5, A); break;
 case 0xB0: CB_RES(6, B); break;
 case 0xB1: CB_RES(6, C); break;
 case 0xB2: CB_RES(6, D); break;
 case 0xB3: CB_RES(6, E); break;
 case 0xB4: CB_RES(6, H); break;
 case 0xB5: CB_RES(6, L); break;
 case 0xB6: CB_RES_HL(6); break;
 case 0xB7: CB_RES(6, A); break;
 case 0xB8: CB_RES(7, B); break;
 case 0xB9: CB_RES(7, C); break;
 case 0xBA: CB_RES(7, D); break;
 case 0xBB: CB_RES(7, E); break;
 case 0xBC: CB_RES(7, H); break;
 case 0xBD: CB_RES(7, L); break;
 case 0xBE: CB_RES_HL(7); break;
 case 0xBF: CB_RES(7, A); break;

 case 0xC0: CB_SET(0, B); break;
 case 0xC1: CB_SET(0, C); break;
 case 0xC2: CB_SET(0, D); break;
 case 0xC3: CB_SET(0, E); break;
 case 0xC4: CB_SET(0, H); break;
 case 0xC5: CB_SET(0, L); break;
 case 0xC6: CB_SET_HL(0); break;
 case 0xC7: CB_SET(0, A); break;
 case 0xC8: CB_SET(1, B); break;
 case 0xC9: CB_SET(1, C); break;
 case 0xCA: CB_SET(1, D); break;
 case 0xCB: CB_SET(1, E); break;
 case 0xCC: CB_SET(1, H); break;
 case 0xCD: CB_SET(1, L); break;
 case 0xCE: CB_SET_HL(1); break;
 case 0xCF: CB_SET(1, A); break;
 case 0xD0: CB_SET(2, B); break;
 case 0xD1: CB_SET(2, C); break;
 case 0xD2: CB_SET(2, D); break;
 case 0xD3: CB_SET(2, E); break;
 case 0xD4: CB_SET(2, H); break;
 case 0xD5: CB_SET(2, L); break;
 case 0xD6: CB_SET_HL(2); break;
 case 0xD7: CB_SET(2, A); break;
 case 0xD8: CB_SET(3, B); break;
 case 0xD9: CB_SET(3, C); break;
 case 0xDA: CB_SET(3, D); break;
 case 0xDB: CB_SET(3, E); break;
 case 0xDC: CB_SET(3, H); break;
 case 0xDD: CB_SET(3, L); break;
 case 0xDE: CB_SET_HL(3); break;
 case 0xDF: CB_SET(3, A); break;
 case 0xE0: CB_SET(4, B); break;
 case 0xE1: CB_SET(4, C); break;
 case 0xE2: CB_SET(4, D); break;
 case 0xE3: CB_SET(4, E); break;
 case 0xE4: CB_SET(4, H); break;
 case 0xE5: CB_SET(4, L); break;
 case 0xE6: CB_SET_HL(4); break;
 case 0xE7: CB_SET(4, A); break;
 case 0xE8: CB_SET(5, B); break;
 case 0xE9: CB_SET(5, C); break;
 case 0xEA: CB_SET(5, D); break;
 case 0xEB: CB_SET(5, E); break;
 case 0xEC: CB_SET(5, H); break;
 case 0xED: CB_SET(5, L); break;
 case 0xEE: CB_SET_HL(5); break;
 case 0xEF: CB_SET(5, A); break;
 case 0xF0: CB_SET(6, B); break;
 case 0xF1: CB_SET(6, C); break;
 case 0xF2: CB_SET(6, D); break;
 case 0xF3: CB_SET(6, E); break;
 case 0xF4: CB_SET(6, H); break;
 case 0xF5: CB_SET(6, L); break;
 case 0xF6: CB_SET_HL(6); break;
 case 0xF7: CB_SET(6, A); break;
 case 0xF8: CB_SET(7, B); break;
 case 0xF9: CB_SET(7, C); break;
 case 0xFA: CB_SET(7, D); break;
 case 0xFB: CB_SET(7, E); break;
 case 0xFC: CB_SET(7, H); break;
 case 0xFD: CB_SET(7, L); break;
 case 0xFE: CB_SET_HL(7); break;
 case 0xFF: CB_SET(7, A); break;
 }

#undef CBINST
#undef CBINST_HL
#undef CBINST_UNDOC
#undef CBINST_UNDOC_HL
#undef CB_BIT
#undef CB_BIT_HL
#undef CB_RES
#undef CB_RES_HL
#undef CB_SET
#undef CB_SET_HL

