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
 case 0x40:			/* IN B, (C) */
	 WZ = BC;
	 delay(12);
	 in(B);
	 break;
 case 0x41:			/* OUT (C), B */
	 delay(12);
	 output(BC, B);
	 break;
 case 0x42:			/* SBC HL, BC */
	 WZ = HL + 1;
	 sbc16(HLw, BCw);
	 delay(15);
	 break;
 case 0x43:			/* LD (nn), BC */
	 WZ = readw(PC);
	 PC += 2;
	 writew(WZ++, BC);
	 delay(20);
	 break;
 case 0x44:			/* NEG */
	 neg(A);
	 delay(8);
	 break;
 case 0x45:			/* RETN */
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x46:			/* IM 0 */
	 IM = 0;
	 delay(8);
	 break;
 case 0x47:			/* LD I,A */
	 I = A;
	 delay(9);
	 break;

 case 0x48:			/* IN C, (C) */
	 WZ = BC;
	 delay(12);
	 in(C);
	 break;
 case 0x49:			/* OUT (C), C */
	 delay(12);
	 output(BC, C);
	 break;
 case 0x4A:			/* ADC HL, BC */
	 WZ = HL + 1;
	 adc16(HLw, BCw);
	 delay(15);
	 break;
 case 0x4B:			/* LD BC, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 BC = readw(WZ++);
	 delay(20);
	 break;
 case 0x4C:			/* NEG */
	 UNDOCUMENTED(8);
	 neg(A);
	 delay(8);
	 break;
 case 0x4D:			/* RETI */
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x4E:			/* IM 0 */
	 UNDOCUMENTED(8);
	 IM = 0;
	 delay(8);
	 break;
 case 0x4F:			/* LD R,A */
	 Rl = A;
	 Rh = A & 0x80;
	 delay(9);
	 break;

 case 0x50:			/* IN D, (C) */
	 WZ = BC;
	 delay(12);
	 in(D);
	 break;
 case 0x51:			/* OUT (C), D */
	 delay(12);
	 output(BC, D);
	 break;
 case 0x52:			/* SBC HL, DE */
	 WZ = HL + 1;
	 sbc16(HLw, DEw);
	 delay(15);
	 break;
 case 0x53:			/* LD (nn), DE */
	 WZ = readw(PC);
	 PC += 2;
	 writew(WZ++, DE);
	 delay(20);
	 break;
 case 0x54:			/* NEG */
	 UNDOCUMENTED(8);
	 neg(A);
	 delay(8);
	 break;
 case 0x55:			/* RETN */
	 UNDOCUMENTED(8);
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x56:			/* IM 1 */
	 IM = 1;
	 delay(8);
	 break;
 case 0x57:			/* LD A,I */
	 ld_a_ir(I);
	 delay(9);
	 break;

 case 0x58:			/* IN E, (C) */
	 WZ = BC;
	 delay(12);
	 in(E);
	 break;
 case 0x59:			/* OUT (C), E */
	 delay(12);
	 output(BC, E);
	 break;
 case 0x5A:			/* ADC HL, DE */
	 WZ = HL + 1;
	 adc16(HLw, DEw);
	 delay(15);
	 break;
 case 0x5B:			/* LD DE, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 DE = readw(WZ++);
	 delay(20);
	 break;
 case 0x5C:			/* NEG */
	 UNDOCUMENTED(8);
	 neg(A);
	 delay(8);
	 break;
 case 0x5D:			/* RETN */
	 UNDOCUMENTED(8);
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x5E:			/* IM 2 */
	 IM = 2;
	 delay(8);
	 break;
 case 0x5F:			/* LD A,R */
	 ld_a_ir(R);
	 delay(9);
	 break;

 case 0x60:			/* IN H, (C) */
	 WZ = BC;
	 delay(12);
	 in(H);
	 break;
 case 0x61:			/* OUT (C), H */
	 delay(12);
	 output(BC, H);
	 break;
 case 0x62:			/* SBC HL, HL */
	 WZ = HL + 1;
	 sbc16(HLw, HLw);
	 delay(15);
	 break;
 case 0x63:			/* LD (nn), HL */
	 WZ = readw(PC);
	 PC += 2;
	 writew(WZ++, HL);
	 delay(20);
	 break;
 case 0x64:			/* NEG */
	 UNDOCUMENTED(8);
	 neg(A);
	 delay(8);
	 break;
 case 0x65:			/* RETN */
	 UNDOCUMENTED(8);
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x66:			/* IM 0 */
	 UNDOCUMENTED(8);
	 IM = 0;
	 delay(8);
	 break;
 case 0x67:			/* RRD */
	 rrd;
	 delay(18);
	 break;

 case 0x68:			/* IN L, (C) */
	 WZ = BC;
	 delay(12);
	 in(L);
	 break;
 case 0x69:			/* OUT (C), L */
	 delay(12);
	 output(BC, L);
	 break;
 case 0x6A:			/* ADC HL, HL */
	 WZ = HL + 1;
	 adc16(HLw, HLw);
	 delay(15);
	 break;
 case 0x6B:			/* LD HL, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 HL = readw(WZ++);
	 delay(20);
	 break;
 case 0x6C:			/* NEG */
	 UNDOCUMENTED(8);
	 neg(A);
	 delay(8);
	 break;
 case 0x6D:			/* RETN */
	 UNDOCUMENTED(8);
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x6E:			/* IM 0 */
	 UNDOCUMENTED(8);
	 IM = 0;
	 delay(8);
	 break;
 case 0x6F:			/* RLD */
	 rld;
	 delay(18);
	 break;

 case 0x70:			/* IN (C) */
	 UNDOCUMENTED(8);
	 WZ = BC;
	 delay(12);
	 in(tmp1);
	 break;
 case 0x71:			/* OUT (C), 0 */
	 UNDOCUMENTED(8);
	 delay(12);
	 output(BC, 0);
	 break;
 case 0x72:			/* SBC HL, SP */
	 WZ = HL + 1;
	 sbc16(HLw, SPw);
	 delay(15);
	 break;
 case 0x73:			/* LD (nn), SP */
	 WZ = readw(PC);
	 PC += 2;
	 writew(WZ++, SP);
	 delay(20);
	 break;
 case 0x74:			/* NEG */
	 UNDOCUMENTED(8);	
	 neg(A);
	 delay(8);
	 break;
 case 0x75:			/* RETN */
	 UNDOCUMENTED(8);
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x76:			/* IM 1 */
	 UNDOCUMENTED(8);
	 IM = 1;
	 delay(8);
	 break;

 case 0x78:			/* IN A, (C) */
	 WZ = BC;
	 delay(12);
	 in(A);
	 break;
 case 0x79:			/* OUT (C), A */
	 delay(12);
	 output(BC, A);
	 break;
 case 0x7A:			/* ADC HL, SP */
	 WZ = HL + 1;
	 adc16(HLw, SPw);
	 delay(15);
	 break;
 case 0x7B:			/* LD SP, (nn) */
	 WZ = readw(PC);
	 PC += 2;
	 SP = readw(WZ);
	 WZ++;
	 delay(20);
	 break;
 case 0x7C:			/* NEG */
	 UNDOCUMENTED(8);
	 neg(A);
	 delay(8);
	 break;
 case 0x7D:			/* RETN */
	 UNDOCUMENTED(8);
	 IFF1 = IFF2;
	 pop(PC);
	 delay(14);
	 break;
 case 0x7E:			/* IM 2 */
	 UNDOCUMENTED(8);
	 IM = 2;
	 delay(8);
	 break;

 case 0xA0:			/* LDI */
	 ldi;
	 delay(16);
	 break;
 case 0xA1:			/* CPI */
	 cpi;
	 delay(16);
	 break;
 case 0xA2:			/* INI */
	 delay(13);
	 ini;
	 delay(3);
	 break;
 case 0xA3:			/* OUTI */
	 delay(16);
	 outi;
	 break;

 case 0xA8:			/* LDD */
	 ldd;
	 delay(16);
	 break;
 case 0xA9:			/* CPD */
	 cpd;
	 delay(16);
	 break;
 case 0xAA:			/* IND */
	 delay(13);
	 ind;
	 delay(3);
	 break;
 case 0xAB:			/* OUTD */
	 delay(16);
	 outd;
	 break;

 case 0xB0:			/* LDIR */
	 ldi;
	 if (BCw) {
		 PC -= 2;
		 delay(21);
	 }
	 else
		 delay(16);
	 break;
 case 0xB1:			/* CPIR */
	 cpi;
	 if (BCw && !(F & FLAG_Z)) {
		 PC -= 2;
		 delay(21);
	 }
	 else
		 delay(16);
	 break;
 case 0xB2:			/* INIR */
	 delay(13);
	 ini;
	 if (B) {
		 PC -= 2;
		 delay(8);
	 }
	 else
		 delay(3);
	 break;
 case 0xB3:			/* OTIR */
	 delay(16);
	 outi;
	 if (B) {
		 PC -= 2;
		 delay(5);
	 }
	 break;

 case 0xB8:			/* LDDR */
	 ldd;
	 if (BCw) {
		 PC -= 2;
		 delay(21);
	 }
	 else
		 delay(16);
	 break;
 case 0xB9:			/* CPDR */
	 cpd;
	 if (BCw && !(F & FLAG_Z)) {
		 PC -= 2;
		 delay(21);
	 }
	 else
		 delay(16);
	 break;
 case 0xBA:			/* INDR */
	 delay(13);
	 ind;
	 if (B) {
		 PC -= 2;
		 delay(8);
	 }
	 else
		 delay(3);
	 break;
 case 0xBB:			/* OTDR */
	 delay(16);
	 outd;
	 if (B) {
		 PC -= 2;
		 delay(5);
	 }
	 break;

 default:
	 delay(8);
	 if (calc->hw.z80_instr)
		 (*calc->hw.z80_instr)(calc, 0xed00 | op);
	 else if (calc->z80.emuflags & TILEM_Z80_BREAK_INVALID)
		 tilem_z80_stop(calc, TILEM_STOP_INVALID_INST);
	 break;
 }
