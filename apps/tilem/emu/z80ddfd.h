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

#ifdef PREFIX_DD
# define RegH IXh
# define RegL IXl
# define RegHL IX
#else
# define RegH IYh
# define RegL IYl
# define RegHL IY
#endif

switch (op) {
 case 0x09:			/* ADD IX, BC */
	 WZ = RegHL + 1;
	 add16(RegHL, BC);
	 delay(11);
	 break;
 case 0x19:			/* ADD IX, DE */
	 WZ = RegHL + 1;
	 add16(RegHL, DE);
	 delay(11);
	 break;
 case 0x21:			/* LD IX, nn */
	 RegHL = readw(PC);
	 PC += 2;
	 delay(10);
	 break;
 case 0x22:			/* LD (nn), IX */
	 WZ = readw(PC);
	 PC += 2;
	 writeb(WZ, RegL);
	 WZ++;
	 writeb(WZ, RegH);
	 delay(16);
	 break;
 case 0x23:			/* INC IX */
	 RegHL++;
	 delay(6);
	 break;
 case 0x24:			/* INC IXH */
	 UNDOCUMENTED(4);
	 inc(RegH);
	 delay(4);
	 break;
 case 0x25:			/* DEC IXH */
	 UNDOCUMENTED(4);
	 dec(RegH);
	 delay(4);
	 break;
 case 0x26:			/* LD IXH, n */
	 UNDOCUMENTED(4);
	 RegH = readb(PC++);
	 delay(7);
	 break;
 case 0x29:			/* ADD IX, IX */
	 WZ = RegHL + 1;
	 add16(RegHL, RegHL);
	 delay(11);
	 break;
 case 0x2A:			/* LD IX, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 RegL = readb(WZ);
	 WZ++;
	 RegH = readb(WZ);
	 delay(16);
	 break;
 case 0x2B:			/* DEC IX */
	 RegHL--;
	 delay(6);
	 break;
 case 0x2C:			/* INC IXL */
	 UNDOCUMENTED(4);
	 inc(RegL);
	 delay(4);
	 break;
 case 0x2D:			/* DEC IXL */
	 UNDOCUMENTED(4);
	 dec(RegL);
	 delay(4);
	 break;
 case 0x2E:			/* LD IXL,n */
	 UNDOCUMENTED(4);
	 RegL = readb(PC++);
	 delay(7);
	 break;

 case 0x34:			/* INC (IX + n) */
	 offs = (int) (signed char) readb(PC++);
	 WZ = RegHL + offs;
	 tmp1 = readb(WZ);
	 inc(tmp1);
	 writeb(WZ, tmp1);
	 delay(19);
	 break;
 case 0x35:			/* DEC (IX + n) */
	 offs = (int) (signed char) readb(PC++);
	 WZ = RegHL + offs;
	 tmp1 = readb(WZ);
	 dec(tmp1);
	 writeb(WZ, tmp1);
	 delay(19);
	 break;
 case 0x36:			/* LD (IX + n), n */
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, readb(PC++));
	 delay(15);		/* Yes, really! */
	 break;

 case 0x39:			/* ADD IX, SP */
	 WZ = RegHL + 1;
	 add16(RegHL, SP);
	 delay(11);
	 break;

 case 0x44: UNDOCUMENTED(4); B = RegH; delay(4); break;
 case 0x45: UNDOCUMENTED(4); B = RegL; delay(4); break;
 case 0x46:
	 offs = (int) (signed char) readb(PC++);
	 B = readb(RegHL + offs);
	 delay(15);
	 break;

 case 0x4C: UNDOCUMENTED(4); C = RegH; delay(4); break;
 case 0x4D: UNDOCUMENTED(4); C = RegL; delay(4); break;
 case 0x4E:
	 offs = (int) (signed char) readb(PC++);
	 C = readb(RegHL + offs);
	 delay(15);
	 break;

 case 0x54: UNDOCUMENTED(4); D = RegH; delay(4); break;
 case 0x55: UNDOCUMENTED(4); D = RegL; delay(4); break;
 case 0x56:
	 offs = (int) (signed char) readb(PC++);
	 D = readb(RegHL + offs);
	 delay(15);
	 break;

 case 0x5C: UNDOCUMENTED(4); E = RegH; delay(4); break;
 case 0x5D: UNDOCUMENTED(4); E = RegL; delay(4); break;
 case 0x5E:
	 offs = (int) (signed char) readb(PC++);
	 E = readb(RegHL + offs);
	 delay(15);
	 break;

 case 0x60: UNDOCUMENTED(4); RegH = B; delay(4); break;
 case 0x61: UNDOCUMENTED(4); RegH = C; delay(4); break;
 case 0x62: UNDOCUMENTED(4); RegH = D; delay(4); break;
 case 0x63: UNDOCUMENTED(4); RegH = E; delay(4); break;
 case 0x64: UNDOCUMENTED(4); RegH = RegH; delay(4); break;
 case 0x65: UNDOCUMENTED(4); RegH = RegL; delay(4); break;
 case 0x66:
	 offs = (int) (signed char) readb(PC++);
	 H = readb(RegHL + offs);
	 delay(15);
	 break;
 case 0x67: UNDOCUMENTED(4); RegH = A; delay(4); break;

 case 0x68: UNDOCUMENTED(4); RegL = B; delay(4); break;
 case 0x69: UNDOCUMENTED(4); RegL = C; delay(4); break;
 case 0x6A: UNDOCUMENTED(4); RegL = D; delay(4); break;
 case 0x6B: UNDOCUMENTED(4); RegL = E; delay(4); break;
 case 0x6C: UNDOCUMENTED(4); RegL = RegH; delay(4); break;
 case 0x6D: UNDOCUMENTED(4); RegL = RegL; delay(4); break;
 case 0x6E:
	 offs = (int) (signed char) readb(PC++);
	 L = readb(RegHL + offs);
	 delay(15);
	 break;
 case 0x6F: UNDOCUMENTED(4); RegL = A; delay(4); break;

 case 0x70:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, B);
	 delay(15);
	 break;
 case 0x71:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, C);
	 delay(15);
	 break;
 case 0x72:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, D);
	 delay(15);
	 break;
 case 0x73:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, E);
	 delay(15);
	 break;
 case 0x74:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, H);
	 delay(15);
	 break;
 case 0x75:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, L);
	 delay(15);
	 break;
 case 0x77:
	 offs = (int) (signed char) readb(PC++);
	 writeb(RegHL + offs, A);
	 delay(15);
	 break;

 case 0x7C: UNDOCUMENTED(4); A = RegH; delay(4); break;
 case 0x7D: UNDOCUMENTED(4); A = RegL; delay(4); break;
 case 0x7E:
	 offs = (int) (signed char) readb(PC++);
	 A = readb(RegHL + offs);
	 delay(15);
	 break;

 case 0x84: UNDOCUMENTED(4); add8(A, RegH); delay(4); break;
 case 0x85: UNDOCUMENTED(4); add8(A, RegL); delay(4); break;
 case 0x86:
	 offs = (int) (signed char) readb(PC++);
	 add8(A, readb(RegHL + offs));
	 delay(7);
	 break;

 case 0x8C: UNDOCUMENTED(4); adc8(A, RegH); delay(4); break;
 case 0x8D: UNDOCUMENTED(4); adc8(A, RegL); delay(4); break;
 case 0x8E:
	 offs = (int) (signed char) readb(PC++);
	 adc8(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0x94: UNDOCUMENTED(4); sub8(A, RegH); delay(4); break;
 case 0x95: UNDOCUMENTED(4); sub8(A, RegL); delay(4); break;
 case 0x96: 
	 offs = (int) (signed char) readb(PC++);
	 sub8(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0x9C: UNDOCUMENTED(4); sbc8(A, RegH); delay(4); break;
 case 0x9D: UNDOCUMENTED(4); sbc8(A, RegL); delay(4); break;
 case 0x9E: 
	 offs = (int) (signed char) readb(PC++);
	 sbc8(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0xA4: UNDOCUMENTED(4); and(A, RegH); delay(4); break;
 case 0xA5: UNDOCUMENTED(4); and(A, RegL); delay(4); break;
 case 0xA6: 
	 offs = (int) (signed char) readb(PC++);
	 and(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0xAC: UNDOCUMENTED(4); xor(A, RegH); delay(4); break;
 case 0xAD: UNDOCUMENTED(4); xor(A, RegL); delay(4); break;
 case 0xAE: 
	 offs = (int) (signed char) readb(PC++);
	 xor(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0xB4: UNDOCUMENTED(4); or(A, RegH); delay(4); break;
 case 0xB5: UNDOCUMENTED(4); or(A, RegL); delay(4); break;
 case 0xB6: 
	 offs = (int) (signed char) readb(PC++);
	 or(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0xBC: UNDOCUMENTED(4); cp(A, RegH); delay(4); break;
 case 0xBD: UNDOCUMENTED(4); cp(A, RegL); delay(4); break;
 case 0xBE: 
	 offs = (int) (signed char) readb(PC++);
	 cp(A, readb(RegHL + offs));
	 delay(15);
	 break;

 case 0xCB:
	 offs = (int) (signed char) readb(PC++);
	 WZ = RegHL + offs;
	 op = readb(PC++);
#ifdef PREFIX_DD
	 goto opcode_ddcb;
#else
	 goto opcode_fdcb;
#endif

 case 0xE1:			/* POP IX */
	 pop(RegHL);
	 delay(10);
	 break;
 case 0xE3:			/* EX (SP), IX */
	 WZ = readw(SP);
	 writew(SP, RegHL);
	 RegHL = WZ;
	 delay(19);
	 break;
 case 0xE5:			/* PUSH IX */
	 push(RegHL);
	 delay(11);
	 break;

 case 0xE9:			/* JP IX */
	 PC = RegHL;
	 delay(4);
	 break;

 case 0xF9:			/* LD SP, IX */
	 SP = RegHL;
	 delay(4);
	 break;

 default:
	 goto opcode_main;
 }

#undef RegH
#undef RegL
#undef RegHL

