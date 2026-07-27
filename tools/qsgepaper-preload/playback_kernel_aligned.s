.syntax unified
.arch armv7-a
.arch_extension idiv
.fpu neon
.global playback_kernel_aligned

playback_kernel_aligned:
push {r4, r5, r6, r7, r8, lr}
mov r4, r2
ldr ip, [sp, #0x18]
mov r2, r3
cmp r4, #8
addls pc, pc, r4, lsl #2
b .L0x3a3f0
b .L0x3a280
b .L0x3a39c
b .L0x3a3d8
b .L0x3a3e4
b .L0x3a3a8
b .L0x3a3b4
b .L0x3a3c0
b .L0x3a3cc
b .L0x3a274

.L0x3a274:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3dbe0

.L0x3a280:
ldr r3, [r1, #0x14]
ldr r0, [r1, #0xc]
cmp r3, r0
movlt ip, r4
mvnlt r6, #0
blt .L0x3a2e8
ldr r5, [r1, #0x10]
sub r6, ip, #1
ldr lr, [r1, #0x18]
cmp r5, lr
movgt ip, r4
mvngt r6, #0
bgt .L0x3a2d4
sub lr, lr, r5
cmp r2, r6
add r5, lr, #1
moveq r6, lr
udiv r5, r5, ip
mul ip, r5, r2
subne r5, r5, #1
addne r6, r5, ip

.L0x3a2d4:
ldr lr, [r1, #0x18]
ldr r2, [r1, #0x10]
cmp lr, r2
subge r3, r3, r0
addge r4, r3, #1

.L0x3a2e8:
ldr r3, [r1, #0x3c]
cmp ip, r6
ldr r5, [r3, #0x14]
pophi {r4, r5, r6, r7, r8, pc}
lsl r5, r5, #1
lsr lr, r4, #3
lsl r2, r4, #1
ldr r7, [r1, #0x44]
mul r4, r5, ip

.L0x3a30c:
add r8, r7, r4
ubfx r1, r8, #4, #2
add r0, r8, r2
rsb r1, r1, #0
ubfx r0, r0, #4, #2
and r1, r1, #3
cmp r1, lr
movhs r1, lr
cmp r1, #0
moveq r3, r8
beq .L0x3a348
add r3, r8, r1, lsl #4

.L0x3a33c:
add r8, r8, #0x10
cmp r8, r3
bne .L0x3a33c

.L0x3a348:
sub r0, lr, r0
bic r0, r0, r0, asr #31
cmp r0, r1
bls .L0x3a36c

.L0x3a358:
add r1, r1, #4
add r3, r3, #0x40
cmp r0, r1
pld [r3, #0x80]
bhi .L0x3a358

.L0x3a36c:
cmp lr, r1
subhi r1, lr, r1
addhi r1, r3, r1, lsl #4
bls .L0x3a388

.L0x3a37c:
add r3, r3, #0x10
cmp r3, r1
bne .L0x3a37c

.L0x3a388:
add r4, r4, r5
add ip, ip, #1
cmp ip, r6
bls .L0x3a30c
pop {r4, r5, r6, r7, r8, pc}

.L0x3a39c:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3a3f8

.L0x3a3a8:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3b7f0

.L0x3a3b4:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3bfb8

.L0x3a3c0:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3c860

.L0x3a3cc:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3d1c0

.L0x3a3d8:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3a9e0

.L0x3a3e4:
pop {r4, r5, r6, r7, r8, lr}
mov r3, ip
b .L0x3b098

.L0x3a3f0:
pop {r4, r5, r6, r7, r8, pc}

.L0x3a3f8:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r7, r1
ldr r1, [r1, #0x14]
sub sp, sp, #0x1c
ldr lr, [r7, #0xc]
cmp r1, lr
blt .L0x3a450
ldr r4, [r7, #0x10]
sub r5, r3, #1
ldr ip, [r7, #0x18]
cmp r4, ip
ble .L0x3a9b8
mov r8, #0
mvn r3, #0
str r3, [sp, #0xc]

.L0x3a434:
ldr r2, [r7, #0x18]
ldr r3, [r7, #0x10]
cmp r2, r3
subge r1, r1, lr
addge ip, r1, #1
movlt ip, #0
b .L0x3a460

.L0x3a450:
mov ip, #0
mvn r3, #0
str r3, [sp, #0xc]
mov r8, ip

.L0x3a460:
ldr r3, [r7, #0x2c]
mov r2, #0x10
ldr r4, [r0]
cmp lr, #0
ldr r0, [r3, #0xc]
mov fp, #0x104
ldrsh r1, [r7, #0x28]
udiv r2, r2, r0
ldr r0, [sp, #0xc]
sdiv r1, r1, r2
ldr r5, [r7, #0x3c]
add r2, lr, #7
movge r2, lr
ldr lr, [r7, #0x10]
cmp r0, r8
ldr r0, [r3, #4]
add lr, lr, #3
asr r2, r2, #3
mul r0, r0, r0
mla r2, fp, lr, r2
mla r1, r0, r1, r1
ldr r3, [r3, #0x10]
add r2, r2, #0x1a
add r3, r3, r1, lsl #1
add sl, r4, r2, lsl #2
ldr r1, [r5, #0x14]
blo .L0x3a9b0
lsl r2, r1, #1
lsr sb, ip, #3
lsl r1, ip, #1
vldr d20, .LDIC0x3a628
vldr d21, .LDIC0x3a630
mul ip, r8, r2
vmov.i16 q9, #3
vmov.i32 q11, #0
str r2, [sp, #0x14]
mov r0, ip
str r1, [sp, #0x10]

.L0x3a4f8:
ldr r2, [r7, #0x44]
ldr r1, [sp, #0x10]
add r2, r2, r0
ubfx lr, r2, #4, #2
add r5, r2, r1
rsb lr, lr, #0
ubfx r5, r5, #4, #2
and lr, lr, #3
cmp lr, sb
movhs lr, sb
cmp lr, #0
moveq ip, r2
beq .L0x3a638
add ip, r2, lr, lsl #4
mov r1, #0x410
mla r4, r1, r8, sl

.L0x3a538:
vld1.16 {d6, d7}, [r2]!
vmov.u16 r1, d7[2]
cmp ip, r2
add r1, r3, r1, lsl #1
vld1.16 {d24[], d25[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q12, q12, q10
add r1, r3, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q12, q12, q9
uxth r1, r1
vshl.u16 q8, q8, q10
add r1, r3, r1, lsl #1
vshl.i16 q12, q12, #2
vld1.16 {d26[], d27[]}, [r1]
vand q8, q8, q9
vshl.u16 q13, q13, q10
vmov.u16 r1, d7[1]
vshl.i16 q13, q13, #0xe
add r1, r3, r1, lsl #1
vadd.i16 q8, q8, q13
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q12, q12, q10
add r1, r3, r1, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #4
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q12, q12, q10
add r1, r3, r1, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #6
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q12, q12, q10
add r1, r3, r1, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #8
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q12, q12, q10
add r1, r3, r1, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #0xa
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r1]
vshl.u16 q12, q12, q10
vand q12, q12, q9
vshl.i16 q12, q12, #0xc
vadd.i16 q8, q8, q12
vst1.16 {d16[0]}, [r4]
add r4, r4, #4
bne .L0x3a538
b .L0x3a638

.LDIC0x3a628:
.quad -1407387768586240

.LDIC0x3a630:
.quad -3659221942468616

.L0x3a638:
sub r5, sb, r5
bic r5, r5, r5, asr #31
cmp r5, lr
bls .L0x3a894
mla r2, fp, r8, lr
str r8, [sp, #8]
mov r1, ip
add r2, sl, r2, lsl #2
mov r4, lr
stm sp, {r0, lr}

.L0x3a660:
mov r0, r1
add r1, r1, #0x40
vorr q12, q11, q11
vld4.16 {d0, d2, d4, d6}, [r0]!
vorr q8, q11, q11
vld4.16 {d1, d3, d5, d7}, [r0]
vorr q13, q11, q11
vmov.u16 r0, d0[1]
vorr q14, q11, q11
add r0, r3, r0, lsl #1
add r4, r4, #4
pld [r1, #0x80]
add r8, r2, #4
vld1.16 {d24[0]}, [r0]
vmov.u16 r0, d2[1]
cmp r4, r5
add r6, r2, #8
add lr, r2, #0xc
add r0, r3, r0, lsl #1
vld1.16 {d16[0]}, [r0]
vmov.u16 r0, d4[1]
add r0, r3, r0, lsl #1
vld1.16 {d26[0]}, [r0]
vmov r0, s0
uxth r0, r0
add r0, r3, r0, lsl #1
vld1.16 {d24[1]}, [r0]
vmov r0, s4
uxth r0, r0
add r0, r3, r0, lsl #1
vld1.16 {d16[1]}, [r0]
vmov r0, s8
uxth r0, r0
add r0, r3, r0, lsl #1
vld1.16 {d26[1]}, [r0]
vmov.u16 r0, d0[3]
add r0, r3, r0, lsl #1
vld1.16 {d24[2]}, [r0]
vmov.u16 r0, d2[3]
add r0, r3, r0, lsl #1
vld1.16 {d16[2]}, [r0]
vmov.u16 r0, d4[3]
add r0, r3, r0, lsl #1
vld1.16 {d26[2]}, [r0]
vmov.u16 r0, d6[1]
add r0, r3, r0, lsl #1
vld1.16 {d28[0]}, [r0]
vmov.u16 r0, d0[2]
add r0, r3, r0, lsl #1
vld1.16 {d24[3]}, [r0]
vmov.u16 r0, d2[2]
add r0, r3, r0, lsl #1
vld1.16 {d16[3]}, [r0]
vmov.u16 r0, d4[2]
add r0, r3, r0, lsl #1
vld1.16 {d26[3]}, [r0]
vmov r0, s12
uxth r0, r0
add r0, r3, r0, lsl #1
vld1.16 {d28[1]}, [r0]
vmov.u16 r0, d1[1]
add r0, r3, r0, lsl #1
vld1.16 {d25[0]}, [r0]
vmov.u16 r0, d3[1]
add r0, r3, r0, lsl #1
vld1.16 {d17[0]}, [r0]
vmov.u16 r0, d5[1]
add r0, r3, r0, lsl #1
vld1.16 {d27[0]}, [r0]
vmov.u16 r0, d6[3]
add r0, r3, r0, lsl #1
vld1.16 {d28[2]}, [r0]
vmov.u16 r0, d1[0]
add r0, r3, r0, lsl #1
vld1.16 {d25[1]}, [r0]
vmov.u16 r0, d3[0]
add r0, r3, r0, lsl #1
vld1.16 {d17[1]}, [r0]
vmov.u16 r0, d5[0]
add r0, r3, r0, lsl #1
vld1.16 {d27[1]}, [r0]
vmov.u16 r0, d6[2]
add r0, r3, r0, lsl #1
vld1.16 {d28[3]}, [r0]
vmov.u16 r0, d1[3]
add r0, r3, r0, lsl #1
vld1.16 {d25[2]}, [r0]
vmov.u16 r0, d3[3]
add r0, r3, r0, lsl #1
vld1.16 {d17[2]}, [r0]
vmov.u16 r0, d5[3]
add r0, r3, r0, lsl #1
vld1.16 {d27[2]}, [r0]
vmov.u16 r0, d7[1]
add r0, r3, r0, lsl #1
vld1.16 {d29[0]}, [r0]
vmov.u16 r0, d1[2]
add r0, r3, r0, lsl #1
vld1.16 {d25[3]}, [r0]
vmov.u16 r0, d3[2]
vand q12, q12, q9
add r0, r3, r0, lsl #1
vld1.16 {d17[3]}, [r0]
vmov.u16 r0, d5[2]
vand q8, q8, q9
add r0, r3, r0, lsl #1
vshl.i16 q12, q12, #6
vld1.16 {d27[3]}, [r0]
vmov.u16 r0, d7[0]
vshl.i16 q8, q8, #4
add r0, r3, r0, lsl #1
vand q13, q13, q9
vld1.16 {d29[1]}, [r0]
vmov.u16 r0, d7[3]
vadd.i16 q12, q12, q8
add r0, r3, r0, lsl #1
vshl.i16 q13, q13, #2
vld1.16 {d29[2]}, [r0]
vmov.u16 r0, d7[2]
add r0, r3, r0, lsl #1
vld1.16 {d29[3]}, [r0]
vand q14, q14, q9
vadd.i16 q12, q12, q14
vadd.i16 q12, q12, q13
vmovn.i16 d24, q12
vst1.16 {d24[0]}, [r2]
vst1.16 {d24[1]}, [r8]
add r2, r2, #0x10
vst1.16 {d24[2]}, [r6]
vst1.16 {d24[3]}, [lr]
blo .L0x3a660
ldm sp, {r0, lr}
ldr r8, [sp, #8]
sub r5, r5, #1
sub r5, r5, lr
add lr, lr, #4
bic r2, r5, #3
lsr r5, r5, #2
add lr, r2, lr
add r5, r5, #1
add ip, ip, r5, lsl #6

.L0x3a894:
cmp lr, sb
bhs .L0x3a998
mla r1, fp, r8, lr
sub lr, sb, lr
add r1, sl, r1, lsl #2
add lr, ip, lr, lsl #4

.L0x3a8ac:
vld1.16 {d6, d7}, [ip]!
vmov.u16 r2, d7[2]
cmp ip, lr
add r2, r3, r2, lsl #1
vld1.16 {d24[], d25[]}, [r2]
vmov.u16 r2, d7[3]
vshl.u16 q12, q12, q10
add r2, r3, r2, lsl #1
vld1.16 {d16[], d17[]}, [r2]
vmov r2, s12
vand q12, q12, q9
uxth r2, r2
vshl.u16 q8, q8, q10
add r2, r3, r2, lsl #1
vshl.i16 q12, q12, #2
vld1.16 {d26[], d27[]}, [r2]
vand q8, q8, q9
vshl.u16 q13, q13, q10
vmov.u16 r2, d7[1]
vshl.i16 q13, q13, #0xe
add r2, r3, r2, lsl #1
vadd.i16 q8, q8, q13
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r2]
vmov.u16 r2, d7[0]
vshl.u16 q12, q12, q10
add r2, r3, r2, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #4
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r2]
vmov.u16 r2, d6[3]
vshl.u16 q12, q12, q10
add r2, r3, r2, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #6
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r2]
vmov.u16 r2, d6[2]
vshl.u16 q12, q12, q10
add r2, r3, r2, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #8
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r2]
vmov.u16 r2, d6[1]
vshl.u16 q12, q12, q10
add r2, r3, r2, lsl #1
vand q12, q12, q9
vshl.i16 q12, q12, #0xa
vadd.i16 q8, q8, q12
vld1.16 {d24[], d25[]}, [r2]
vshl.u16 q12, q12, q10
vand q12, q12, q9
vshl.i16 q12, q12, #0xc
vadd.i16 q8, q8, q12
vst1.16 {d16[0]}, [r1]
add r1, r1, #4
bne .L0x3a8ac

.L0x3a998:
ldr r2, [sp, #0x14]
add r8, r8, #1
add r0, r0, r2
ldr r2, [sp, #0xc]
cmp r8, r2
bls .L0x3a4f8

.L0x3a9b0:
add sp, sp, #0x1c
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3a9b8:
sub ip, ip, r4
cmp r2, r5
add r4, ip, #1
streq ip, [sp, #0xc]
udiv r3, r4, r3
mul r8, r3, r2
subne r3, r3, #1
addne r3, r3, r8
strne r3, [sp, #0xc]
b .L0x3a434

.L0x3a9e0:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov lr, r1
vpush {d8, d9, d10, d11}
ldr ip, [lr, #0xc]
sub sp, sp, #0x34
str r1, [sp, #0x20]
ldr r1, [r1, #0x14]
cmp r1, ip
blt .L0x3aa44
ldr r4, [lr, #0x10]
sub r5, r3, #1
ldr lr, [lr, #0x18]
cmp r4, lr
ble .L0x3b070
mov sb, #0
mvn r3, #0
str r3, [sp, #0x24]

.L0x3aa24:
ldr r3, [sp, #0x20]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
subge r2, r1, ip
addge r2, r2, #1
movlt r2, #0
b .L0x3aa54

.L0x3aa44:
mov r2, #0
mvn r3, #0
str r3, [sp, #0x24]
mov sb, r2

.L0x3aa54:
ldr r6, [sp, #0x20]
cmp ip, #0
add r3, ip, #7
movge r3, ip
ldr lr, [r6, #0x2c]
mov r4, #0x10
ldr ip, [sp, #0x24]
ldrsh r1, [r6, #0x28]
cmp sb, ip
ldr ip, [lr, #0xc]
ldr r5, [lr, #0x10]
udiv ip, r4, ip
sdiv r1, r1, ip
ldr ip, [lr, #4]
ldr r4, [r6, #0x10]
mul ip, ip, ip
asr r3, r3, #3
add r4, r4, #3
mla r1, ip, r1, r1
ldr lr, [r6, #0x3c]
add fp, r5, r1, lsl #1
mov r1, #0x104
mla r3, r1, r4, r3
ldr ip, [lr, #0x14]
ldr lr, [r0]
add r3, r3, #0x1a
ldr r0, [r0, #4]
lsl r3, r3, #2
add r8, lr, r3
add r7, r0, r3
bhi .L0x3b064
lsl r3, ip, #1
mul r6, r3, sb
vldr d20, .LDIC0x3ac50
vldr d21, .LDIC0x3ac58
vmov.i16 q9, #3
vmov.i32 q12, #0
vmov.i16 q11, #0xc
lsr r0, r2, #3
mov r5, r6
mul sl, r1, sb
str r3, [sp, #0x2c]
mov r6, r0
lsl r2, r2, #1
str r2, [sp, #0x28]

.L0x3ab08:
ldr r3, [sp, #0x20]
ldr r1, [sp, #0x28]
ldr r3, [r3, #0x44]
add r3, r3, r5
ubfx r2, r3, #4, #2
add r4, r3, r1
rsb r2, r2, #0
ubfx r4, r4, #4, #2
and r2, r2, #3
cmp r2, r6
movlo r0, r2
movhs r0, r6
cmp r0, #0
moveq ip, r3
beq .L0x3ac60
add ip, r3, r0, lsl #4
mov r2, #0x410
mul r2, r2, sb

.L0x3ab50:
vld1.16 {d6, d7}, [r3]!
vmov.u16 r1, d7[2]
cmp r3, ip
add r1, fp, r1, lsl #1
vld1.16 {d26[], d27[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q13, q13, q10
add r1, fp, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q13, q13, q9
uxth r1, r1
vshl.u16 q8, q8, q10
add r1, fp, r1, lsl #1
vshl.i16 q13, q13, #2
vld1.16 {d28[], d29[]}, [r1]
vand q8, q8, q9
vshl.u16 q14, q14, q10
vmov.u16 r1, d7[1]
vshl.i16 q14, q14, #0xe
add r1, fp, r1, lsl #1
vadd.i16 q8, q8, q14
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q13, q13, q10
add r1, fp, r1, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #4
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q13, q13, q10
add r1, fp, r1, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #6
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q13, q13, q10
add r1, fp, r1, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #8
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q13, q13, q10
add r1, fp, r1, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #0xa
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r1]
add r1, r8, r2
vshl.u16 q13, q13, q10
vand q13, q13, q9
vshl.i16 q13, q13, #0xc
vadd.i16 q8, q8, q13
vst1.16 {d16[0]}, [r1]
add r1, r7, r2
add r2, r2, #4
vst1.16 {d16[1]}, [r1]
bne .L0x3ab50
b .L0x3ac60

.LDIC0x3ac50:
.quad -1407387768586240

.LDIC0x3ac58:
.quad -3659221942468616

.L0x3ac60:
sub r4, r6, r4
bic r4, r4, r4, asr #31
cmp r4, r0
bls .L0x3af34
add r3, r0, sl
mov r1, ip
str sb, [sp, #0xc]
mov lr, r0
lsl r3, r3, #2
add r2, r8, r3
add r3, r7, r3
stmib sp, {r0, ip}
str r8, [sp, #0x10]
str r7, [sp, #0x14]
str r6, [sp, #0x18]
str r5, [sp, #0x1c]

.L0x3aca0:
mov r0, r1
add r1, r1, #0x40
vorr q8, q12, q12
vld4.16 {d0, d2, d4, d6}, [r0]!
vorr q15, q12, q12
vld4.16 {d1, d3, d5, d7}, [r0]
vorr q14, q12, q12
vmov.u16 r0, d0[1]
vorr q13, q12, q12
add r0, fp, r0, lsl #1
add lr, lr, #4
pld [r1, #0x80]
add sb, r2, #4
vld1.16 {d16[0]}, [r0]
vmov.u16 r0, d2[1]
cmp r4, lr
add r8, r2, #8
add r7, r2, #0xc
add r6, r3, #4
add r0, fp, r0, lsl #1
add r5, r3, #8
add ip, r3, #0xc
vld1.16 {d30[0]}, [r0]
vmov.u16 r0, d4[1]
add r0, fp, r0, lsl #1
vld1.16 {d28[0]}, [r0]
vmov r0, s0
uxth r0, r0
add r0, fp, r0, lsl #1
vld1.16 {d16[1]}, [r0]
vmov r0, s4
uxth r0, r0
add r0, fp, r0, lsl #1
vld1.16 {d30[1]}, [r0]
vmov r0, s8
uxth r0, r0
add r0, fp, r0, lsl #1
vld1.16 {d28[1]}, [r0]
vmov.u16 r0, d0[3]
add r0, fp, r0, lsl #1
vld1.16 {d16[2]}, [r0]
vmov.u16 r0, d2[3]
add r0, fp, r0, lsl #1
vld1.16 {d30[2]}, [r0]
vmov.u16 r0, d4[3]
add r0, fp, r0, lsl #1
vld1.16 {d28[2]}, [r0]
vmov.u16 r0, d6[1]
add r0, fp, r0, lsl #1
vld1.16 {d26[0]}, [r0]
vmov.u16 r0, d0[2]
add r0, fp, r0, lsl #1
vld1.16 {d16[3]}, [r0]
vmov.u16 r0, d2[2]
add r0, fp, r0, lsl #1
vld1.16 {d30[3]}, [r0]
vmov.u16 r0, d4[2]
add r0, fp, r0, lsl #1
vld1.16 {d28[3]}, [r0]
vmov r0, s12
uxth r0, r0
add r0, fp, r0, lsl #1
vld1.16 {d26[1]}, [r0]
vmov.u16 r0, d1[1]
add r0, fp, r0, lsl #1
vld1.16 {d17[0]}, [r0]
vmov.u16 r0, d3[1]
add r0, fp, r0, lsl #1
vld1.16 {d31[0]}, [r0]
vmov.u16 r0, d5[1]
add r0, fp, r0, lsl #1
vld1.16 {d29[0]}, [r0]
vmov.u16 r0, d6[3]
add r0, fp, r0, lsl #1
vld1.16 {d26[2]}, [r0]
vmov.u16 r0, d1[0]
add r0, fp, r0, lsl #1
vld1.16 {d17[1]}, [r0]
vmov.u16 r0, d3[0]
add r0, fp, r0, lsl #1
vld1.16 {d31[1]}, [r0]
vmov.u16 r0, d5[0]
add r0, fp, r0, lsl #1
vld1.16 {d29[1]}, [r0]
vmov.u16 r0, d6[2]
add r0, fp, r0, lsl #1
vld1.16 {d26[3]}, [r0]
vmov.u16 r0, d1[3]
add r0, fp, r0, lsl #1
vld1.16 {d17[2]}, [r0]
vmov.u16 r0, d3[3]
add r0, fp, r0, lsl #1
vld1.16 {d31[2]}, [r0]
vmov.u16 r0, d5[3]
add r0, fp, r0, lsl #1
vld1.16 {d29[2]}, [r0]
vmov.u16 r0, d7[1]
vorr q4, q14, q14
add r0, fp, r0, lsl #1
vld1.16 {d27[0]}, [r0]
vmov.u16 r0, d1[2]
add r0, fp, r0, lsl #1
vld1.16 {d17[3]}, [r0]
vmov.u16 r0, d3[2]
vand q14, q8, q9
add r0, fp, r0, lsl #1
vand q8, q8, q11
vld1.16 {d31[3]}, [r0]
vmov.u16 r0, d5[2]
vand q5, q15, q9
add r0, fp, r0, lsl #1
vand q15, q15, q11
vld1.16 {d9[3]}, [r0]
vmov.u16 r0, d7[0]
vshl.i16 q15, q15, #2
add r0, fp, r0, lsl #1
vshl.i16 q14, q14, #6
vld1.16 {d27[1]}, [r0]
vmov.u16 r0, d7[3]
vshl.i16 q8, q8, #4
add r0, fp, r0, lsl #1
vshl.i16 q5, q5, #4
vld1.16 {d27[2]}, [r0]
vmov.u16 r0, d7[2]
vadd.i16 q8, q8, q15
add r0, fp, r0, lsl #1
vand q15, q4, q9
vld1.16 {d27[3]}, [r0]
vadd.i16 q14, q14, q5
vand q3, q13, q9
vand q4, q4, q11
vand q13, q13, q11
vshl.i16 q15, q15, #2
vadd.i16 q8, q8, q4
vadd.i16 q14, q14, q3
vshr.u16 q13, q13, #2
vadd.i16 q14, q14, q15
vadd.i16 q8, q8, q13
vmovn.i16 d28, q14
vmovn.i16 d16, q8
vst1.16 {d28[0]}, [r2]
vst1.16 {d28[1]}, [sb]
add r2, r2, #0x10
vst1.16 {d28[2]}, [r8]
vst1.16 {d28[3]}, [r7]
vst1.16 {d16[0]}, [r3]
vst1.16 {d16[1]}, [r6]
add r3, r3, #0x10
vst1.16 {d16[2]}, [r5]
vst1.16 {d16[3]}, [ip]
bhi .L0x3aca0
ldmib sp, {r0, ip}
ldr sb, [sp, #0xc]
sub r4, r4, #1
sub r4, r4, r0
add r3, r0, #4
bic r2, r4, #3
lsr r4, r4, #2
ldr r8, [sp, #0x10]
add r4, r4, #1
ldr r7, [sp, #0x14]
ldr r6, [sp, #0x18]
ldr r5, [sp, #0x1c]
add r0, r2, r3
add ip, ip, r4, lsl #6

.L0x3af34:
cmp r0, r6
bhs .L0x3b048
add r2, r0, sl
sub r0, r6, r0
lsl r2, r2, #2
add r1, r8, r2
add r0, ip, r0, lsl #4
add r2, r7, r2

.L0x3af54:
vld1.16 {d6, d7}, [ip]!
vmov.u16 r3, d7[2]
cmp ip, r0
add r3, fp, r3, lsl #1
vld1.16 {d26[], d27[]}, [r3]
vmov.u16 r3, d7[3]
vshl.u16 q13, q13, q10
add r3, fp, r3, lsl #1
vld1.16 {d16[], d17[]}, [r3]
vmov r3, s12
vand q13, q13, q9
uxth r3, r3
vshl.u16 q8, q8, q10
add r3, fp, r3, lsl #1
vshl.i16 q13, q13, #2
vld1.16 {d28[], d29[]}, [r3]
vand q8, q8, q9
vshl.u16 q14, q14, q10
vmov.u16 r3, d7[1]
vshl.i16 q14, q14, #0xe
add r3, fp, r3, lsl #1
vadd.i16 q8, q8, q14
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r3]
vmov.u16 r3, d7[0]
vshl.u16 q13, q13, q10
add r3, fp, r3, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #4
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r3]
vmov.u16 r3, d6[3]
vshl.u16 q13, q13, q10
add r3, fp, r3, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #6
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r3]
vmov.u16 r3, d6[2]
vshl.u16 q13, q13, q10
add r3, fp, r3, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #8
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r3]
vmov.u16 r3, d6[1]
vshl.u16 q13, q13, q10
add r3, fp, r3, lsl #1
vand q13, q13, q9
vshl.i16 q13, q13, #0xa
vadd.i16 q8, q8, q13
vld1.16 {d26[], d27[]}, [r3]
vshl.u16 q13, q13, q10
vand q13, q13, q9
vshl.i16 q13, q13, #0xc
vadd.i16 q8, q8, q13
vst1.16 {d16[0]}, [r1]
vst1.16 {d16[1]}, [r2]
add r1, r1, #4
add r2, r2, #4
bne .L0x3af54

.L0x3b048:
ldr r3, [sp, #0x2c]
add sb, sb, #1
add sl, sl, #0x104
add r5, r5, r3
ldr r3, [sp, #0x24]
cmp sb, r3
bls .L0x3ab08

.L0x3b064:
add sp, sp, #0x34
vpop {d8, d9, d10, d11}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3b070:
sub lr, lr, r4
cmp r5, r2
add r4, lr, #1
streq lr, [sp, #0x24]
udiv r3, r4, r3
mul sb, r3, r2
subne r3, r3, #1
addne r3, r3, sb
strne r3, [sp, #0x24]
b .L0x3aa24

.L0x3b098:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov lr, r1
vpush {d8, d9, d10, d11, d12, d13, d14, d15}
ldr ip, [lr, #0xc]
sub sp, sp, #0x3c
str r1, [sp, #0x28]
ldr r1, [r1, #0x14]
cmp r1, ip
blt .L0x3b100
ldr r4, [lr, #0x10]
sub r5, r3, #1
ldr lr, [lr, #0x18]
cmp r4, lr
ble .L0x3b7c0
mvn r3, #0
str r3, [sp, #0x2c]
mov r3, #0
mov r7, r3

.L0x3b0e0:
ldr r3, [sp, #0x28]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
subge r2, r1, ip
addge r2, r2, #1
movlt r2, #0
b .L0x3b110

.L0x3b100:
mov r2, #0
mvn r3, #0
str r3, [sp, #0x2c]
mov r7, r2

.L0x3b110:
ldr r6, [sp, #0x28]
cmp ip, #0
add r3, ip, #7
movge r3, ip
ldr r4, [r6, #0x2c]
mov lr, #0x10
ldr ip, [sp, #0x2c]
mov sl, r7
ldrsh r1, [r6, #0x28]
cmp ip, r7
ldr ip, [r4, #0xc]
ldr r5, [r4, #0x10]
udiv ip, lr, ip
sdiv r1, r1, ip
ldr ip, [r4, #4]
ldr lr, [r6, #0x10]
mul ip, ip, ip
ldr r4, [r6, #0x3c]
add lr, lr, #3
mla r1, ip, r1, r1
asr r3, r3, #3
add fp, r5, r1, lsl #1
mov r1, #0x104
mla r3, r1, lr, r3
ldr ip, [r4, #0x14]
ldr lr, [r0, #4]
add r3, r3, #0x1a
ldr r4, [r0]
ldr r0, [r0, #8]
lsl r3, r3, #2
add r8, r4, r3
add r7, lr, r3
add sb, r0, r3
blo .L0x3b7b4
vldr d20, .LDIC0x3b328
vldr d21, .LDIC0x3b330
vmov.i16 q9, #3
vmov.i32 q13, #0
vmov.i16 q12, #0xc
vmov.i16 q11, #0x30
lsl r3, ip, #1
mov r5, sl
str r3, [sp, #0x34]
mul r3, r3, r5
mul r6, r1, sl
lsr sl, r2, #3
lsl r2, r2, #1
str r2, [sp, #0x30]
str r3, [sp]

.L0x3b1d4:
ldr r3, [sp, #0x28]
ldr r1, [sp, #0x30]
ldr r2, [r3, #0x44]
ldr r3, [sp]
add r2, r2, r3
ubfx r3, r2, #4, #2
add r0, r2, r1
rsb r3, r3, #0
ubfx r0, r0, #4, #2
and r3, r3, #3
cmp r3, sl
movlo ip, r3
movhs ip, sl
cmp ip, #0
moveq lr, r2
beq .L0x3b338
add lr, r2, ip, lsl #4
mov r3, #0x410
mul r3, r3, r5

.L0x3b220:
vld1.16 {d6, d7}, [r2]!
vmov.u16 r1, d7[2]
cmp r2, lr
add r1, fp, r1, lsl #1
vld1.16 {d28[], d29[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q14, q14, q10
add r1, fp, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q14, q14, q9
uxth r1, r1
vshl.u16 q8, q8, q10
add r1, fp, r1, lsl #1
vshl.i16 q14, q14, #2
vld1.16 {d30[], d31[]}, [r1]
vand q8, q8, q9
vshl.u16 q15, q15, q10
vmov.u16 r1, d7[1]
vshl.i16 q15, q15, #0xe
add r1, fp, r1, lsl #1
vadd.i16 q8, q8, q15
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q14, q14, q10
add r1, fp, r1, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #4
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q14, q14, q10
add r1, fp, r1, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #6
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q14, q14, q10
add r1, fp, r1, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #8
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q14, q14, q10
add r1, fp, r1, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #0xa
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r1]
add r1, r8, r3
vshl.u16 q14, q14, q10
vand q14, q14, q9
vshl.i16 q14, q14, #0xc
vadd.i16 q8, q8, q14
vst1.16 {d16[0]}, [r1]
add r1, r7, r3
vst1.16 {d16[1]}, [r1]
add r1, sb, r3
add r3, r3, #4
vst1.16 {d16[2]}, [r1]
bne .L0x3b220
b .L0x3b338

.LDIC0x3b328:
.quad -1407387768586240

.LDIC0x3b330:
.quad -3659221942468616

.L0x3b338:
sub r3, sl, r0
bic r3, r3, r3, asr #31
cmp r3, ip
str r3, [sp, #4]
bls .L0x3b678
add r3, ip, r6
mov r0, lr
str sl, [sp, #0x20]
mov r4, ip
lsl r3, r3, #2
add r1, r8, r3
add r2, r7, r3
ldr sl, [sp, #4]
add r3, sb, r3
str lr, [sp, #8]
str r5, [sp, #0xc]
str ip, [sp, #0x10]
str sb, [sp, #0x14]
str r8, [sp, #0x18]
str r7, [sp, #0x1c]
str r6, [sp, #0x24]

.L0x3b38c:
mov ip, r0
add r0, r0, #0x40
vorr q14, q13, q13
vld4.16 {d0, d2, d4, d6}, [ip]!
vorr q15, q13, q13
vld4.16 {d1, d3, d5, d7}, [ip]
vorr q4, q13, q13
vmov.u16 ip, d0[1]
vorr q8, q13, q13
add ip, fp, ip, lsl #1
add sb, r1, #4
pld [r0, #0x80]
add r8, r1, #8
vld1.16 {d28[0]}, [ip]
vmov.u16 ip, d2[1]
add r7, r1, #0xc
add r6, r2, #4
add r5, r2, #8
add lr, r2, #0xc
add ip, fp, ip, lsl #1
add r4, r4, #4
cmp sl, r4
vld1.16 {d30[0]}, [ip]
vmov.u16 ip, d4[1]
add ip, fp, ip, lsl #1
vld1.16 {d8[0]}, [ip]
vmov ip, s0
uxth ip, ip
add ip, fp, ip, lsl #1
vld1.16 {d28[1]}, [ip]
vmov ip, s4
uxth ip, ip
add ip, fp, ip, lsl #1
vld1.16 {d30[1]}, [ip]
vmov ip, s8
uxth ip, ip
add ip, fp, ip, lsl #1
vld1.16 {d8[1]}, [ip]
vmov.u16 ip, d0[3]
add ip, fp, ip, lsl #1
vld1.16 {d28[2]}, [ip]
vmov.u16 ip, d2[3]
add ip, fp, ip, lsl #1
vld1.16 {d30[2]}, [ip]
vmov.u16 ip, d4[3]
add ip, fp, ip, lsl #1
vld1.16 {d8[2]}, [ip]
vmov.u16 ip, d6[1]
add ip, fp, ip, lsl #1
vld1.16 {d16[0]}, [ip]
vmov.u16 ip, d0[2]
add ip, fp, ip, lsl #1
vld1.16 {d28[3]}, [ip]
vmov.u16 ip, d2[2]
add ip, fp, ip, lsl #1
vld1.16 {d30[3]}, [ip]
vmov.u16 ip, d4[2]
add ip, fp, ip, lsl #1
vld1.16 {d8[3]}, [ip]
vmov ip, s12
uxth ip, ip
add ip, fp, ip, lsl #1
vld1.16 {d16[1]}, [ip]
vmov.u16 ip, d1[1]
add ip, fp, ip, lsl #1
vld1.16 {d29[0]}, [ip]
vmov.u16 ip, d3[1]
add ip, fp, ip, lsl #1
vld1.16 {d31[0]}, [ip]
vmov.u16 ip, d5[1]
add ip, fp, ip, lsl #1
vld1.16 {d9[0]}, [ip]
vmov.u16 ip, d6[3]
add ip, fp, ip, lsl #1
vld1.16 {d16[2]}, [ip]
vmov.u16 ip, d1[0]
add ip, fp, ip, lsl #1
vld1.16 {d29[1]}, [ip]
vmov.u16 ip, d3[0]
add ip, fp, ip, lsl #1
vld1.16 {d31[1]}, [ip]
vmov.u16 ip, d5[0]
add ip, fp, ip, lsl #1
vld1.16 {d9[1]}, [ip]
vmov.u16 ip, d6[2]
add ip, fp, ip, lsl #1
vld1.16 {d16[3]}, [ip]
vmov.u16 ip, d1[3]
add ip, fp, ip, lsl #1
vld1.16 {d29[2]}, [ip]
vmov.u16 ip, d3[3]
add ip, fp, ip, lsl #1
vld1.16 {d31[2]}, [ip]
vmov.u16 ip, d5[3]
add ip, fp, ip, lsl #1
vld1.16 {d9[2]}, [ip]
vmov.u16 ip, d7[1]
add ip, fp, ip, lsl #1
vld1.16 {d17[0]}, [ip]
vmov.u16 ip, d1[2]
add ip, fp, ip, lsl #1
vld1.16 {d29[3]}, [ip]
vmov.u16 ip, d3[2]
vand q6, q14, q9
add ip, fp, ip, lsl #1
vand q5, q14, q12
vld1.16 {d31[3]}, [ip]
vmov.u16 ip, d5[2]
vand q1, q15, q9
add ip, fp, ip, lsl #1
vand q2, q15, q12
vld1.16 {d9[3]}, [ip]
vmov.u16 ip, d7[0]
vand q14, q14, q11
add ip, fp, ip, lsl #1
vshl.i16 q5, q5, #4
vld1.16 {d17[1]}, [ip]
vmov.u16 ip, d7[3]
vshl.i16 q6, q6, #6
add ip, fp, ip, lsl #1
vshl.i16 q14, q14, #2
vld1.16 {d17[2]}, [ip]
vmov.u16 ip, d7[2]
vand q3, q4, q11
add ip, fp, ip, lsl #1
vshl.i16 q1, q1, #4
vld1.16 {d17[3]}, [ip]
vshl.i16 q2, q2, #2
vorr q7, q8, q8
vshr.u16 q8, q3, #2
vand q0, q4, q9
vand q3, q7, q9
vadd.i16 q2, q5, q2
vand q4, q4, q12
vand q5, q7, q12
vand q15, q15, q11
vadd.i16 q1, q6, q1
vadd.i16 q8, q14, q8
vand q7, q7, q11
vshl.i16 q0, q0, #2
vadd.i16 q3, q1, q3
vadd.i16 q2, q2, q4
vshr.u16 q5, q5, #2
vadd.i16 q8, q8, q15
vshr.u16 q7, q7, #4
vadd.i16 q3, q3, q0
vadd.i16 q2, q2, q5
vadd.i16 q8, q8, q7
vmovn.i16 d6, q3
vmovn.i16 d4, q2
vmovn.i16 d16, q8
add ip, r3, #4
vst1.16 {d6[0]}, [r1]
vst1.16 {d6[1]}, [sb]
vst1.16 {d6[2]}, [r8]
vst1.16 {d6[3]}, [r7]
vst1.16 {d4[0]}, [r2]
vst1.16 {d4[1]}, [r6]
vst1.16 {d4[2]}, [r5]
vst1.16 {d4[3]}, [lr]
vst1.16 {d16[0]}, [r3]
vst1.16 {d16[1]}, [ip]
add ip, r3, #8
add r1, r1, #0x10
add r2, r2, #0x10
vst1.16 {d16[2]}, [ip]
add ip, r3, #0xc
add r3, r3, #0x10
vst1.16 {d16[3]}, [ip]
bhi .L0x3b38c
ldr r3, [sp, #4]
ldr ip, [sp, #0x10]
ldr lr, [sp, #8]
sub r3, r3, #1
sub r3, r3, ip
add r2, ip, #4
bic r1, r3, #3
lsr r3, r3, #2
ldr r5, [sp, #0xc]
add r3, r3, #1
ldr sb, [sp, #0x14]
ldr r8, [sp, #0x18]
ldr r7, [sp, #0x1c]
ldr sl, [sp, #0x20]
ldr r6, [sp, #0x24]
add ip, r1, r2
add lr, lr, r3, lsl #6

.L0x3b678:
cmp ip, sl
bhs .L0x3b790
add r3, ip, r6
sub ip, sl, ip
lsl r3, r3, #2
add ip, lr, ip, lsl #4

.L0x3b690:
vld1.16 {d6, d7}, [lr]!
vmov.u16 r2, d7[2]
cmp lr, ip
add r2, fp, r2, lsl #1
vld1.16 {d28[], d29[]}, [r2]
vmov.u16 r2, d7[3]
vshl.u16 q14, q14, q10
add r2, fp, r2, lsl #1
vld1.16 {d16[], d17[]}, [r2]
vmov r2, s12
vand q14, q14, q9
uxth r2, r2
vshl.u16 q8, q8, q10
add r2, fp, r2, lsl #1
vshl.i16 q14, q14, #2
vld1.16 {d30[], d31[]}, [r2]
vand q8, q8, q9
vshl.u16 q15, q15, q10
vmov.u16 r2, d7[1]
vshl.i16 q15, q15, #0xe
add r2, fp, r2, lsl #1
vadd.i16 q8, q8, q15
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r2]
vmov.u16 r2, d7[0]
vshl.u16 q14, q14, q10
add r2, fp, r2, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #4
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r2]
vmov.u16 r2, d6[3]
vshl.u16 q14, q14, q10
add r2, fp, r2, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #6
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r2]
vmov.u16 r2, d6[2]
vshl.u16 q14, q14, q10
add r2, fp, r2, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #8
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r2]
vmov.u16 r2, d6[1]
vshl.u16 q14, q14, q10
add r2, fp, r2, lsl #1
vand q14, q14, q9
vshl.i16 q14, q14, #0xa
vadd.i16 q8, q8, q14
vld1.16 {d28[], d29[]}, [r2]
add r2, r8, r3
vshl.u16 q14, q14, q10
vand q14, q14, q9
vshl.i16 q14, q14, #0xc
vadd.i16 q8, q8, q14
vst1.16 {d16[0]}, [r2]
add r2, r7, r3
vst1.16 {d16[1]}, [r2]
add r2, sb, r3
add r3, r3, #4
vst1.16 {d16[2]}, [r2]
bne .L0x3b690

.L0x3b790:
ldr r3, [sp]
add r5, r5, #1
ldr r2, [sp, #0x34]
add r6, r6, #0x104
add r3, r3, r2
str r3, [sp]
ldr r3, [sp, #0x2c]
cmp r5, r3
bls .L0x3b1d4

.L0x3b7b4:
add sp, sp, #0x3c
vpop {d8, d9, d10, d11, d12, d13, d14, d15}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3b7c0:
sub lr, lr, r4
cmp r2, r5
add r4, lr, #1
streq lr, [sp, #0x2c]
udiv r3, r4, r3
mul r2, r3, r2
subne r3, r3, #1
addne r3, r3, r2
mov r7, r2
strne r3, [sp, #0x2c]
b .L0x3b0e0

.L0x3b7f0:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov lr, r1
vpush {d8, d9, d10, d11, d12, d13, d14, d15}
ldr ip, [r1, #0x14]
sub sp, sp, #0x44
str r1, [sp, #0x30]
ldr r1, [r1, #0xc]
cmp ip, r1
blt .L0x3b854
ldr r4, [lr, #0x10]
sub r5, r3, #1
ldr lr, [lr, #0x18]
cmp r4, lr
ble .L0x3bf8c
mov r6, #0
mvn r3, #0
str r3, [sp, #0x34]

.L0x3b834:
ldr r3, [sp, #0x30]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
subge r2, ip, r1
addge r2, r2, #1
movlt r2, #0
b .L0x3b864

.L0x3b854:
mov r2, #0
mvn r3, #0
str r3, [sp, #0x34]
mov r6, r2

.L0x3b864:
ldr r7, [sp, #0x30]
cmp r1, #0
add lr, r1, #7
movge lr, r1
ldr ip, [r7, #0x2c]
mov r4, #0x10
ldr r1, [sp, #0x34]
ldrsh r3, [r7, #0x28]
cmp r1, r6
ldr r1, [ip, #0xc]
ldr r5, [ip, #0x10]
udiv r1, r4, r1
sdiv r3, r3, r1
ldr r1, [ip, #4]
ldr r4, [r7, #0x10]
mul r1, r1, r1
asr lr, lr, #3
add r4, r4, #3
mla r3, r1, r3, r3
ldr ip, [r7, #0x3c]
mov r1, #0x104
add fp, r5, r3, lsl #1
mla r3, r1, r4, lr
ldr r5, [r0]
ldr ip, [ip, #0x14]
add r3, r3, #0x1a
ldmib r0, {r4, lr}
ldr r0, [r0, #0xc]
lsl r3, r3, #2
add r8, r5, r3
add r7, r4, r3
add sl, lr, r3
add sb, r0, r3
blo .L0x3bf80
vldr d26, .LDIC0x3ba88
vldr d27, .LDIC0x3ba90
vmov.i16 q12, #3
vmov.i32 q6, #0
vmov.i16 q5, #0xc
vmov.i16 q4, #0x30
lsl r3, ip, #1
mov lr, r6
mul r1, r1, r6
str r3, [sp, #0x3c]
mul r3, r3, r6
lsr r4, r2, #3
lsl r2, r2, #1
str r1, [sp, #0xc]
str r2, [sp, #0x38]
str r3, [sp, #0x10]

.L0x3b92c:
ldr r3, [sp, #0x30]
ldr r1, [sp, #0x38]
ldr r2, [r3, #0x44]
ldr r3, [sp, #0x10]
add r2, r2, r3
ubfx r3, r2, #4, #2
add r0, r2, r1
rsb r3, r3, #0
ubfx r0, r0, #4, #2
and r3, r3, #3
cmp r3, r4
movlo r5, r3
movhs r5, r4
cmp r5, #0
streq r2, [sp, #8]
beq .L0x3ba98
add ip, r2, r5, lsl #4
mov r3, #0x410
mul r3, r3, lr
str ip, [sp, #8]

.L0x3b97c:
vld1.16 {d6, d7}, [r2]!
vmov.u16 r1, d7[2]
cmp r2, ip
add r1, fp, r1, lsl #1
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q9, q9, q13
add r1, fp, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q9, q9, q12
uxth r1, r1
vshl.u16 q8, q8, q13
add r1, fp, r1, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r1]
vand q8, q8, q12
vshl.u16 q10, q10, q13
vmov.u16 r1, d7[1]
vshl.i16 q10, q10, #0xe
add r1, fp, r1, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q9, q9, q13
add r1, fp, r1, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q9, q9, q13
add r1, fp, r1, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q9, q9, q13
add r1, fp, r1, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q9, q9, q13
add r1, fp, r1, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
add r1, r8, r3
vshl.u16 q9, q9, q13
vand q9, q9, q12
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r1]
add r1, r7, r3
vst1.16 {d16[1]}, [r1]
add r1, sl, r3
vst1.16 {d16[2]}, [r1]
add r1, sb, r3
add r3, r3, #4
vst1.16 {d16[3]}, [r1]
bne .L0x3b97c
b .L0x3ba98

.LDIC0x3ba88:
.quad -1407387768586240

.LDIC0x3ba90:
.quad -3659221942468616

.L0x3ba98:
sub r3, r4, r0
bic r3, r3, r3, asr #31
cmp r3, r5
str r3, [sp, #4]
bls .L0x3be2c
ldr r3, [sp, #0xc]
mov r6, r5
ldr ip, [sp, #8]
add r3, r5, r3
str r5, [sp, #0x14]
lsl r3, r3, #2
add r0, r8, r3
add r1, r7, r3
add r2, sl, r3
add r3, sb, r3
str sl, [sp, #0x18]
str sb, [sp, #0x1c]
str r8, [sp, #0x20]
str r7, [sp, #0x24]
str lr, [sp, #0x28]
str r4, [sp, #0x2c]

.L0x3baec:
ldr lr, [sp, #4]
add r6, r6, #4
vorr q11, q6, q6
cmp lr, r6
mov lr, ip
vorr q8, q6, q6
vld4.16 {d0, d2, d4, d6}, [lr]!
vorr q10, q6, q6
vld4.16 {d1, d3, d5, d7}, [lr]
vorr q9, q6, q6
vmov.u16 lr, d0[1]
add ip, ip, #0x40
add sl, r0, #4
add sb, r0, #8
add r8, r0, #0xc
add lr, fp, lr, lsl #1
add r7, r1, #4
pld [ip, #0x80]
add r5, r1, #8
vld1.16 {d22[0]}, [lr]
vmov.u16 lr, d2[1]
add r4, r1, #0xc
add lr, fp, lr, lsl #1
vld1.16 {d16[0]}, [lr]
vmov.u16 lr, d4[1]
add lr, fp, lr, lsl #1
vld1.16 {d20[0]}, [lr]
vmov lr, s0
uxth lr, lr
add lr, fp, lr, lsl #1
vld1.16 {d22[1]}, [lr]
vmov lr, s4
uxth lr, lr
add lr, fp, lr, lsl #1
vld1.16 {d16[1]}, [lr]
vmov lr, s8
uxth lr, lr
add lr, fp, lr, lsl #1
vld1.16 {d20[1]}, [lr]
vmov.u16 lr, d0[3]
add lr, fp, lr, lsl #1
vld1.16 {d22[2]}, [lr]
vmov.u16 lr, d2[3]
add lr, fp, lr, lsl #1
vld1.16 {d16[2]}, [lr]
vmov.u16 lr, d4[3]
add lr, fp, lr, lsl #1
vld1.16 {d20[2]}, [lr]
vmov.u16 lr, d6[1]
add lr, fp, lr, lsl #1
vld1.16 {d18[0]}, [lr]
vmov.u16 lr, d0[2]
add lr, fp, lr, lsl #1
vld1.16 {d22[3]}, [lr]
vmov.u16 lr, d2[2]
add lr, fp, lr, lsl #1
vld1.16 {d16[3]}, [lr]
vmov.u16 lr, d4[2]
add lr, fp, lr, lsl #1
vld1.16 {d20[3]}, [lr]
vmov lr, s12
uxth lr, lr
add lr, fp, lr, lsl #1
vld1.16 {d18[1]}, [lr]
vmov.u16 lr, d1[1]
add lr, fp, lr, lsl #1
vld1.16 {d23[0]}, [lr]
vmov.u16 lr, d3[1]
add lr, fp, lr, lsl #1
vld1.16 {d17[0]}, [lr]
vmov.u16 lr, d5[1]
add lr, fp, lr, lsl #1
vld1.16 {d21[0]}, [lr]
vmov.u16 lr, d6[3]
add lr, fp, lr, lsl #1
vld1.16 {d18[2]}, [lr]
vmov.u16 lr, d1[0]
add lr, fp, lr, lsl #1
vld1.16 {d23[1]}, [lr]
vmov.u16 lr, d3[0]
add lr, fp, lr, lsl #1
vld1.16 {d17[1]}, [lr]
vmov.u16 lr, d5[0]
add lr, fp, lr, lsl #1
vld1.16 {d21[1]}, [lr]
vmov.u16 lr, d6[2]
add lr, fp, lr, lsl #1
vld1.16 {d18[3]}, [lr]
vmov.u16 lr, d1[3]
add lr, fp, lr, lsl #1
vld1.16 {d23[2]}, [lr]
vmov.u16 lr, d3[3]
add lr, fp, lr, lsl #1
vld1.16 {d17[2]}, [lr]
vmov.u16 lr, d5[3]
add lr, fp, lr, lsl #1
vld1.16 {d21[2]}, [lr]
vmov.u16 lr, d7[1]
add lr, fp, lr, lsl #1
vld1.16 {d19[0]}, [lr]
vmov.u16 lr, d1[2]
add lr, fp, lr, lsl #1
vld1.16 {d23[3]}, [lr]
vmov.u16 lr, d3[2]
vand q7, q11, q12
add lr, fp, lr, lsl #1
vand q1, q11, q4
vld1.16 {d17[3]}, [lr]
vmov.u16 lr, d5[2]
vand q0, q8, q5
add lr, fp, lr, lsl #1
vand q14, q8, q12
vld1.16 {d21[3]}, [lr]
vmov.u16 lr, d7[0]
vand q15, q10, q4
add lr, fp, lr, lsl #1
vshl.i16 q0, q0, #2
vld1.16 {d19[1]}, [lr]
vmov.u16 lr, d7[3]
vshl.i16 q7, q7, #6
add lr, fp, lr, lsl #1
vshl.i16 q1, q1, #2
vld1.16 {d19[2]}, [lr]
vmov.u16 lr, d7[2]
vand q3, q11, q5
vshl.i16 q14, q14, #4
vshl.i16 q3, q3, #4
vshr.u16 q15, q15, #2
vadd.i16 q3, q3, q0
vmov.i16 q0, #0xc0
vadd.i16 q15, q1, q15
vand q2, q10, q12
vadd.i16 q14, q7, q14
vand q1, q8, q4
vand q7, q10, q5
vand q8, q8, q0
vand q10, q10, q0
add lr, fp, lr, lsl #1
vshr.u16 q8, q8, #2
vld1.16 {d19[3]}, [lr]
vshr.u16 q10, q10, #4
vadd.i16 q7, q3, q7
vadd.i16 q1, q15, q1
vand q3, q9, q5
vand q15, q9, q4
vadd.i16 q8, q8, q10
vand q10, q9, q0
vand q9, q9, q12
vshl.i16 q2, q2, #2
vshr.u16 q3, q3, #2
vshr.u16 q15, q15, #4
vadd.i16 q9, q14, q9
vand q11, q11, q0
vadd.i16 q7, q7, q3
vadd.i16 q1, q1, q15
vadd.i16 q9, q9, q2
vadd.i16 q8, q8, q11
vshr.u16 q10, q10, #6
vmovn.i16 d14, q7
vmovn.i16 d2, q1
vmovn.i16 d18, q9
add lr, r2, #4
vadd.i16 q8, q8, q10
vst1.16 {d18[0]}, [r0]
vst1.16 {d18[1]}, [sl]
vst1.16 {d18[2]}, [sb]
vst1.16 {d18[3]}, [r8]
vst1.16 {d14[0]}, [r1]
vst1.16 {d14[1]}, [r7]
vst1.16 {d14[2]}, [r5]
vst1.16 {d14[3]}, [r4]
vst1.16 {d2[0]}, [r2]
vst1.16 {d2[1]}, [lr]
add lr, r2, #8
add r0, r0, #0x10
vmovn.i16 d16, q8
vst1.16 {d2[2]}, [lr]
add lr, r2, #0xc
add r1, r1, #0x10
add r2, r2, #0x10
vst1.16 {d2[3]}, [lr]
add lr, r3, #4
vst1.16 {d16[0]}, [r3]
vst1.16 {d16[1]}, [lr]
add lr, r3, #8
vst1.16 {d16[2]}, [lr]
add lr, r3, #0xc
add r3, r3, #0x10
vst1.16 {d16[3]}, [lr]
bhi .L0x3baec
ldr r3, [sp, #4]
ldr r5, [sp, #0x14]
ldr sl, [sp, #0x18]
sub r3, r3, #1
sub r3, r3, r5
add r2, r5, #4
bic r1, r3, #3
lsr r3, r3, #2
add r5, r1, r2
add r3, r3, #1
ldr r2, [sp, #8]
ldr sb, [sp, #0x1c]
ldr r8, [sp, #0x20]
ldr r7, [sp, #0x24]
ldr lr, [sp, #0x28]
ldr r4, [sp, #0x2c]
add r3, r2, r3, lsl #6
str r3, [sp, #8]

.L0x3be2c:
cmp r5, r4
bhs .L0x3bf54
ldr r3, [sp, #0xc]
ldr r1, [sp, #8]
add r3, r5, r3
sub r5, r4, r5
lsl r3, r3, #2
add r5, r1, r5, lsl #4

.L0x3be4c:
vld1.16 {d6, d7}, [r1]!
vmov.u16 r2, d7[2]
cmp r1, r5
add r2, fp, r2, lsl #1
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d7[3]
vshl.u16 q9, q9, q13
add r2, fp, r2, lsl #1
vld1.16 {d16[], d17[]}, [r2]
vmov r2, s12
vand q9, q9, q12
uxth r2, r2
vshl.u16 q8, q8, q13
add r2, fp, r2, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r2]
vand q8, q8, q12
vshl.u16 q10, q10, q13
vmov.u16 r2, d7[1]
vshl.i16 q10, q10, #0xe
add r2, fp, r2, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d7[0]
vshl.u16 q9, q9, q13
add r2, fp, r2, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[3]
vshl.u16 q9, q9, q13
add r2, fp, r2, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[2]
vshl.u16 q9, q9, q13
add r2, fp, r2, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[1]
vshl.u16 q9, q9, q13
add r2, fp, r2, lsl #1
vand q9, q9, q12
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
add r2, r8, r3
vshl.u16 q9, q9, q13
vand q9, q9, q12
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r2]
add r2, r7, r3
vst1.16 {d16[1]}, [r2]
add r2, sl, r3
vst1.16 {d16[2]}, [r2]
add r2, sb, r3
add r3, r3, #4
vst1.16 {d16[3]}, [r2]
bne .L0x3be4c

.L0x3bf54:
ldr r3, [sp, #0x10]
add lr, lr, #1
ldr r2, [sp, #0x3c]
add r3, r3, r2
str r3, [sp, #0x10]
ldr r3, [sp, #0x34]
cmp lr, r3
ldr r3, [sp, #0xc]
add r3, r3, #0x104
str r3, [sp, #0xc]
bls .L0x3b92c

.L0x3bf80:
add sp, sp, #0x44
vpop {d8, d9, d10, d11, d12, d13, d14, d15}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3bf8c:
sub lr, lr, r4
cmp r2, r5
add r4, lr, #1
streq lr, [sp, #0x34]
udiv r3, r4, r3
mul r6, r3, r2
subne r3, r3, #1
addne r3, r3, r6
strne r3, [sp, #0x34]
b .L0x3b834

.L0x3bfb8:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov lr, r1
vpush {d8, d9, d10, d11, d12, d13, d14, d15}
ldr ip, [r1, #0x14]
sub sp, sp, #0x6c
str r1, [sp, #0x58]
ldr r1, [r1, #0xc]
cmp ip, r1
blt .L0x3c020
ldr r4, [lr, #0x10]
sub r5, r3, #1
ldr lr, [lr, #0x18]
cmp r4, lr
ble .L0x3c830
mvn r3, #0
str r3, [sp, #0x5c]
mov r3, #0
str r3, [sp, #0x3c]

.L0x3c000:
ldr r3, [sp, #0x58]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
subge r2, ip, r1
addge r2, r2, #1
movlt r2, #0
b .L0x3c030

.L0x3c020:
mov r2, #0
mvn r3, #0
str r3, [sp, #0x5c]
str r2, [sp, #0x3c]

.L0x3c030:
ldr r5, [sp, #0x58]
cmp r1, #0
ldr r6, [sp, #0x5c]
add lr, r1, #7
ldr ip, [r5, #0x2c]
movge lr, r1
ldr r1, [sp, #0x3c]
mov r4, #0x10
ldrsh r3, [r5, #0x28]
cmp r6, r1
mov r6, r5
ldr r1, [ip, #0xc]
asr lr, lr, #3
udiv r1, r4, r1
sdiv r3, r3, r1
ldr r1, [ip, #4]
ldr r4, [r5, #0x10]
mul r1, r1, r1
ldr r5, [ip, #0x10]
add r4, r4, #3
mla r3, r1, r3, r3
ldr ip, [r6, #0x3c]
mov r1, #0x104
add r7, r5, r3, lsl #1
mla r3, r1, r4, lr
ldmib r0, {r4, lr}
ldr r5, [r0]
add r3, r3, #0x1a
ldr ip, [ip, #0x14]
lsl r3, r3, #2
add fp, lr, r3
ldr lr, [r0, #0xc]
ldr r0, [r0, #0x10]
add r8, r5, r3
add sl, lr, r3
add r5, r4, r3
add sb, r0, r3
blo .L0x3c824
ldr r3, [sp, #0x3c]
mov r0, r5
vldr d28, .LDIC0x3c280
vldr d29, .LDIC0x3c288
vmov.i16 q13, #3
vmov.i32 q5, #0
vmov.i16 q4, #0xc
vmov.i16 q15, #0x30
vmov.i16 q7, #0xc0
lsl ip, ip, #1
mul r1, r1, r3
mul r3, ip, r3
lsr r4, r2, #3
lsl r2, r2, #1
str ip, [sp, #0x64]
str r1, [sp, #0x34]
str r2, [sp, #0x60]
str r3, [sp, #0x38]
str r4, [sp, #0x30]

.L0x3c114:
ldr r3, [sp, #0x58]
ldr r2, [sp, #0x60]
ldr lr, [r3, #0x44]
ldr r3, [sp, #0x38]
add lr, lr, r3
ubfx r3, lr, #4, #2
add r6, lr, r2
rsb r3, r3, #0
ldr r2, [sp, #0x30]
and r3, r3, #3
ubfx r6, r6, #4, #2
cmp r3, r2
movlo r2, r3
str r2, [sp, #0x2c]
cmp r2, #0
moveq r2, lr
beq .L0x3c290
ldr r3, [sp, #0x3c]
mov r1, #0x410
add r2, lr, r2, lsl #4
mul r3, r1, r3

.L0x3c168:
vld1.16 {d6, d7}, [lr]!
vmov.u16 r1, d7[2]
cmp lr, r2
add r1, r7, r1, lsl #1
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q9, q9, q14
add r1, r7, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q9, q9, q13
uxth r1, r1
vshl.u16 q8, q8, q14
add r1, r7, r1, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r1]
vand q8, q8, q13
vshl.u16 q10, q10, q14
vmov.u16 r1, d7[1]
vshl.i16 q10, q10, #0xe
add r1, r7, r1, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q9, q9, q14
add r1, r7, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q9, q9, q14
add r1, r7, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q9, q9, q14
add r1, r7, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q9, q9, q14
add r1, r7, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
add r1, r8, r3
vshl.u16 q9, q9, q14
vand q9, q9, q13
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r1]
add r1, r0, r3
vst1.16 {d16[1]}, [r1]
add r1, fp, r3
vst1.16 {d16[2]}, [r1]
add r1, sl, r3
vst1.16 {d16[3]}, [r1]
add r1, sb, r3
add r3, r3, #4
vst1.16 {d17[0]}, [r1]
bne .L0x3c168
b .L0x3c290

.LDIC0x3c280:
.quad -1407387768586240

.LDIC0x3c288:
.quad -3659221942468616

.L0x3c290:
ldr r3, [sp, #0x30]
sub r3, r3, r6
bic r3, r3, r3, asr #31
str r3, [sp, #0x28]
mov r1, r3
ldr r3, [sp, #0x2c]
cmp r1, r3
bls .L0x3c6b4
ldr r1, [sp, #0x34]
str fp, [sp, #0x40]
add r3, r3, r1
str sl, [sp, #0x44]
lsl r3, r3, #2
add lr, r8, r3
add r4, r0, r3
add ip, fp, r3
add r5, sl, r3
add r6, sb, r3
ldr r3, [sp, #0x2c]
str sb, [sp, #0x48]
str r8, [sp, #0x4c]
mov r1, r3
str r0, [sp, #0x50]
str r2, [sp, #0x54]

.L0x3c2f0:
ldr r3, [sp, #0x28]
add r1, r1, #4
vorr q8, q5, q5
cmp r3, r1
mov r3, r2
add r2, r2, #0x40
add r8, lr, #8
vld4.16 {d0, d2, d4, d6}, [r3]!
pld [r2, #0x80]
add sb, lr, #0xc
vorr q10, q5, q5
vld4.16 {d1, d3, d5, d7}, [r3]
str r8, [sp, #0x14]
add r3, lr, #4
str sb, [sp, #0x18]
mov r0, r3
vmov r8, s0
add r3, r4, #0xc
str r3, [sp, #0x24]
add sl, r4, #4
vmov.u16 r3, d0[1]
vorr q9, q5, q5
add r3, r7, r3, lsl #1
add fp, r4, #8
vorr q11, q5, q5
vld1.16 {d16[0]}, [r3]
vmov.u16 r3, d2[1]
str sl, [sp, #0x1c]
add sb, r5, #4
str fp, [sp, #0x20]
add sl, ip, #0xc
add r3, r7, r3, lsl #1
add fp, ip, #8
vld1.16 {d20[0]}, [r3]
vmov.u16 r3, d4[1]
add r3, r7, r3, lsl #1
vld1.16 {d18[0]}, [r3]
uxth r3, r8
vmov r8, s4
add r3, r7, r3, lsl #1
vld1.16 {d16[1]}, [r3]
uxth r3, r8
vmov r8, s8
add r3, r7, r3, lsl #1
vld1.16 {d20[1]}, [r3]
uxth r3, r8
vmov r8, s12
add r3, r7, r3, lsl #1
vld1.16 {d18[1]}, [r3]
vmov.u16 r3, d0[3]
add r3, r7, r3, lsl #1
vld1.16 {d16[2]}, [r3]
vmov.u16 r3, d2[3]
add r3, r7, r3, lsl #1
vld1.16 {d20[2]}, [r3]
vmov.u16 r3, d4[3]
add r3, r7, r3, lsl #1
vld1.16 {d18[2]}, [r3]
vmov.u16 r3, d6[1]
add r3, r7, r3, lsl #1
vld1.16 {d22[0]}, [r3]
vmov.u16 r3, d0[2]
add r3, r7, r3, lsl #1
vld1.16 {d16[3]}, [r3]
vmov.u16 r3, d2[2]
add r3, r7, r3, lsl #1
vld1.16 {d20[3]}, [r3]
vmov.u16 r3, d4[2]
add r3, r7, r3, lsl #1
vld1.16 {d18[3]}, [r3]
uxth r3, r8
add r8, r5, #8
add r3, r7, r3, lsl #1
vld1.16 {d22[1]}, [r3]
vmov.u16 r3, d1[1]
add r3, r7, r3, lsl #1
vld1.16 {d17[0]}, [r3]
vmov.u16 r3, d3[1]
add r3, r7, r3, lsl #1
vld1.16 {d21[0]}, [r3]
vmov.u16 r3, d5[1]
add r3, r7, r3, lsl #1
vld1.16 {d19[0]}, [r3]
vmov.u16 r3, d6[3]
add r3, r7, r3, lsl #1
vld1.16 {d22[2]}, [r3]
vmov.u16 r3, d1[0]
add r3, r7, r3, lsl #1
vld1.16 {d17[1]}, [r3]
vmov.u16 r3, d3[0]
add r3, r7, r3, lsl #1
vld1.16 {d21[1]}, [r3]
vmov.u16 r3, d5[0]
add r3, r7, r3, lsl #1
vld1.16 {d19[1]}, [r3]
vmov.u16 r3, d6[2]
add r3, r7, r3, lsl #1
vld1.16 {d22[3]}, [r3]
vmov.u16 r3, d1[3]
add r3, r7, r3, lsl #1
vld1.16 {d17[2]}, [r3]
vmov.u16 r3, d3[3]
add r3, r7, r3, lsl #1
vld1.16 {d21[2]}, [r3]
vmov.u16 r3, d5[3]
add r3, r7, r3, lsl #1
vld1.16 {d19[2]}, [r3]
vmov.u16 r3, d7[1]
add r3, r7, r3, lsl #1
vld1.16 {d23[0]}, [r3]
vmov.u16 r3, d1[2]
add r3, r7, r3, lsl #1
vld1.16 {d17[3]}, [r3]
vmov.u16 r3, d3[2]
vand q6, q8, q13
add r3, r7, r3, lsl #1
vand q12, q8, q15
vld1.16 {d21[3]}, [r3]
vmov.u16 r3, d5[2]
vand q0, q10, q4
add r3, r7, r3, lsl #1
vand q2, q10, q13
vld1.16 {d19[3]}, [r3]
vmov.u16 r3, d7[0]
vshl.i16 q6, q6, #6
add r3, r7, r3, lsl #1
vshl.i16 q0, q0, #2
vld1.16 {d23[1]}, [r3]
vmov.u16 r3, d7[3]
vshl.i16 q2, q2, #4
add r3, r7, r3, lsl #1
vand q1, q9, q13
vld1.16 {d23[2]}, [r3]
vmov.u16 r3, d7[2]
vand q3, q8, q4
add r3, r7, r3, lsl #1
vadd.i16 q2, q6, q2
vshl.i16 q3, q3, #4
vld1.16 {d23[3]}, [r3]
vadd.i16 q3, q3, q0
vshl.i16 q6, q12, #2
vand q0, q9, q15
vand q12, q9, q4
vshr.u16 q0, q0, #2
vadd.i16 q3, q3, q12
vand q12, q11, q4
vadd.i16 q6, q6, q0
vshr.u16 q12, q12, #2
vand q0, q10, q15
vadd.i16 q3, q3, q12
vadd.i16 q6, q6, q0
vmov.i16 q0, #0x300
vmovn.i16 d25, q3
vand q3, q10, q7
vand q10, q10, q0
vst1.64 {d4, d5}, [sp:0x40]
vorr q2, q0, q0
vshr.u16 q0, q10, #4
vand q10, q11, q15
vshr.u16 q3, q3, #2
vshr.u16 q10, q10, #4
vshl.i16 q1, q1, #2
vadd.i16 q6, q6, q10
add r3, r6, #0xc
vmovn.i16 d21, q6
vand q6, q9, q7
vand q9, q9, q2
vshr.u16 q6, q6, #4
vshr.u16 q9, q9, #6
vadd.i16 q3, q3, q6
vand q6, q8, q7
vand q8, q8, q2
vadd.i16 q3, q3, q6
vshr.u16 q8, q8, #2
vand q6, q11, q7
vadd.i16 q8, q8, q0
vshr.u16 q6, q6, #6
vadd.i16 q8, q8, q9
vand q9, q11, q2
vand q11, q11, q13
vld1.64 {d4, d5}, [sp:0x40]
vadd.i16 q2, q2, q11
vshr.u16 q11, q9, #8
vadd.i16 q2, q2, q1
vadd.i16 q9, q3, q6
vmovn.i16 d4, q2
vadd.i16 q8, q8, q11
vst1.16 {d4[0]}, [lr]
vst1.16 {d4[1]}, [r0]
ldr r0, [sp, #0x14]
add lr, lr, #0x10
vmovn.i16 d18, q9
vst1.16 {d4[2]}, [r0]
ldr r0, [sp, #0x18]
vmovn.i16 d16, q8
vst1.16 {d4[3]}, [r0]
ldr r0, [sp, #0x1c]
vst1.16 {d25[0]}, [r4]
vst1.16 {d25[1]}, [r0]
ldr r0, [sp, #0x20]
add r4, r4, #0x10
vst1.16 {d25[2]}, [r0]
ldr r0, [sp, #0x24]
vst1.16 {d25[3]}, [r0]
add r0, ip, #4
vst1.16 {d21[0]}, [ip]
vst1.16 {d21[1]}, [r0]
add r0, r5, #0xc
add ip, ip, #0x10
vst1.16 {d21[2]}, [fp]
vst1.16 {d21[3]}, [sl]
vst1.16 {d18[0]}, [r5]
vst1.16 {d18[1]}, [sb]
vst1.16 {d18[2]}, [r8]
vst1.16 {d18[3]}, [r0]
add r0, r6, #4
add r5, r5, #0x10
vst1.16 {d16[0]}, [r6]
vst1.16 {d16[1]}, [r0]
add r0, r6, #8
add r6, r6, #0x10
vst1.16 {d16[2]}, [r0]
vst1.16 {d16[3]}, [r3]
bhi .L0x3c2f0
ldr r3, [sp, #0x28]
ldr r1, [sp, #0x2c]
ldr r2, [sp, #0x54]
sub r3, r3, #1
sub r3, r3, r1
add r1, r1, #4
bic lr, r3, #3
lsr r3, r3, #2
ldr fp, [sp, #0x40]
add r3, r3, #1
ldr sl, [sp, #0x44]
ldr sb, [sp, #0x48]
ldr r8, [sp, #0x4c]
ldr r0, [sp, #0x50]
add r2, r2, r3, lsl #6
add r1, lr, r1
str r1, [sp, #0x2c]

.L0x3c6b4:
ldr r3, [sp, #0x2c]
ldr r1, [sp, #0x30]
cmp r3, r1
bhs .L0x3c7f0
ldr r1, [sp, #0x2c]
ldr r3, [sp, #0x34]
ldr ip, [sp, #0x30]
add r3, r1, r3
sub r1, ip, r1
lsl r3, r3, #2
add r1, r2, r1, lsl #4

.L0x3c6e0:
vld1.16 {d6, d7}, [r2]!
vmov.u16 ip, d7[2]
cmp r2, r1
add ip, r7, ip, lsl #1
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d7[3]
vshl.u16 q9, q9, q14
add ip, r7, ip, lsl #1
vld1.16 {d16[], d17[]}, [ip]
vmov ip, s12
vand q9, q9, q13
uxth ip, ip
vshl.u16 q8, q8, q14
add ip, r7, ip, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [ip]
vand q8, q8, q13
vshl.u16 q10, q10, q14
vmov.u16 ip, d7[1]
vshl.i16 q10, q10, #0xe
add ip, r7, ip, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d7[0]
vshl.u16 q9, q9, q14
add ip, r7, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d6[3]
vshl.u16 q9, q9, q14
add ip, r7, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d6[2]
vshl.u16 q9, q9, q14
add ip, r7, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d6[1]
vshl.u16 q9, q9, q14
add ip, r7, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
add ip, r8, r3
vshl.u16 q9, q9, q14
vand q9, q9, q13
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [ip]
add ip, r0, r3
vst1.16 {d16[1]}, [ip]
add ip, fp, r3
vst1.16 {d16[2]}, [ip]
add ip, sl, r3
vst1.16 {d16[3]}, [ip]
add ip, sb, r3
add r3, r3, #4
vst1.16 {d17[0]}, [ip]
bne .L0x3c6e0

.L0x3c7f0:
ldr r3, [sp, #0x38]
ldr r2, [sp, #0x64]
add r3, r3, r2
str r3, [sp, #0x38]
ldr r3, [sp, #0x3c]
ldr r2, [sp, #0x5c]
add r3, r3, #1
cmp r3, r2
str r3, [sp, #0x3c]
ldr r3, [sp, #0x34]
add r3, r3, #0x104
str r3, [sp, #0x34]
bls .L0x3c114

.L0x3c824:
add sp, sp, #0x6c
vpop {d8, d9, d10, d11, d12, d13, d14, d15}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3c830:
sub lr, lr, r4
cmp r2, r5
add r4, lr, #1
streq lr, [sp, #0x5c]
udiv r3, r4, r3
mul r2, r3, r2
subne r3, r3, #1
addne r3, r3, r2
str r2, [sp, #0x3c]
strne r3, [sp, #0x5c]
b .L0x3c000

.L0x3c860:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r6, r1
ldr ip, [r1, #0x14]
ldr r1, [r1, #0xc]
vpush {d8, d9, d10, d11, d12, d13, d14, d15}
cmp ip, r1
sub sp, sp, #0x7c
blt .L0x3c8b8
ldr r4, [r6, #0x10]
sub r5, r3, #1
ldr lr, [r6, #0x18]
cmp r4, lr
mvngt r8, #0
movgt r5, #0
ble .L0x3d19c

.L0x3c89c:
ldr r3, [r6, #0x10]
ldr r2, [r6, #0x18]
cmp r2, r3
subge r3, ip, r1
addge r3, r3, #1
movlt r3, #0
b .L0x3c8c4

.L0x3c8b8:
mov r3, #0
mvn r8, #0
mov r5, r3

.L0x3c8c4:
ldr ip, [r6, #0x2c]
cmp r1, #0
ldrsh r2, [r6, #0x28]
add lr, r1, #7
ldr r7, [ip, #0x10]
movge lr, r1
ldr r1, [ip, #0xc]
mov r4, #0x10
asr lr, lr, #3
udiv r1, r4, r1
sdiv r2, r2, r1
ldr r1, [ip, #4]
ldr r4, [r6, #0x10]
mul r1, r1, r1
ldr ip, [r6, #0x3c]
add r4, r4, #3
mla r2, r1, r2, r2
cmp r8, r5
mov r1, #0x104
add fp, r7, r2, lsl #1
mla r2, r1, r4, lr
ldmib r0, {r4, lr}
ldr r7, [r0]
add r2, r2, #0x1a
ldr ip, [ip, #0x14]
lsl r2, r2, #2
add lr, lr, r2
str lr, [sp, #0x28]
ldr lr, [r0, #0xc]
add sl, r7, r2
add r7, lr, r2
ldr lr, [r0, #0x10]
ldr r0, [r0, #0x14]
add r4, r4, r2
add sb, lr, r2
add r2, r0, r2
str sl, [sp, #0x34]
str r4, [sp, #0x24]
str r7, [sp, #0x2c]
str sb, [sp, #0x30]
str r2, [sp, #0x38]
blo .L0x3d190
mul r4, r1, r5
lsr lr, r3, #3
lsl r1, r3, #1
ldr r3, [sp, #0x24]
str r1, [sp, #0x70]
add r1, sl, #4
str r1, [sp, #0xc]
add r1, r3, #4
ldr r3, [sp, #0x28]
str r1, [sp, #0x40]
str r8, [sp, #0x74]
add r1, r3, #4
ldr r3, [sp, #0x2c]
mov r8, r6
str r1, [sp, #0x44]
mov r6, r5
add r1, r3, #4
vldr d30, .LDIC0x3cb60
vldr d31, .LDIC0x3cb68
ldr r3, [sp, #0x30]
vmov.i16 q14, #3
lsl sb, ip, #1
add r3, r3, #4
mul r7, sb, r5
str r3, [sp, #0x48]
mov r5, r4
str r1, [sp, #0x20]
mov r4, lr
add r3, r2, #4
str r3, [sp, #0x4c]

.L0x3c9e4:
ldr r1, [r8, #0x44]
ldr r2, [sp, #0x70]
add r1, r1, r7
ubfx r3, r1, #4, #2
add ip, r1, r2
rsb r3, r3, #0
ubfx ip, ip, #4, #2
and r3, r3, #3
cmp r3, r4
movlo lr, r3
movhs lr, r4
cmp lr, #0
moveq r2, r1
beq .L0x3cb70
add r2, r1, lr, lsl #4
mov r3, #0x410
mul r3, r3, r6

.L0x3ca28:
vld1.16 {d6, d7}, [r1]!
vmov.u16 r0, d7[2]
cmp r1, r2
add r0, fp, r0, lsl #1
vld1.16 {d18[], d19[]}, [r0]
vmov.u16 r0, d7[3]
vshl.u16 q9, q9, q15
add r0, fp, r0, lsl #1
vld1.16 {d16[], d17[]}, [r0]
vmov r0, s12
vand q9, q9, q14
uxth r0, r0
vshl.u16 q8, q8, q15
add r0, fp, r0, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r0]
vand q8, q8, q14
vshl.u16 q10, q10, q15
vmov.u16 r0, d7[1]
vshl.i16 q10, q10, #0xe
add r0, fp, r0, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r0]
vmov.u16 r0, d7[0]
vshl.u16 q9, q9, q15
add r0, fp, r0, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r0]
vmov.u16 r0, d6[3]
vshl.u16 q9, q9, q15
add r0, fp, r0, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r0]
vmov.u16 r0, d6[2]
vshl.u16 q9, q9, q15
add r0, fp, r0, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r0]
vmov.u16 r0, d6[1]
vshl.u16 q9, q9, q15
add r0, fp, r0, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r0]
ldr r0, [sp, #0x34]
vshl.u16 q9, q9, q15
add r0, r0, r3
vand q9, q9, q14
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r0]
ldr r0, [sp, #0x24]
add r0, r0, r3
vst1.16 {d16[1]}, [r0]
ldr r0, [sp, #0x28]
add r0, r0, r3
vst1.16 {d16[2]}, [r0]
ldr r0, [sp, #0x2c]
add r0, r0, r3
vst1.16 {d16[3]}, [r0]
ldr r0, [sp, #0x30]
add r0, r0, r3
vst1.16 {d17[0]}, [r0]
ldr r0, [sp, #0x38]
add r0, r0, r3
add r3, r3, #4
vst1.16 {d17[1]}, [r0]
bne .L0x3ca28
b .L0x3cb70

.LDIC0x3cb60:
.quad -1407387768586240

.LDIC0x3cb68:
.quad -3659221942468616

.L0x3cb70:
sub r3, r4, ip
bic r3, r3, r3, asr #31
cmp r3, lr
str r3, [sp, #0x3c]
bls .L0x3d018
add r3, lr, r5
vmov.i32 q6, #0
lsl r0, r3, #2
add r3, r5, #2
add r3, r3, lr
vmov.i16 q5, #0xc
vmov.i16 q4, #0x30
lsl r1, r3, #2
stmib sp, {r2, lr}
str lr, [sp, #0x50]
str r6, [sp, #0x54]
str r2, [sp, #0x58]
str r4, [sp, #0x5c]
str r7, [sp, #0x60]
str sb, [sp, #0x64]
str r5, [sp, #0x68]
str r8, [sp, #0x6c]

.L0x3cbc8:
ldr r3, [sp, #8]
ldr lr, [sp, #4]
ldr ip, [sp, #0x3c]
add r3, r3, #4
str r3, [sp, #8]
cmp ip, r3
mov r3, lr
vorr q11, q6, q6
vld4.16 {d0, d2, d4, d6}, [r3]!
vorr q8, q6, q6
vld4.16 {d1, d3, d5, d7}, [r3]
vorr q10, q6, q6
vmov.u16 r3, d0[1]
vorr q9, q6, q6
add r3, fp, r3, lsl #1
add lr, lr, #0x40
ldr r2, [sp, #0x34]
vld1.16 {d22[0]}, [r3]
vmov.u16 r3, d2[1]
ldr ip, [sp, #0xc]
pld [lr, #0x80]
add r3, fp, r3, lsl #1
str lr, [sp, #4]
vld1.16 {d16[0]}, [r3]
vmov.u16 r3, d4[1]
vmov.i16 q7, #0xc0
add r3, fp, r3, lsl #1
add sl, r2, r0
vld1.16 {d20[0]}, [r3]
vmov r3, s0
add sb, ip, r0
add r8, r2, r1
uxth r3, r3
add r3, fp, r3, lsl #1
vld1.16 {d22[1]}, [r3]
vmov r3, s4
uxth r3, r3
add r3, fp, r3, lsl #1
vld1.16 {d16[1]}, [r3]
vmov r3, s8
uxth r3, r3
add r3, fp, r3, lsl #1
vld1.16 {d20[1]}, [r3]
vmov.u16 r3, d0[3]
add r3, fp, r3, lsl #1
vld1.16 {d22[2]}, [r3]
vmov.u16 r3, d2[3]
add r3, fp, r3, lsl #1
vld1.16 {d16[2]}, [r3]
vmov.u16 r3, d4[3]
add r3, fp, r3, lsl #1
vld1.16 {d20[2]}, [r3]
vmov.u16 r3, d6[1]
add r3, fp, r3, lsl #1
vld1.16 {d18[0]}, [r3]
vmov.u16 r3, d0[2]
add r3, fp, r3, lsl #1
vld1.16 {d22[3]}, [r3]
vmov.u16 r3, d2[2]
add r3, fp, r3, lsl #1
vld1.16 {d16[3]}, [r3]
vmov.u16 r3, d4[2]
add r3, fp, r3, lsl #1
vld1.16 {d20[3]}, [r3]
vmov r3, s12
uxth r3, r3
add r3, fp, r3, lsl #1
vld1.16 {d18[1]}, [r3]
vmov.u16 r3, d1[1]
add r3, fp, r3, lsl #1
vld1.16 {d23[0]}, [r3]
vmov.u16 r3, d3[1]
add r3, fp, r3, lsl #1
vld1.16 {d17[0]}, [r3]
vmov.u16 r3, d5[1]
add r3, fp, r3, lsl #1
vld1.16 {d21[0]}, [r3]
vmov.u16 r3, d6[3]
add r3, fp, r3, lsl #1
vld1.16 {d18[2]}, [r3]
vmov.u16 r3, d1[0]
add r3, fp, r3, lsl #1
vld1.16 {d23[1]}, [r3]
vmov.u16 r3, d3[0]
add r3, fp, r3, lsl #1
vld1.16 {d17[1]}, [r3]
vmov.u16 r3, d5[0]
add r3, fp, r3, lsl #1
vld1.16 {d21[1]}, [r3]
vmov.u16 r3, d6[2]
add r3, fp, r3, lsl #1
vld1.16 {d18[3]}, [r3]
vmov.u16 r3, d1[3]
add r3, fp, r3, lsl #1
vld1.16 {d23[2]}, [r3]
vmov.u16 r3, d3[3]
ldr r2, [sp, #0x44]
add r3, fp, r3, lsl #1
add r6, r2, r0
vld1.16 {d17[2]}, [r3]
vmov.u16 r3, d5[3]
add r4, r2, r1
ldr r2, [sp, #0x20]
add r3, fp, r3, lsl #1
add ip, r2, r0
vld1.16 {d21[2]}, [r3]
vmov.u16 r3, d7[1]
add r3, fp, r3, lsl #1
vld1.16 {d19[0]}, [r3]
vmov.u16 r3, d1[2]
add r3, fp, r3, lsl #1
vld1.16 {d23[3]}, [r3]
vmov.u16 r3, d3[2]
vand q13, q11, q5
add r3, fp, r3, lsl #1
vand q1, q11, q4
vld1.16 {d17[3]}, [r3]
vmov.u16 r3, d5[2]
vand q12, q8, q5
add r3, fp, r3, lsl #1
vand q2, q8, q14
vld1.16 {d21[3]}, [r3]
vmov.u16 r3, d7[0]
vshl.i16 q12, q12, #2
add r3, fp, r3, lsl #1
vshl.i16 q13, q13, #4
vld1.16 {d19[1]}, [r3]
vmov.u16 r3, d7[3]
vshl.i16 q2, q2, #4
add r3, fp, r3, lsl #1
vadd.i16 q13, q13, q12
vld1.16 {d19[2]}, [r3]
vmov.u16 r3, d7[2]
vand q3, q11, q14
add r3, fp, r3, lsl #1
vand q12, q10, q5
vld1.16 {d19[3]}, [r3]
vshl.i16 q3, q3, #6
vadd.i16 q13, q13, q12
vadd.i16 q3, q3, q2
vand q12, q9, q5
vand q2, q10, q4
vshr.u16 q12, q12, #2
vshr.u16 q2, q2, #2
vshl.i16 q1, q1, #2
vadd.i16 q13, q13, q12
vadd.i16 q1, q1, q2
vand q12, q8, q7
vand q2, q10, q7
vstr d6, [sp, #0x10]
vstr d7, [sp, #0x18]
vshr.u16 q2, q2, #4
vmov.i16 q3, #0x300
vshr.u16 q12, q12, #2
vand q7, q8, q3
vadd.i16 q12, q12, q2
vand q2, q11, q3
vshr.u16 q7, q7, #4
vmov.i16 q3, #0xc00
vshr.u16 q2, q2, #2
vand q0, q10, q14
vadd.i16 q2, q2, q7
vand q7, q11, q3
vmov.i16 q3, #0xc0
vshr.u16 q7, q7, #4
vand q11, q11, q3
vshl.i16 q0, q0, #2
vadd.i16 q11, q12, q11
vmov.i16 q12, #0xc00
vmovn.i16 d26, q13
vorr q3, q12, q12
vand q12, q8, q12
vand q8, q8, q4
vshr.u16 q12, q12, #6
vadd.i16 q1, q1, q8
vadd.i16 q12, q7, q12
vmov.i16 q7, #0x300
ldr r3, [sp, #0x28]
vand q8, q10, q7
vand q10, q10, q3
vshr.u16 q8, q8, #6
vshr.u16 q10, q10, #8
vadd.i16 q2, q2, q8
vand q8, q9, q3
vadd.i16 q12, q12, q10
vshr.u16 q8, q8, #0xa
vand q10, q9, q14
vadd.i16 q12, q12, q8
add r7, r3, r0
add r5, r3, r1
vmovn.i16 d17, q12
vldr d24, [sp, #0x10]
vldr d25, [sp, #0x18]
ldr r3, [sp, #0x2c]
vadd.i16 q3, q12, q10
vand q12, q9, q4
vadd.i16 q3, q3, q0
vshr.u16 q12, q12, #4
vmovn.i16 d6, q3
vadd.i16 q1, q1, q12
vst1.16 {d6[0]}, [sl]
vst1.16 {d6[1]}, [sb]
vst1.16 {d6[2]}, [r8]
ldr r8, [sp, #0xc]
ldr sb, [sp, #0x24]
add r8, r8, r1
ldr sl, [sp, #0x40]
vst1.16 {d6[3]}, [r8]
vmov.i16 q3, #0xc0
add r8, sb, r0
vmovn.i16 d2, q1
vand q10, q9, q3
vand q9, q9, q7
vshr.u16 q10, q10, #6
vshr.u16 q9, q9, #8
vadd.i16 q11, q11, q10
vst1.16 {d26[0]}, [r8]
add r8, sl, r0
vmovn.i16 d22, q11
vst1.16 {d26[1]}, [r8]
vadd.i16 q2, q2, q9
add r8, sb, r1
add r2, r3, r1
vst1.16 {d26[2]}, [r8]
add lr, r3, r0
add r8, sl, r1
ldr r3, [sp, #0x20]
vst1.16 {d26[3]}, [r8]
vmovn.i16 d4, q2
vst1.16 {d2[0]}, [r7]
add r3, r3, r1
vst1.16 {d2[1]}, [r6]
vst1.16 {d2[2]}, [r5]
vst1.16 {d2[3]}, [r4]
vst1.16 {d22[0]}, [lr]
vst1.16 {d22[1]}, [ip]
vst1.16 {d22[2]}, [r2]
ldr r2, [sp, #0x30]
vst1.16 {d22[3]}, [r3]
ldr ip, [sp, #0x48]
add r3, r0, r2
vst1.16 {d4[0]}, [r3]
add r3, ip, r0
vst1.16 {d4[1]}, [r3]
add r3, r1, r2
ldr r2, [sp, #0x38]
vst1.16 {d4[2]}, [r3]
add r3, ip, r1
ldr ip, [sp, #0x4c]
vst1.16 {d4[3]}, [r3]
add r3, r0, r2
vst1.16 {d17[0]}, [r3]
add r3, ip, r0
add r0, r0, #0x10
vst1.16 {d17[1]}, [r3]
add r3, r1, r2
vst1.16 {d17[2]}, [r3]
add r3, ip, r1
add r1, r1, #0x10
vst1.16 {d17[3]}, [r3]
bhi .L0x3cbc8
ldr r3, [sp, #0x3c]
add r2, sp, #0x58
ldr lr, [sp, #0x50]
ldr r6, [sp, #0x54]
sub r3, r3, #1
sub r3, r3, lr
add r1, lr, #4
bic r0, r3, #3
ldm r2, {r2, r4, r7, sb}
lsr r3, r3, #2
ldr r5, [sp, #0x68]
add r3, r3, #1
ldr r8, [sp, #0x6c]
add lr, r0, r1
add r2, r2, r3, lsl #6

.L0x3d018:
cmp lr, r4
bhs .L0x3d178
sub r0, r4, lr
add r3, lr, r5
str r6, [sp, #4]
str r4, [sp, #8]
str r7, [sp, #0x10]
lsl r3, r3, #2
add r0, r2, r0, lsl #4
ldr ip, [sp, #0x34]
ldr lr, [sp, #0x24]
ldr sl, [sp, #0x38]
ldr r6, [sp, #0x2c]
ldr r4, [sp, #0x28]
ldr r7, [sp, #0x30]

.L0x3d054:
vld1.16 {d6, d7}, [r2]!
vmov.u16 r1, d7[2]
cmp r2, r0
add r1, fp, r1, lsl #1
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q9, q9, q15
add r1, fp, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q9, q9, q14
uxth r1, r1
vshl.u16 q8, q8, q15
add r1, fp, r1, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r1]
vand q8, q8, q14
vshl.u16 q10, q10, q15
vmov.u16 r1, d7[1]
vshl.i16 q10, q10, #0xe
add r1, fp, r1, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q9, q9, q15
add r1, fp, r1, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q9, q9, q15
add r1, fp, r1, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q9, q9, q15
add r1, fp, r1, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q9, q9, q15
add r1, fp, r1, lsl #1
vand q9, q9, q14
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
add r1, ip, r3
vshl.u16 q9, q9, q15
vand q9, q9, q14
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r1]
add r1, lr, r3
vst1.16 {d16[1]}, [r1]
add r1, r4, r3
vst1.16 {d16[2]}, [r1]
add r1, r6, r3
vst1.16 {d16[3]}, [r1]
add r1, r7, r3
vst1.16 {d17[0]}, [r1]
add r1, sl, r3
add r3, r3, #4
vst1.16 {d17[1]}, [r1]
bne .L0x3d054
ldr r6, [sp, #4]
ldr r4, [sp, #8]
ldr r7, [sp, #0x10]

.L0x3d178:
ldr r3, [sp, #0x74]
add r6, r6, #1
add r7, r7, sb
add r5, r5, #0x104
cmp r6, r3
bls .L0x3c9e4

.L0x3d190:
add sp, sp, #0x7c
vpop {d8, d9, d10, d11, d12, d13, d14, d15}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3d19c:
sub lr, lr, r4
cmp r2, r5
add r4, lr, #1
moveq r8, lr
udiv r3, r4, r3
mul r5, r3, r2
subne r3, r3, #1
addne r8, r3, r5
b .L0x3c89c

.L0x3d1c0:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov lr, r1
vpush {d8, d9, d10, d11, d12, d13, d14, d15}
ldr ip, [r1, #0x14]
sub sp, sp, #0x7c
str r1, [sp, #0x68]
ldr r1, [r1, #0xc]
cmp ip, r1
blt .L0x3d228
ldr r4, [lr, #0x10]
sub r5, r3, #1
ldr lr, [lr, #0x18]
cmp r4, lr
ble .L0x3dbb0
mvn r3, #0
str r3, [sp, #0x6c]
mov r3, #0
str r3, [sp, #0x54]

.L0x3d208:
ldr r3, [sp, #0x68]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
subge r3, ip, r1
addge r3, r3, #1
movlt r3, #0
b .L0x3d238

.L0x3d228:
mov r3, #0
mvn r2, #0
str r2, [sp, #0x6c]
str r3, [sp, #0x54]

.L0x3d238:
ldr r6, [sp, #0x68]
cmp r1, #0
ldr r7, [sp, #0x54]
add lr, r1, #7
ldr ip, [r6, #0x2c]
movge lr, r1
ldr r1, [sp, #0x6c]
mov r4, #0x10
ldrsh r2, [r6, #0x28]
cmp r1, r7
ldr r1, [ip, #0xc]
ldr r5, [ip, #0x10]
udiv r1, r4, r1
sdiv r2, r2, r1
ldr r1, [ip, #4]
ldr r4, [r6, #0x10]
mul r1, r1, r1
asr lr, lr, #3
add r4, r4, #3
mla r2, r1, r2, r2
ldr ip, [r6, #0x3c]
mov r1, #0x104
add sb, r5, r2, lsl #1
mla r2, r1, r4, lr
ldmib r0, {r4, lr}
ldr r5, [r0]
add r2, r2, #0x1a
ldr ip, [ip, #0x14]
lsl r2, r2, #2
add lr, lr, r2
str lr, [sp, #4]
ldr lr, [r0, #0xc]
add r5, r5, r2
add lr, lr, r2
str lr, [sp, #0x3c]
ldr lr, [r0, #0x10]
add r4, r4, r2
add lr, lr, r2
str lr, [sp, #0x40]
ldr lr, [r0, #0x14]
ldr r0, [r0, #0x18]
add lr, lr, r2
add r2, r0, r2
str r5, [sp, #0x34]
str r4, [sp, #0x38]
str lr, [sp, #0x44]
str r2, [sp, #0x48]
blo .L0x3dba4
mov r2, #0x410
vldr d28, .LDIC0x3d4e0
vldr d29, .LDIC0x3d4e8
vmov.i16 q13, #3
vmov.i32 q5, #0
lsl r0, ip, #1
mul sl, r2, r7
lsr r2, r3, #3
lsl r3, r3, #1
mul fp, r1, r7
str r3, [sp, #0x70]
mul r3, r0, r7
str r0, [sp, #0x74]
mov r7, sb
str r2, [sp, #0x4c]
str r3, [sp, #0x50]

.L0x3d338:
ldr r3, [sp, #0x68]
ldr r2, [sp, #0x50]
ldr r3, [r3, #0x44]
ldr r1, [sp, #0x70]
add r3, r3, r2
add r1, r3, r1
ubfx r1, r1, #4, #2
ubfx r2, r3, #4, #2
str r1, [sp, #8]
rsb r2, r2, #0
ldr r1, [sp, #0x4c]
and r2, r2, #3
cmp r2, r1
movlo sb, r2
movhs sb, r1
cmp sb, #0
moveq r8, r3
beq .L0x3d4f0
ldr r2, [sp, #0x34]
add r8, r3, sb, lsl #4
add r6, r2, sl
ldr r2, [sp, #0x38]
add r5, r2, sl
ldr r2, [sp, #4]
add r4, r2, sl
ldr r2, [sp, #0x3c]
add lr, r2, sl
ldr r2, [sp, #0x40]
add ip, r2, sl
ldr r2, [sp, #0x44]
add r0, r2, sl
ldr r2, [sp, #0x48]
add r1, r2, sl

.L0x3d3bc:
vld1.16 {d6, d7}, [r3]!
vmov.u16 r2, d7[2]
cmp r3, r8
add r2, r7, r2, lsl #1
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d7[3]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vld1.16 {d16[], d17[]}, [r2]
vmov r2, s12
vand q9, q9, q13
uxth r2, r2
vshl.u16 q8, q8, q14
add r2, r7, r2, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r2]
vand q8, q8, q13
vshl.u16 q10, q10, q14
vmov.u16 r2, d7[1]
vshl.i16 q10, q10, #0xe
add r2, r7, r2, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d7[0]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[3]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[2]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[1]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vshl.u16 q9, q9, q14
vand q9, q9, q13
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r6]
vst1.16 {d16[1]}, [r5]
add r6, r6, #4
add r5, r5, #4
vst1.16 {d16[2]}, [r4]
vst1.16 {d16[3]}, [lr]
add r4, r4, #4
add lr, lr, #4
vst1.16 {d17[0]}, [ip]
vst1.16 {d17[1]}, [r0]
add ip, ip, #4
add r0, r0, #4
vst1.16 {d17[2]}, [r1]
add r1, r1, #4
bne .L0x3d3bc
b .L0x3d4f0

.LDIC0x3d4e0:
.quad -1407387768586240

.LDIC0x3d4e8:
.quad -3659221942468616

.L0x3d4f0:
ldr r3, [sp, #0x4c]
ldr r2, [sp, #8]
sub r3, r3, r2
bic r3, r3, r3, asr #31
cmp r3, sb
str r3, [sp, #0x30]
bls .L0x3da00
add r3, sb, fp
strd r8, sb, [sp, #8]
lsl r1, r3, #2
add r3, fp, #1
add r3, r3, sb
str sb, [sp, #0x58]
lsl r2, r3, #2
add r3, fp, #2
add r3, r3, sb
str r8, [sp, #0x5c]
lsl r5, r3, #2
add r3, fp, #3
add r3, r3, sb
str fp, [sp, #0x60]
lsl r6, r3, #2
str sl, [sp, #0x64]

.L0x3d54c:
ldr r3, [sp, #0xc]
ldr r0, [sp, #0x30]
add r3, r3, #4
cmp r0, r3
ldr r0, [sp, #8]
str r3, [sp, #0xc]
mov r3, r0
add r0, r0, #0x40
vorr q11, q5, q5
vld4.16 {d0, d2, d4, d6}, [r3]!
vorr q8, q5, q5
vld4.16 {d1, d3, d5, d7}, [r3]
vorr q9, q5, q5
vmov.u16 r3, d0[1]
vmov.i16 q15, #0xc
add r3, r7, r3, lsl #1
vmov.u16 fp, d6[1]
vld1.16 {d22[0]}, [r3]
vmov.u16 r3, d2[1]
vmov.u16 sb, d6[3]
add r3, r7, r3, lsl #1
vmov.u16 r8, d6[2]
vld1.16 {d16[0]}, [r3]
vmov.u16 r3, d4[1]
vmov.u16 r4, d7[1]
add r3, r7, r3, lsl #1
vmov.u16 lr, d7[0]
vld1.16 {d18[0]}, [r3]
vmov r3, s0
vmov.u16 ip, d7[3]
pld [r0, #0x80]
uxth r3, r3
str r0, [sp, #8]
add r3, r7, r3, lsl #1
vmov.u16 r0, d7[2]
vld1.16 {d22[1]}, [r3]
vmov r3, s4
vmov.i16 q4, #0x30
uxth r3, r3
vmov.i16 q7, #0x300
add r3, r7, r3, lsl #1
add fp, r7, fp, lsl #1
vld1.16 {d16[1]}, [r3]
vmov r3, s8
add sb, r7, sb, lsl #1
add r8, r7, r8, lsl #1
uxth r3, r3
add r4, r7, r4, lsl #1
add r3, r7, r3, lsl #1
add lr, r7, lr, lsl #1
vld1.16 {d18[1]}, [r3]
vmov.u16 r3, d0[3]
add ip, r7, ip, lsl #1
add r0, r7, r0, lsl #1
add r3, r7, r3, lsl #1
vld1.16 {d22[2]}, [r3]
vmov.u16 r3, d2[3]
add r3, r7, r3, lsl #1
vld1.16 {d16[2]}, [r3]
vmov.u16 r3, d4[3]
add r3, r7, r3, lsl #1
vld1.16 {d18[2]}, [r3]
vmov.u16 r3, d0[2]
add r3, r7, r3, lsl #1
vld1.16 {d22[3]}, [r3]
vmov.u16 r3, d2[2]
add r3, r7, r3, lsl #1
vld1.16 {d16[3]}, [r3]
vmov.u16 r3, d4[2]
add r3, r7, r3, lsl #1
vld1.16 {d18[3]}, [r3]
vmov r3, s12
uxth sl, r3
vmov.u16 r3, d1[1]
add sl, r7, sl, lsl #1
add r3, r7, r3, lsl #1
vld1.16 {d23[0]}, [r3]
vmov.u16 r3, d3[1]
add r3, r7, r3, lsl #1
vld1.16 {d17[0]}, [r3]
vmov.u16 r3, d5[1]
add r3, r7, r3, lsl #1
vld1.16 {d19[0]}, [r3]
vmov.u16 r3, d1[0]
add r3, r7, r3, lsl #1
vld1.16 {d23[1]}, [r3]
vmov.u16 r3, d3[0]
add r3, r7, r3, lsl #1
vld1.16 {d17[1]}, [r3]
vmov.u16 r3, d5[0]
add r3, r7, r3, lsl #1
vld1.16 {d19[1]}, [r3]
vmov.u16 r3, d1[3]
add r3, r7, r3, lsl #1
vld1.16 {d23[2]}, [r3]
vmov.u16 r3, d3[3]
add r3, r7, r3, lsl #1
vld1.16 {d17[2]}, [r3]
vmov.u16 r3, d5[3]
add r3, r7, r3, lsl #1
vld1.16 {d19[2]}, [r3]
vmov.u16 r3, d1[2]
add r3, r7, r3, lsl #1
vld1.16 {d23[3]}, [r3]
vmov.u16 r3, d3[2]
vand q3, q11, q15
add r3, r7, r3, lsl #1
vand q0, q11, q4
vld1.16 {d17[3]}, [r3]
vmov.u16 r3, d5[2]
vand q15, q8, q15
vshl.i16 q3, q3, #4
vshl.i16 q15, q15, #2
add r3, r7, r3, lsl #1
vmov.i16 q1, #0xc0
vld1.16 {d19[3]}, [r3]
vadd.i16 q2, q3, q15
vshl.i16 q0, q0, #2
vstr d4, [sp, #0x10]
vstr d5, [sp, #0x18]
vand q2, q9, q4
vand q15, q8, q1
vshr.u16 q2, q2, #2
vshr.u16 q15, q15, #2
vadd.i16 q0, q0, q2
vand q2, q9, q1
vand q1, q11, q7
vshr.u16 q2, q2, #4
vmov.i16 q3, #0xc00
vadd.i16 q15, q15, q2
vand q2, q8, q7
vshr.u16 q1, q1, #2
vshr.u16 q2, q2, #4
vand q7, q8, q3
vadd.i16 q1, q1, q2
vand q2, q11, q3
vshr.u16 q7, q7, #6
vmov.i16 q3, #0x3000
vshr.u16 q2, q2, #4
vand q12, q11, q13
vadd.i16 q2, q2, q7
vand q7, q11, q3
vmov.i16 q3, #0xc0
vand q10, q8, q13
vand q11, q11, q3
vshl.i16 q10, q10, #4
vadd.i16 q11, q15, q11
vmov.i16 q15, #0x3000
vshl.i16 q12, q12, #6
vorr q3, q15, q15
vand q15, q8, q15
vadd.i16 q12, q12, q10
vshr.u16 q7, q7, #6
vorr q10, q5, q5
vshr.u16 q15, q15, #8
vld1.16 {d20[0]}, [fp]
vadd.i16 q15, q7, q15
vand q8, q8, q4
vmov.i16 q7, #0x300
vld1.16 {d20[1]}, [sl]
vadd.i16 q8, q0, q8
vand q0, q9, q7
vld1.16 {d20[2]}, [sb]
vshr.u16 q0, q0, #6
vld1.16 {d20[3]}, [r8]
vadd.i16 q1, q1, q0
vmov.i16 q0, #0xc00
vld1.16 {d21[0]}, [r4]
vand q0, q9, q0
vld1.16 {d21[1]}, [lr]
vshr.u16 q0, q0, #8
vld1.16 {d21[2]}, [ip]
vadd.i16 q2, q2, q0
vand q0, q9, q3
vld1.16 {d21[3]}, [r0]
vshr.u16 q0, q0, #0xa
vand q6, q9, q13
vadd.i16 q0, q15, q0
vand q15, q10, q13
vshl.i16 q6, q6, #2
vadd.i16 q12, q12, q15
ldr ip, [sp, #0x34]
vmov.i16 q3, #0xc
vadd.i16 q12, q12, q6
add r0, r1, ip
vstr d16, [sp, #0x20]
vstr d17, [sp, #0x28]
vmovn.i16 d24, q12
vldr d16, [sp, #0x10]
vldr d17, [sp, #0x18]
vst1.16 {d24[0]}, [r0]
add r0, r2, ip
vand q9, q9, q3
vst1.16 {d24[1]}, [r0]
add r0, r5, ip
vadd.i16 q9, q8, q9
vst1.16 {d24[2]}, [r0]
add r0, r6, ip
ldr ip, [sp, #0x38]
vst1.16 {d24[3]}, [r0]
vand q12, q10, q3
add r0, r1, ip
vldr d16, [sp, #0x20]
vldr d17, [sp, #0x28]
vshr.u16 q12, q12, #2
ldr r3, [sp, #4]
vmov.i16 q3, #0xc0
vadd.i16 q9, q9, q12
add r3, r1, r3
vmovn.i16 d18, q9
vst1.16 {d18[0]}, [r0]
add r0, r2, ip
vst1.16 {d18[1]}, [r0]
add r0, r5, ip
vst1.16 {d18[2]}, [r0]
add r0, r6, ip
vst1.16 {d18[3]}, [r0]
vand q9, q10, q4
ldr r0, [sp, #4]
vshr.u16 q9, q9, #4
vadd.i16 q8, q8, q9
vand q9, q10, q7
vmovn.i16 d16, q8
vshr.u16 q9, q9, #8
vst1.16 {d16[0]}, [r3]
add r3, r2, r0
vadd.i16 q1, q1, q9
vst1.16 {d16[1]}, [r3]
add r3, r5, r0
vmovn.i16 d2, q1
vst1.16 {d16[2]}, [r3]
add r3, r6, r0
ldr r0, [sp, #0x3c]
vst1.16 {d16[3]}, [r3]
vand q8, q10, q3
add r3, r1, r0
vmov.i16 q3, #0xc00
vshr.u16 q8, q8, #6
vadd.i16 q11, q11, q8
vand q8, q10, q3
vmovn.i16 d22, q11
vmov.i16 q3, #0x3000
vst1.16 {d22[0]}, [r3]
add r3, r2, r0
vshr.u16 q8, q8, #0xa
vst1.16 {d22[1]}, [r3]
add r3, r5, r0
vand q10, q10, q3
vst1.16 {d22[2]}, [r3]
add r3, r6, r0
vadd.i16 q2, q2, q8
vst1.16 {d22[3]}, [r3]
ldr r0, [sp, #0x40]
vmovn.i16 d4, q2
add r3, r1, r0
vshr.u16 q10, q10, #0xc
vst1.16 {d2[0]}, [r3]
add r3, r2, r0
vadd.i16 q0, q0, q10
vst1.16 {d2[1]}, [r3]
add r3, r5, r0
vmovn.i16 d0, q0
vst1.16 {d2[2]}, [r3]
add r3, r6, r0
ldr r0, [sp, #0x44]
vst1.16 {d2[3]}, [r3]
add r3, r1, r0
vst1.16 {d4[0]}, [r3]
add r3, r2, r0
vst1.16 {d4[1]}, [r3]
add r3, r5, r0
vst1.16 {d4[2]}, [r3]
add r3, r6, r0
ldr r0, [sp, #0x48]
vst1.16 {d4[3]}, [r3]
add r3, r1, r0
add r1, r1, #0x10
vst1.16 {d0[0]}, [r3]
add r3, r2, r0
add r2, r2, #0x10
vst1.16 {d0[1]}, [r3]
add r3, r5, r0
add r5, r5, #0x10
vst1.16 {d0[2]}, [r3]
add r3, r6, r0
add r6, r6, #0x10
vst1.16 {d0[3]}, [r3]
bhi .L0x3d54c
ldr r3, [sp, #0x30]
ldr sb, [sp, #0x58]
ldr r8, [sp, #0x5c]
sub r3, r3, #1
sub r3, r3, sb
add r2, sb, #4
bic r1, r3, #3
lsr r3, r3, #2
ldr fp, [sp, #0x60]
add r3, r3, #1
ldr sl, [sp, #0x64]
add sb, r1, r2
add r8, r8, r3, lsl #6

.L0x3da00:
ldr r3, [sp, #0x4c]
cmp sb, r3
bhs .L0x3db74
ldr r2, [sp, #0x4c]
add r3, sb, fp
sub r6, r2, sb
ldr r2, [sp, #0x34]
lsl r3, r3, #2
add r5, r2, r3
ldr r2, [sp, #0x38]
add r6, r8, r6, lsl #4
add r4, r2, r3
ldr r2, [sp, #4]
add lr, r2, r3
ldr r2, [sp, #0x3c]
add ip, r2, r3
ldr r2, [sp, #0x40]
add r0, r2, r3
ldr r2, [sp, #0x44]
add r1, r2, r3
ldr r2, [sp, #0x48]
add r3, r2, r3

.L0x3da58:
vld1.16 {d6, d7}, [r8]!
vmov.u16 r2, d7[2]
cmp r8, r6
add r2, r7, r2, lsl #1
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d7[3]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vld1.16 {d16[], d17[]}, [r2]
vmov r2, s12
vand q9, q9, q13
uxth r2, r2
vshl.u16 q8, q8, q14
add r2, r7, r2, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r2]
vand q8, q8, q13
vshl.u16 q10, q10, q14
vmov.u16 r2, d7[1]
vshl.i16 q10, q10, #0xe
add r2, r7, r2, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d7[0]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[3]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[2]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vmov.u16 r2, d6[1]
vshl.u16 q9, q9, q14
add r2, r7, r2, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r2]
vshl.u16 q9, q9, q14
vand q9, q9, q13
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r5]
vst1.16 {d16[1]}, [r4]
add r5, r5, #4
add r4, r4, #4
vst1.16 {d16[2]}, [lr]
vst1.16 {d16[3]}, [ip]
add lr, lr, #4
add ip, ip, #4
vst1.16 {d17[0]}, [r0]
vst1.16 {d17[1]}, [r1]
add r0, r0, #4
add r1, r1, #4
vst1.16 {d17[2]}, [r3]
add r3, r3, #4
bne .L0x3da58

.L0x3db74:
ldr r3, [sp, #0x50]
add fp, fp, #0x104
ldr r2, [sp, #0x74]
add sl, sl, #0x410
add r3, r3, r2
str r3, [sp, #0x50]
ldr r3, [sp, #0x54]
ldr r2, [sp, #0x6c]
add r3, r3, #1
cmp r3, r2
str r3, [sp, #0x54]
bls .L0x3d338

.L0x3dba4:
add sp, sp, #0x7c
vpop {d8, d9, d10, d11, d12, d13, d14, d15}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3dbb0:
sub lr, lr, r4
cmp r2, r5
add r4, lr, #1
streq lr, [sp, #0x6c]
udiv r3, r4, r3
mul r2, r2, r3
subne r3, r3, #1
addne r3, r3, r2
str r2, [sp, #0x54]
strne r3, [sp, #0x6c]
b .L0x3d208

.L0x3dbe0:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov sl, r1
ldr ip, [r1, #0x14]
ldr r1, [r1, #0xc]
vpush {d8, d9, d10, d11, d12, d13, d14, d15}
cmp ip, r1
sub sp, sp, #0x84
blt .L0x3dc40
ldr r4, [sl, #0x10]
sub r5, r3, #1
ldr lr, [sl, #0x18]
cmp r4, lr
ble .L0x3e654
mvn r3, #0
str r3, [sp, #0x70]
mov r3, #0
str r3, [sp, #0x64]

.L0x3dc24:
ldr r2, [sl, #0x18]
ldr r3, [sl, #0x10]
cmp r2, r3
subge r2, ip, r1
addge r2, r2, #1
movlt r2, #0
b .L0x3dc50

.L0x3dc40:
mov r2, #0
mvn r3, #0
str r3, [sp, #0x70]
str r2, [sp, #0x64]

.L0x3dc50:
ldr ip, [sl, #0x2c]
cmp r1, #0
ldr r5, [sp, #0x70]
add lr, r1, #7
ldrsh r3, [sl, #0x28]
movge lr, r1
ldr r1, [sp, #0x64]
mov r4, #0x10
asr lr, lr, #3
cmp r5, r1
ldr r1, [ip, #0xc]
ldr r5, [ip, #0x10]
udiv r1, r4, r1
sdiv r3, r3, r1
ldr r1, [ip, #4]
ldr r4, [sl, #0x10]
mul r1, r1, r1
ldr ip, [sl, #0x3c]
add r4, r4, #3
mla r3, r1, r3, r3
ldr r8, [ip, #0x14]
mov r1, #0x104
add r5, r5, r3, lsl #1
mla r3, r1, r4, lr
ldmib r0, {r4, lr}
ldr ip, [r0]
add r3, r3, #0x1a
lsl r3, r3, #2
add fp, lr, r3
ldr lr, [r0, #0xc]
add sb, ip, r3
add r7, lr, r3
ldr lr, [r0, #0x10]
add ip, r4, r3
add r6, lr, r3
ldr lr, [r0, #0x14]
add r4, lr, r3
ldr lr, [r0, #0x18]
ldr r0, [r0, #0x1c]
add lr, lr, r3
add r0, r0, r3
str lr, [sp, #0x4c]
str r0, [sp, #0x54]
blo .L0x3e648
ldr r3, [sp, #0x64]
vldr d14, .LDIC0x3ded8
vldr d15, .LDIC0x3dee0
vmov.i16 q13, #3
lsl r0, r8, #1
mov r8, sb
mul r1, r1, r3
mul lr, r0, r3
str sl, [sp, #0x7c]
mov sb, ip
str r1, [sp, #0x5c]
mov sl, r5
str r0, [sp, #0x78]
mov r5, r4
lsr r1, r2, #3
lsl r2, r2, #1
str r1, [sp, #0x58]
str r2, [sp, #0x74]
str lr, [sp, #0x60]

.L0x3dd4c:
ldr r3, [sp, #0x7c]
ldr r2, [sp, #0x74]
ldr r1, [r3, #0x44]
ldr r3, [sp, #0x60]
add r1, r1, r3
add r2, r1, r2
ubfx r3, r1, #4, #2
ubfx r0, r2, #4, #2
rsb r3, r3, #0
ldr r2, [sp, #0x58]
and r3, r3, #3
cmp r3, r2
movhs lr, r2
movlo lr, r3
cmp lr, #0
moveq r2, r1
beq .L0x3dee8
ldr r3, [sp, #0x64]
mov r2, #0x410
ldr r4, [sp, #0x54]
mul r3, r2, r3
add r2, r1, lr, lsl #4

.L0x3dda4:
vld1.16 {d6, d7}, [r1]!
vmov.u16 ip, d7[2]
cmp r1, r2
add ip, sl, ip, lsl #1
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d7[3]
vshl.u16 q9, q9, q7
add ip, sl, ip, lsl #1
vld1.16 {d16[], d17[]}, [ip]
vmov ip, s12
vand q9, q9, q13
uxth ip, ip
vshl.u16 q8, q8, q7
add ip, sl, ip, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [ip]
vand q8, q8, q13
vshl.u16 q10, q10, q7
vmov.u16 ip, d7[1]
vshl.i16 q10, q10, #0xe
add ip, sl, ip, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d7[0]
vshl.u16 q9, q9, q7
add ip, sl, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d6[3]
vshl.u16 q9, q9, q7
add ip, sl, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d6[2]
vshl.u16 q9, q9, q7
add ip, sl, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
vmov.u16 ip, d6[1]
vshl.u16 q9, q9, q7
add ip, sl, ip, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [ip]
add ip, r8, r3
vshl.u16 q9, q9, q7
vand q9, q9, q13
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [ip]
add ip, sb, r3
vst1.16 {d16[1]}, [ip]
add ip, fp, r3
vst1.16 {d16[2]}, [ip]
add ip, r7, r3
vst1.16 {d16[3]}, [ip]
add ip, r6, r3
vst1.16 {d17[0]}, [ip]
add ip, r5, r3
vst1.16 {d17[1]}, [ip]
ldr ip, [sp, #0x4c]
add ip, ip, r3
vst1.16 {d17[2]}, [ip]
add ip, r4, r3
add r3, r3, #4
vst1.16 {d17[3]}, [ip]
bne .L0x3dda4
b .L0x3dee8

.LDIC0x3ded8:
.quad -1407387768586240

.LDIC0x3dee0:
.quad -3659221942468616

.L0x3dee8:
ldr r3, [sp, #0x58]
sub r3, r3, r0
bic r3, r3, r3, asr #31
cmp r3, lr
str r3, [sp, #0x50]
bls .L0x3e4c0
ldr r0, [sp, #0x5c]
vmov.i16 q5, #0x3000
add r3, lr, r0
vmov.i16 q4, #0xc000
lsl r4, r3, #2
add r3, r0, #1
add r3, r3, lr
stm sp, {r2, lr}
lsl ip, r3, #2
add r3, r0, #2
add r3, r3, lr
str r2, [sp, #0x68]
lsl r1, r3, #2
add r3, r0, #3
add r3, r3, lr
str lr, [sp, #0x6c]
mov lr, r7
lsl r0, r3, #2
mov r3, r4
mov r2, r0
mov r7, ip
mov r4, r6
mov r0, sb
mov r6, r1
mov ip, fp
mov r1, r8
mov r8, r3

.L0x3df6c:
ldr r3, [sp, #4]
ldr sb, [sp, #0x50]
add r3, r3, #4
cmp sb, r3
ldr sb, [sp]
str r3, [sp, #4]
mov r3, sb
add sb, sb, #0x40
vmov.i32 q8, #0
vld4.16 {d0, d2, d4, d6}, [r3]!
vmov.i32 q11, #0
vld4.16 {d1, d3, d5, d7}, [r3]
pld [sb, #0x80]
str sb, [sp]
vmov.u16 r3, d0[1]
vmov fp, s0
vmov.u16 sb, d4[2]
add r3, sl, r3, lsl #1
vmov.i16 q6, #0xc
vld1.16 {d16[0]}, [r3]
vmov.u16 r3, d2[1]
add sb, sl, sb, lsl #1
str sb, [sp, #0x1c]
add r3, sl, r3, lsl #1
vld1.16 {d22[0]}, [r3]
vmov.u16 r3, d4[1]
add r3, sl, r3, lsl #1
str r3, [sp, #8]
uxth r3, fp
vmov fp, s4
add r3, sl, r3, lsl #1
vld1.16 {d16[1]}, [r3]
uxth r3, fp
vmov fp, s8
add r3, sl, r3, lsl #1
vld1.16 {d22[1]}, [r3]
uxth r3, fp
add fp, sl, r3, lsl #1
vmov.u16 r3, d0[3]
str fp, [sp, #0x18]
vmov.u16 fp, d4[3]
add r3, sl, r3, lsl #1
vld1.16 {d16[2]}, [r3]
vmov.u16 r3, d2[3]
add fp, sl, fp, lsl #1
add r3, sl, r3, lsl #1
vld1.16 {d22[2]}, [r3]
vmov.u16 r3, d6[1]
add r3, sl, r3, lsl #1
str r3, [sp, #0x30]
vmov.u16 r3, d0[2]
add r3, sl, r3, lsl #1
vld1.16 {d16[3]}, [r3]
vmov.u16 r3, d2[2]
add r3, sl, r3, lsl #1
vld1.16 {d22[3]}, [r3]
vmov r3, s12
uxth r3, r3
add r3, sl, r3, lsl #1
str r3, [sp, #0x34]
vmov.u16 r3, d1[1]
add r3, sl, r3, lsl #1
vld1.16 {d17[0]}, [r3]
vmov.u16 r3, d3[1]
add r3, sl, r3, lsl #1
vld1.16 {d23[0]}, [r3]
vmov.u16 r3, d5[1]
add r3, sl, r3, lsl #1
str r3, [sp, #0x20]
vmov.u16 r3, d6[3]
add r3, sl, r3, lsl #1
str r3, [sp, #0x38]
vmov.u16 r3, d1[0]
add r3, sl, r3, lsl #1
vld1.16 {d17[1]}, [r3]
vmov.u16 r3, d3[0]
add r3, sl, r3, lsl #1
vld1.16 {d23[1]}, [r3]
vmov.u16 r3, d5[0]
add r3, sl, r3, lsl #1
str r3, [sp, #0x24]
vmov.u16 r3, d6[2]
add sb, sl, r3, lsl #1
vmov.u16 r3, d1[3]
str sb, [sp, #0x3c]
add r3, sl, r3, lsl #1
vld1.16 {d17[2]}, [r3]
vmov.u16 r3, d3[3]
add r3, sl, r3, lsl #1
vld1.16 {d23[2]}, [r3]
vmov.u16 r3, d5[3]
add r3, sl, r3, lsl #1
str r3, [sp, #0x28]
vmov.u16 r3, d7[1]
add r3, sl, r3, lsl #1
str r3, [sp, #0x40]
vmov.u16 r3, d1[2]
add r3, sl, r3, lsl #1
vld1.16 {d17[3]}, [r3]
vmov.u16 r3, d3[2]
vand q14, q8, q6
add r3, sl, r3, lsl #1
vand q15, q8, q13
vld1.16 {d23[3]}, [r3]
vmov.u16 r3, d5[2]
vand q9, q11, q6
add r3, sl, r3, lsl #1
str r3, [sp, #0x2c]
vmov.u16 r3, d7[0]
vshl.i16 q9, q9, #2
add sb, sl, r3, lsl #1
vmov.u16 r3, d7[3]
vshl.i16 q14, q14, #4
str sb, [sp, #0x44]
mov sb, r3
vmov.i16 q2, #0x30
vadd.i16 q14, q14, q9
vmov.i32 q9, #0
add sb, sl, sb, lsl #1
str sb, [sp, #0x48]
ldr sb, [sp, #8]
vand q10, q11, q13
vld1.16 {d18[0]}, [sb]
ldr sb, [sp, #0x18]
vand q12, q8, q2
vld1.16 {d18[1]}, [sb]
ldr sb, [sp, #0x1c]
vmov.u16 r3, d7[2]
vld1.16 {d18[2]}, [fp]
ldr fp, [sp, #0x20]
vand q3, q11, q2
vld1.16 {d18[3]}, [sb]
vshl.i16 q10, q10, #4
vld1.16 {d19[0]}, [fp]
ldr fp, [sp, #0x24]
vshl.i16 q15, q15, #6
vld1.16 {d19[1]}, [fp]
ldr fp, [sp, #0x28]
vadd.i16 q15, q15, q10
vld1.16 {d19[2]}, [fp]
ldr fp, [sp, #0x2c]
vshl.i16 q12, q12, #2
vld1.16 {d19[3]}, [fp]
vmov.i32 q10, #0
vand q2, q9, q2
ldr sb, [sp, #0x30]
vmov.i16 q1, #0xc0
vshr.u16 q2, q2, #2
vld1.16 {d20[0]}, [sb]
vadd.i16 q12, q12, q2
ldr sb, [sp, #0x34]
vand q0, q11, q1
vld1.16 {d20[1]}, [sb]
vadd.i16 q12, q12, q3
vand q3, q9, q1
ldr sb, [sp, #0x38]
vshr.u16 q0, q0, #2
vld1.16 {d20[2]}, [sb]
vshr.u16 q3, q3, #4
ldr sb, [sp, #0x3c]
vand q6, q8, q5
vld1.16 {d20[3]}, [sb]
vadd.i16 q0, q0, q3
vmov.i16 q3, #0x300
ldr sb, [sp, #0x40]
vshr.u16 q6, q6, #6
vld1.16 {d21[0]}, [sb]
vand q1, q8, q3
vand q3, q11, q3
ldr sb, [sp, #0x44]
vshr.u16 q1, q1, #2
vld1.16 {d21[1]}, [sb]
vshr.u16 q3, q3, #4
ldr sb, [sp, #0x48]
vstr d24, [sp, #8]
vstr d25, [sp, #0x10]
vmov.i16 q12, #0xc00
vadd.i16 q1, q1, q3
vand q2, q8, q12
vand q3, q11, q12
vshr.u16 q2, q2, #4
vshr.u16 q3, q3, #6
vmov.i16 q12, #0xc0
vadd.i16 q2, q2, q3
vand q3, q11, q5
vand q11, q11, q4
vshr.u16 q3, q3, #8
vshr.u16 q11, q11, #0xa
vadd.i16 q6, q6, q3
vand q3, q8, q4
vand q8, q8, q12
vshr.u16 q3, q3, #8
vadd.i16 q8, q0, q8
vadd.i16 q3, q3, q11
vmov.i16 q11, #0x300
vmov.i16 q0, #0xc00
vand q11, q9, q11
vld1.16 {d21[2]}, [sb]
vshr.u16 q11, q11, #6
add r3, sl, r3, lsl #1
vldr d24, [sp, #8]
vldr d25, [sp, #0x10]
vadd.i16 q1, q1, q11
vand q11, q9, q0
vand q0, q9, q4
vshr.u16 q11, q11, #8
vld1.16 {d21[3]}, [r3]
vadd.i16 q2, q2, q11
vshr.u16 q0, q0, #0xc
vand q11, q9, q5
vadd.i16 q3, q3, q0
vshr.u16 q11, q11, #0xa
vand q0, q10, q13
vadd.i16 q11, q6, q11
vadd.i16 q15, q15, q0
vmov.i16 q6, #0xc
vand q0, q9, q13
vand q9, q9, q6
vshl.i16 q0, q0, #2
vadd.i16 q14, q14, q9
vadd.i16 q15, q15, q0
vand q9, q10, q6
vmovn.i16 d30, q15
vshr.u16 q9, q9, #2
add r3, r1, r8
vmov.i16 q0, #0xc00
vst1.16 {d30[0]}, [r3]
vadd.i16 q9, q14, q9
add r3, r1, r7
ldr sb, [sp, #0x4c]
vst1.16 {d30[1]}, [r3]
add r3, r1, r6
vmovn.i16 d18, q9
vst1.16 {d30[2]}, [r3]
add r3, r2, r1
vst1.16 {d30[3]}, [r3]
add r3, r0, r8
vst1.16 {d18[0]}, [r3]
add r3, r0, r7
vst1.16 {d18[1]}, [r3]
add r3, r0, r6
vst1.16 {d18[2]}, [r3]
add r3, r0, r2
vst1.16 {d18[3]}, [r3]
vmov.i16 q9, #0x30
add r3, ip, r8
vand q9, q10, q9
vshr.u16 q9, q9, #4
vadd.i16 q12, q12, q9
vmovn.i16 d24, q12
vst1.16 {d24[0]}, [r3]
add r3, ip, r7
vst1.16 {d24[1]}, [r3]
add r3, ip, r6
vst1.16 {d24[2]}, [r3]
add r3, r2, ip
vst1.16 {d24[3]}, [r3]
vmov.i16 q12, #0xc0
add r3, lr, r8
vand q9, q10, q12
vshr.u16 q9, q9, #6
vadd.i16 q8, q8, q9
vmovn.i16 d16, q8
vst1.16 {d16[0]}, [r3]
add r3, lr, r7
vst1.16 {d16[1]}, [r3]
add r3, lr, r6
vst1.16 {d16[2]}, [r3]
add r3, lr, r2
vst1.16 {d16[3]}, [r3]
vmov.i16 q8, #0x300
add r3, r8, r4
vand q8, q10, q8
vshr.u16 q8, q8, #8
vadd.i16 q1, q1, q8
vand q8, q10, q0
vmovn.i16 d2, q1
vshr.u16 q8, q8, #0xa
vst1.16 {d2[0]}, [r3]
vadd.i16 q2, q2, q8
add r3, r7, r4
vand q8, q10, q5
vst1.16 {d2[1]}, [r3]
add r3, r6, r4
vmovn.i16 d4, q2
vst1.16 {d2[2]}, [r3]
vshr.u16 q8, q8, #0xc
add r3, r2, r4
vand q10, q10, q4
vst1.16 {d2[3]}, [r3]
add r3, r8, r5
vadd.i16 q11, q11, q8
vst1.16 {d4[0]}, [r3]
add r3, r7, r5
vmovn.i16 d22, q11
vst1.16 {d4[1]}, [r3]
add r3, r6, r5
vshr.u16 q10, q10, #0xe
vst1.16 {d4[2]}, [r3]
add r3, r2, r5
vadd.i16 q3, q3, q10
vst1.16 {d4[3]}, [r3]
add r3, r8, sb
vmovn.i16 d6, q3
vst1.16 {d22[0]}, [r3]
add r3, r7, sb
vst1.16 {d22[1]}, [r3]
add r3, r6, sb
vst1.16 {d22[2]}, [r3]
add r3, r2, sb
ldr sb, [sp, #0x54]
vst1.16 {d22[3]}, [r3]
add r3, r8, sb
add r8, r8, #0x10
vst1.16 {d6[0]}, [r3]
add r3, r7, sb
add r7, r7, #0x10
vst1.16 {d6[1]}, [r3]
add r3, r6, sb
add r6, r6, #0x10
vst1.16 {d6[2]}, [r3]
add r3, r2, sb
add r2, r2, #0x10
vst1.16 {d6[3]}, [r3]
bhi .L0x3df6c
ldr r3, [sp, #0x50]
mov r7, lr
ldr lr, [sp, #0x6c]
mov sb, r0
ldr r2, [sp, #0x68]
sub r3, r3, #1
sub r3, r3, lr
mov r8, r1
bic r0, r3, #3
add r1, lr, #4
lsr r3, r3, #2
mov fp, ip
add lr, r0, r1
add r3, r3, #1
mov r6, r4
add r2, r2, r3, lsl #6

.L0x3e4c0:
ldr r3, [sp, #0x58]
cmp lr, r3
bhs .L0x3e614
ldr r3, [sp, #0x5c]
ldr r1, [sp, #0x58]
add r3, lr, r3
sub lr, r1, lr
lsl r3, r3, #2
add lr, r2, lr, lsl #4
ldr r0, [sp, #0x4c]
ldr ip, [sp, #0x54]

.L0x3e4ec:
vld1.16 {d6, d7}, [r2]!
vmov.u16 r1, d7[2]
cmp r2, lr
add r1, sl, r1, lsl #1
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[3]
vshl.u16 q9, q9, q7
add r1, sl, r1, lsl #1
vld1.16 {d16[], d17[]}, [r1]
vmov r1, s12
vand q9, q9, q13
uxth r1, r1
vshl.u16 q8, q8, q7
add r1, sl, r1, lsl #1
vshl.i16 q9, q9, #2
vld1.16 {d20[], d21[]}, [r1]
vand q8, q8, q13
vshl.u16 q10, q10, q7
vmov.u16 r1, d7[1]
vshl.i16 q10, q10, #0xe
add r1, sl, r1, lsl #1
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d7[0]
vshl.u16 q9, q9, q7
add r1, sl, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #4
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[3]
vshl.u16 q9, q9, q7
add r1, sl, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #6
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[2]
vshl.u16 q9, q9, q7
add r1, sl, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #8
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
vmov.u16 r1, d6[1]
vshl.u16 q9, q9, q7
add r1, sl, r1, lsl #1
vand q9, q9, q13
vshl.i16 q9, q9, #0xa
vadd.i16 q8, q8, q9
vld1.16 {d18[], d19[]}, [r1]
add r1, r8, r3
vshl.u16 q9, q9, q7
vand q9, q9, q13
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q9
vst1.16 {d16[0]}, [r1]
add r1, sb, r3
vst1.16 {d16[1]}, [r1]
add r1, fp, r3
vst1.16 {d16[2]}, [r1]
add r1, r7, r3
vst1.16 {d16[3]}, [r1]
add r1, r6, r3
vst1.16 {d17[0]}, [r1]
add r1, r5, r3
vst1.16 {d17[1]}, [r1]
add r1, r0, r3
vst1.16 {d17[2]}, [r1]
add r1, ip, r3
add r3, r3, #4
vst1.16 {d17[3]}, [r1]
bne .L0x3e4ec

.L0x3e614:
ldr r3, [sp, #0x60]
ldr r2, [sp, #0x78]
add r3, r3, r2
str r3, [sp, #0x60]
ldr r3, [sp, #0x64]
ldr r2, [sp, #0x70]
add r3, r3, #1
cmp r3, r2
str r3, [sp, #0x64]
ldr r3, [sp, #0x5c]
add r3, r3, #0x104
str r3, [sp, #0x5c]
bls .L0x3dd4c

.L0x3e648:
add sp, sp, #0x84
vpop {d8, d9, d10, d11, d12, d13, d14, d15}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3e654:
sub lr, lr, r4
cmp r2, r5
add r4, lr, #1
streq lr, [sp, #0x70]
udiv r3, r4, r3
mul r2, r2, r3
subne r3, r3, #1
addne r3, r3, r2
str r2, [sp, #0x64]
strne r3, [sp, #0x70]
b .L0x3dc24

.section .note.GNU-stack,""
