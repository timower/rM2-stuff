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

switch (op) {
 case 0x00:			/* NOP */
	 delay(4);
	 break;
 case 0x01:			/* LD BC, nn */
	 BC = readw(PC);
	 PC += 2;
	 delay(10);
	 break;
 case 0x02:			/* LD (BC), A */
	 W = A;
	 writeb(BC, A);
	 delay(7);
	 break;
 case 0x03:			/* INC BC */
	 BC++;
	 delay(6);
	 break;
 case 0x04:			/* INC B */
	 inc(B);
	 delay(4);
	 break;
 case 0x05:			/* DEC B */
	 dec(B);
	 delay(4);
	 break;
 case 0x06:			/* LD B, n */
	 B = readb(PC++);
	 delay(7);
	 break;
 case 0x07:			/* RLCA */
	 rlca;
	 delay(4);
	 break;

 case 0x08:			/* EX AF, AF' */
	 ex(AF, AF2);
	 delay(4);
	 break;
 case 0x09:			/* ADD HL, BC */
	 WZ = HL + 1;
	 add16(HLw, BCw);
	 delay(11);
	 break;
 case 0x0A:			/* LD A, (BC) */
	 WZ = BC;
	 A = readb(WZ++);
	 delay(7);
	 break;
 case 0x0B:			/* DEC BC */
	 BC--;
	 delay(6);
	 break;
 case 0x0C:			/* INC C */
	 inc(C);
	 delay(4);
	 break;
 case 0x0D:			/* DEC C */
	 dec(C);
	 delay(4);
	 break;
 case 0x0E:			/* LD C, n */
	 C = readb(PC++);
	 delay(7);
	 break;
 case 0x0F:			/* RRCA */
	 rrca;
	 delay(4);
	 break;

 case 0x10:			/* DJNZ $+n */
	 offs = (int) (signed char) readb(PC++);
	 B--;
	 if (B) {
		 WZ = PC + offs;
		 PC = WZ;
		 delay(13);
	 }
	 else
		 delay(8);
	 break;
 case 0x11:			/* LD DE, nn */
	 DE = readw(PC);
	 PC += 2;
	 delay(10);
	 break;
 case 0x12:			/* LD (DE), A */
	 W = A;
	 writeb(DE, A);
	 delay(7);
	 break;
 case 0x13:			/* INC DE */
	 DE++;
	 delay(6);
	 break;
 case 0x14:			/* INC D */
	 inc(D);
	 delay(4);
	 break;
 case 0x15:			/* DEC D */
	 dec(D);
	 delay(4);
	 break;
 case 0x16:			/* LD D, n */
	 D = readb(PC++);
	 delay(7);
	 break;
 case 0x17:			/* RLA */
	 rla;
	 delay(4);
	 break;

 case 0x18:			/* JR $+n */
	 offs = (int) (signed char) readb(PC++);
	 WZ = PC + offs;
	 PC = WZ;
	 delay(12);
	 break;
 case 0x19:			/* ADD HL, DE */
	 WZ = HL + 1;
	 add16(HLw, DEw);
	 delay(11);
	 break;
 case 0x1A:			/* LD A, (DE) */
	 WZ = DE;
	 A = readb(WZ++);
	 delay(7);
	 break;
 case 0x1B:			/* DEC DE */
	 DE--;
	 delay(6);
	 break;
 case 0x1C:			/* INC E */
	 inc(E);
	 delay(4);
	 break;
 case 0x1D:			/* DEC E */
	 dec(E);
	 delay(4);
	 break;
 case 0x1E:			/* LD E, n */
	 E = readb(PC++);
	 delay(7);
	 break;
 case 0x1F:			/* RRA */
	 rra;
	 delay(4);
	 break;

 case 0x20:			/* JR NZ, $+n */
	 offs = (int) (signed char) readb(PC++);
	 if (!(F & FLAG_Z)) {
		 WZ = PC + offs;
		 PC = WZ;
		 delay(12);
	 }
	 else
		 delay(7);
	 break;
 case 0x21:			/* LD HL, nn */
	 HL = readw(PC);
	 PC += 2;
	 delay(10);
	 break;
 case 0x22:			/* LD (nn), HL */
	 WZ = readw(PC);
	 PC += 2;
	 writew(WZ++, HL);
	 delay(16);
	 break;
 case 0x23:			/* INC HL */
	 HL++;
	 delay(6);
	 break;
 case 0x24:			/* INC H */
	 inc(H);
	 delay(4);
	 break;
 case 0x25:			/* DEC H */
	 dec(H);
	 delay(4);
	 break;
 case 0x26:			/* LD H, n */
	 H = readb(PC++);
	 delay(7);
	 break;
 case 0x27:			/* DAA */
	 daa;
	 delay(4);
	 break;

 case 0x28:			/* JR Z, $+n */
	 offs = (int) (signed char) readb(PC++);
	 if (F & FLAG_Z) {
		 WZ = PC + offs;
		 PC = WZ;
		 delay(12);
	 }
	 else
		 delay(7);
	 break;
 case 0x29:			/* ADD HL, HL */
	 WZ = HL + 1;
	 add16(HLw, HLw);
	 delay(11);
	 break;
 case 0x2A:			/* LD HL, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 HL = readw(WZ++);
	 delay(16);
	 break;
 case 0x2B:			/* DEC HL */
	 HL--;
	 delay(6);
	 break;
 case 0x2C:			/* INC L */
	 inc(L);
	 delay(4);
	 break;
 case 0x2D:			/* DEC L */
	 dec(L);
	 delay(4);
	 break;
 case 0x2E:			/* LD L,n */
	 L = readb(PC++);
	 delay(7);
	 break;
 case 0x2F:			/* CPL */
	 cpl(A);
	 delay(4);
	 break;

 case 0x30:			/* JR NC, $+n */
	 offs = (int) (signed char) readb(PC++);
	 if (!(F & FLAG_C)) {
		 WZ = PC + offs;
		 PC = WZ;
		 delay(12);
	 }
	 else
		 delay(7);
	 break;
 case 0x31:			/* LD SP, nn */
	 SP = readw(PC);
	 PC += 2;
	 delay(10);
	 break;
 case 0x32:			/* LD (nn), A */
	 tmp2 = readw(PC);
	 PC += 2;
	 writeb(tmp2, A);
	 W = A;			/* is this really correct?! */
	 delay(13);
	 break;
 case 0x33:			/* INC SP */
	 SP++;
	 delay(6);
	 break;
 case 0x34:			/* INC (HL) */
	 tmp1 = readb(HL);
	 inc(tmp1);
	 writeb(HL, tmp1);
	 delay(11);
	 break;
 case 0x35:			/* DEC (HL) */
	 tmp1 = readb(HL);
	 dec(tmp1);
	 writeb(HL, tmp1);
	 delay(11);
	 break;
 case 0x36:			/* LD (HL), n */
	 tmp1 = readb(PC++);
	 writeb(HL, tmp1);
	 delay(10);
	 break;
 case 0x37:			/* SCF */
	 F |= FLAG_C;
	 delay(4);
	 break;
 case 0x38:			/* JR C, $+n */
	 offs = (int) (signed char) readb(PC++);
	 if (F & FLAG_C) {
		 WZ = PC + offs;
		 PC = WZ;
		 delay(12);
	 }
	 else
		 delay(7);
	 break;
 case 0x39:			/* ADD HL, SP */
	 WZ = HL + 1;
	 add16(HLw, SPw);
	 delay(11);
	 break;
 case 0x3A:			/* LD A, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 A = readb(WZ++);
	 delay(13);
	 break;
 case 0x3B:			/* DEC SP */
	 SP--;
	 delay(6);
	 break;
 case 0x3C:			/* INC A */
	 inc(A);
	 delay(4);
	 break;
 case 0x3D:			/* DEC A */
	 dec(A);
	 delay(4);
	 break;
 case 0x3E:			/* LD A, n */
	 A = readb(PC++);
	 delay(7);
	 break;
 case 0x3F:			/* CCF */
	 F ^= FLAG_C;
	 delay(4);
	 break;

 case 0x40: B = B; delay(4); break;
 case 0x41: B = C; delay(4); break;
 case 0x42: B = D; delay(4); break;
 case 0x43: B = E; delay(4); break;
 case 0x44: B = H; delay(4); break;
 case 0x45: B = L; delay(4); break;
 case 0x46: B = readb(HL); delay(7); break;
 case 0x47: B = A; delay(4); break;
 case 0x48: C = B; delay(4); break;
 case 0x49: C = C; delay(4); break;
 case 0x4A: C = D; delay(4); break;
 case 0x4B: C = E; delay(4); break;
 case 0x4C: C = H; delay(4); break;
 case 0x4D: C = L; delay(4); break;
 case 0x4E: C = readb(HL); delay(7); break;
 case 0x4F: C = A; delay(4); break;
 case 0x50: D = B; delay(4); break;
 case 0x51: D = C; delay(4); break;
 case 0x52: D = D; delay(4); break;
 case 0x53: D = E; delay(4); break;
 case 0x54: D = H; delay(4); break;
 case 0x55: D = L; delay(4); break;
 case 0x56: D = readb(HL); delay(7); break;
 case 0x57: D = A; delay(4); break;
 case 0x58: E = B; delay(4); break;
 case 0x59: E = C; delay(4); break;
 case 0x5A: E = D; delay(4); break;
 case 0x5B: E = E; delay(4); break;
 case 0x5C: E = H; delay(4); break;
 case 0x5D: E = L; delay(4); break;
 case 0x5E: E = readb(HL); delay(7); break;
 case 0x5F: E = A; delay(4); break;
 case 0x60: H = B; delay(4); break;
 case 0x61: H = C; delay(4); break;
 case 0x62: H = D; delay(4); break;
 case 0x63: H = E; delay(4); break;
 case 0x64: H = H; delay(4); break;
 case 0x65: H = L; delay(4); break;
 case 0x66: H = readb(HL); delay(7); break;
 case 0x67: H = A; delay(4); break;
 case 0x68: L = B; delay(4); break;
 case 0x69: L = C; delay(4); break;
 case 0x6A: L = D; delay(4); break;
 case 0x6B: L = E; delay(4); break;
 case 0x6C: L = H; delay(4); break;
 case 0x6D: L = L; delay(4); break;
 case 0x6E: L = readb(HL); delay(7); break;
 case 0x6F: L = A; delay(4); break;
 case 0x70: writeb(HL, B); delay(7); break;
 case 0x71: writeb(HL, C); delay(7); break;
 case 0x72: writeb(HL, D); delay(7); break;
 case 0x73: writeb(HL, E); delay(7); break;
 case 0x74: writeb(HL, H); delay(7); break;
 case 0x75: writeb(HL, L); delay(7); break;
 case 0x76: delay(4); break;
 case 0x77: writeb(HL, A); delay(7); break;
 case 0x78: A = B; delay(4); break;
 case 0x79: A = C; delay(4); break;
 case 0x7A: A = D; delay(4); break;
 case 0x7B: A = E; delay(4); break;
 case 0x7C: A = H; delay(4); break;
 case 0x7D: A = L; delay(4); break;
 case 0x7E: A = readb(HL); delay(7); break;
 case 0x7F: A = A; delay(4); break;

 case 0x80: add8(A, B); delay(4); break;
 case 0x81: add8(A, C); delay(4); break;
 case 0x82: add8(A, D); delay(4); break;
 case 0x83: add8(A, E); delay(4); break;
 case 0x84: add8(A, H); delay(4); break;
 case 0x85: add8(A, L); delay(4); break;
 case 0x86: add8(A, readb(HL)); delay(7); break;
 case 0x87: add8(A, A); delay(4); break;
 case 0x88: adc8(A, B); delay(4); break;
 case 0x89: adc8(A, C); delay(4); break;
 case 0x8A: adc8(A, D); delay(4); break;
 case 0x8B: adc8(A, E); delay(4); break;
 case 0x8C: adc8(A, H); delay(4); break;
 case 0x8D: adc8(A, L); delay(4); break;
 case 0x8E: adc8(A, readb(HL)); delay(7); break;
 case 0x8F: adc8(A, A); delay(4); break;
 case 0x90: sub8(A, B); delay(4); break;
 case 0x91: sub8(A, C); delay(4); break;
 case 0x92: sub8(A, D); delay(4); break;
 case 0x93: sub8(A, E); delay(4); break;
 case 0x94: sub8(A, H); delay(4); break;
 case 0x95: sub8(A, L); delay(4); break;
 case 0x96: sub8(A, readb(HL)); delay(7); break;
 case 0x97: sub8(A, A); delay(4); break;
 case 0x98: sbc8(A, B); delay(4); break;
 case 0x99: sbc8(A, C); delay(4); break;
 case 0x9A: sbc8(A, D); delay(4); break;
 case 0x9B: sbc8(A, E); delay(4); break;
 case 0x9C: sbc8(A, H); delay(4); break;
 case 0x9D: sbc8(A, L); delay(4); break;
 case 0x9E: sbc8(A, readb(HL)); delay(7); break;
 case 0x9F: sbc8(A, A); delay(4); break;
 case 0xA0: and(A, B); delay(4); break;
 case 0xA1: and(A, C); delay(4); break;
 case 0xA2: and(A, D); delay(4); break;
 case 0xA3: and(A, E); delay(4); break;
 case 0xA4: and(A, H); delay(4); break;
 case 0xA5: and(A, L); delay(4); break;
 case 0xA6: and(A, readb(HL)); delay(7); break;
 case 0xA7: and(A, A); delay(4); break;
 case 0xA8: xor(A, B); delay(4); break;
 case 0xA9: xor(A, C); delay(4); break;
 case 0xAA: xor(A, D); delay(4); break;
 case 0xAB: xor(A, E); delay(4); break;
 case 0xAC: xor(A, H); delay(4); break;
 case 0xAD: xor(A, L); delay(4); break;
 case 0xAE: xor(A, readb(HL)); delay(7); break;
 case 0xAF: xor(A, A); delay(4); break;
 case 0xB0: or(A, B); delay(4); break;
 case 0xB1: or(A, C); delay(4); break;
 case 0xB2: or(A, D); delay(4); break;
 case 0xB3: or(A, E); delay(4); break;
 case 0xB4: or(A, H); delay(4); break;
 case 0xB5: or(A, L); delay(4); break;
 case 0xB6: or(A, readb(HL)); delay(7); break;
 case 0xB7: or(A, A); delay(4); break;
 case 0xB8: cp(A, B); delay(4); break;
 case 0xB9: cp(A, C); delay(4); break;
 case 0xBA: cp(A, D); delay(4); break;
 case 0xBB: cp(A, E); delay(4); break;
 case 0xBC: cp(A, H); delay(4); break;
 case 0xBD: cp(A, L); delay(4); break;
 case 0xBE: cp(A, readb(HL)); delay(7); break;
 case 0xBF: cp(A, A); delay(4); break;

 case 0xC0:			/* RET NZ */
	 if (!(F & FLAG_Z)) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xC1:			/* POP BC */
	 pop(BC);
	 delay(10);
	 break;
 case 0xC2:			/* JP NZ, nn */
	 WZ = readw(PC);
	 if (!(F & FLAG_Z))
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xC3:			/* JP nn */
	 WZ = readw(PC);
	 PC = WZ;
	 delay(10);
	 break;
 case 0xC4:			/* CALL NZ, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (!(F & FLAG_Z)) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;
 case 0xC5:			/* PUSH BC */
	 push(BC);
	 delay(11);
	 break;
 case 0xC6:			/* ADD A, n */
	 add8(A, readb(PC++));
	 delay(7);
	 break;
 case 0xC7:			/* RST 00h */
	 /* FIXME: I have not tested whether RST affects WZ */
	 push(PC);
	 PC = 0x0000;
	 delay(11);
	 break;

 case 0xC8:			/* RET Z */
	 if (F & FLAG_Z) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xC9:			/* RET */
	 pop(WZ);
	 PC = WZ;
	 delay(10);
	 break;
 case 0xCA:			/* JP Z, nn */
	 WZ = readw(PC);
	 if (F & FLAG_Z)
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;

 case 0xCB:
	 op = readb_m1(PC++);
	 goto opcode_cb;

 case 0xCC:			/* CALL Z, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (F & FLAG_Z) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;
 case 0xCD:			/* CALL nn */
	 WZ = readw(PC);
	 PC += 2;
	 push(PC);
	 PC = WZ;
	 delay(17);
	 break;
 case 0xCE:			/* ADC A, n */
	 adc8(A, readb(PC++));
	 delay(7);
	 break;
 case 0xCF:			/* RST 08h */
	 push(PC);
	 PC = 0x0008;
	 delay(11);
	 break;

 case 0xD0:			/* RET NC */
	 if (!(F & FLAG_C)) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xD1:			/* POP DE */
	 pop(DE);
	 delay(10);
	 break;
 case 0xD2:			/* JP NC, nn */
	 WZ = readw(PC);
	 if (!(F & FLAG_C))
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xD3:			/* OUT (n), A */
	 W = A;
	 Z = readb(PC++);
	 delay(11);
	 output(WZ, A);
	 break;
 case 0xD4:			/* CALL NC, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (!(F & FLAG_C)) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;
 case 0xD5:			/* PUSH DE */
	 push(DE);
	 delay(11);
	 break;
 case 0xD6:			/* SUB n */
	 sub8(A, readb(PC++));
	 delay(7);
	 break;
 case 0xD7:			/* RST 10h */
	 push(PC);
	 PC = 0x0010;
	 delay(11);
	 break;

 case 0xD8:			/* RET C */
	 if (F & FLAG_C) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xD9:			/* EXX */
	 ex(BC, BC2);
	 ex(DE, DE2);
	 ex(HL, HL2);
	 ex(WZ, WZ2);
	 delay(4);
	 break;
 case 0xDA:			/* JP C, nn */
	 WZ = readw(PC);
	 if (F & FLAG_C)
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xDB:			/* IN A, (n) */
	 W = A;
	 Z = readb(PC++);
	 delay(11);
	 A = input(WZ);
	 break;
 case 0xDC:			/* CALL C, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (F & FLAG_C) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;

 case 0xDD:
	 op = readb_m1(PC++);
	 delay(4);
	 goto opcode_dd;

 case 0xDE:			/* SBC A, n */
	 sbc8(A, readb(PC++));
	 delay(7);
	 break;
 case 0xDF:			/* RST 18h */
	 push(PC);
	 PC = 0x0018;
	 delay(11);
	 break;

 case 0xE0:			/* RET PO */
	 if (!(F & FLAG_P)) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xE1:			/* POP HL */
	 pop(HL);
	 delay(10);
	 break;
 case 0xE2:			/* JP PO, nn */
	 WZ = readw(PC);
	 if (!(F & FLAG_P))
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xE3:			/* EX (SP), HL */
	 WZ = readw(SP);
	 writew(SP, HL);
	 HL = WZ;
	 delay(19);
	 break;
 case 0xE4:			/* CALL PO, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (!(F & FLAG_P)) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;
 case 0xE5:			/* PUSH HL */
	 push(HL);
	 delay(11);
	 break;
 case 0xE6:			/* AND n */
	 and(A, readb(PC++));
	 delay(7);
	 break;
 case 0xE7:			/* RST 20h */
	 push(PC);
	 PC = 0x0020;
	 delay(11);
	 break;

 case 0xE8:			/* RET PE */
	 if (F & FLAG_P) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xE9:			/* JP HL */
	 PC = HL;
	 delay(4);
	 break;
 case 0xEA:			/* JP PE, nn */
	 WZ = readw(PC);
	 if (F & FLAG_P)
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xEB:			/* EX DE,HL */
	 ex(DE, HL);
	 delay(4);
	 break;
 case 0xEC:			/* CALL PE, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (F & FLAG_P) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;

 case 0xED:
	 op = readb_m1(PC++);
	 goto opcode_ed;
		
 case 0xEE:			/* XOR n */
	 xor(A, readb(PC++));
	 delay(7);
	 break;
 case 0xEF:			/* RST 28h */
	 push(PC);
	 PC = 0x0028;
	 delay(11);
	 break;

 case 0xF0:			/* RET P */
	 if (!(F & FLAG_S)) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xF1:			/* POP AF */
	 pop(AF);
	 delay(10);
	 break;
 case 0xF2:			/* JP P, nn */
	 WZ = readw(PC);
	 if (!(F & FLAG_S))
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xF3:			/* DI */
	 IFF1 = IFF2 = 0;
	 delay(4);
	 break;
 case 0xF4:			/* CALL P, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (!(F & FLAG_S)) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;
 case 0xF5:			/* PUSH AF */
	 push(AF);
	 delay(11);
	 break;
 case 0xF6:			/* OR n */
	 or(A, readb(PC++));
	 delay(7);
	 break;
 case 0xF7:			/* RST 30h */
	 push(PC);
	 PC = 0x0030;
	 delay(11);
	 break;
		
 case 0xF8:			/* RET M */
	 if (F & FLAG_S) {
		 pop(WZ);
		 PC = WZ;
		 delay(11);
	 }
	 else
		 delay(5);
	 break;
 case 0xF9:			/* LD SP, HL */
	 SP = HL;
	 delay(4);
	 break;
 case 0xFA:			/* JP M, nn */
	 WZ = readw(PC);
	 if (F & FLAG_S)
		 PC = WZ;
	 else
		 PC += 2;
	 delay(10);
	 break;
 case 0xFB:			/* EI */
	 IFF1 = IFF2 = 1;
	 delay(4);
	 break;
 case 0xFC:			/* CALL M, nn */
	 WZ = readw(PC);
	 PC += 2;
	 if (F & FLAG_S) {
		 push(PC);
		 PC = WZ;
		 delay(17);
	 }
	 else
		 delay(10);
	 break;
		
 case 0xFD:
	 op = readb_m1(PC++);
	 delay(4);
	 goto opcode_fd;
		
 case 0xFE:			/* CP n */
	 cp(A, readb(PC++));
	 delay(7);
	 break;
 case 0xFF:			/* RST 38h */
	 push(PC);
	 PC = 0x0038;
	 delay(11);
	 break;
 }
