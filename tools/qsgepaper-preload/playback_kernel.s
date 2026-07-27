.syntax unified
.arch armv7-a
.arch_extension idiv
.fpu neon
.global playback_kernel_plain

.L0x379f0:
ldr ip, [r0, #0x2c]
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r6, r0
ldr r3, [ip, #4]
mov lr, #0x10
ldr r5, [r0, #0x14]
sub sp, sp, #0x14
mul r0, r3, r3
ldr r3, [ip, #0xc]
ldr ip, [ip, #0x10]
udiv lr, lr, r3
ldrsh r3, [r6, #0x28]
vldr d18, .LDIC0x37bf8
vldr d19, .LDIC0x37c00
sdiv r3, r3, lr
mla r3, r0, r3, r3
sub r0, r2, #1
add ip, ip, r3, lsl #1
ldr r3, [r6, #0xc]
cmp r5, r3
blt .L0x37aac
ldr r4, [r6, #0x10]
ldr lr, [r6, #0x18]
cmp r4, lr
bgt .L0x37aa4
sub lr, lr, r4
cmp r1, r0
add r0, lr, #1
moveq r8, lr
udiv r2, r0, r2
mul r4, r2, r1
subne r2, r2, #1
addne r8, r2, r4

.L0x37a74:
ldr r1, [r6, #0x18]
ldr r2, [r6, #0x10]
cmp r1, r2
subge r7, r5, r3
movlt r7, #0
addge r7, r7, #1
lsrge r7, r7, #3
cmp r8, r4
blo .L0x37be4
ldr r3, [r6, #0x3c]
ldr sb, [r3, #0x14]
b .L0x37ac0

.L0x37aa4:
cmp r1, r0
bne .L0x37bec

.L0x37aac:
ldr r3, [r6, #0x3c]
mov r7, #0
mvn r8, #0
mov r4, r7
ldr sb, [r3, #0x14]

.L0x37ac0:
mul r5, sb, r4
vmov.i16 q11, #3
add lr, r5, r7, lsl #3
mov r0, sp
lsl sl, sb, #1
lsl lr, lr, #1

.L0x37ad8:
cmp r7, #0
lslne r2, r5, #1
ldrne r1, [r6, #0x44]
beq .L0x37bd0

.L0x37ae8:
mov r3, r1
ldrh fp, [r3, r2]!
add r2, r2, #0x10
cmp r2, lr
add fp, ip, fp, lsl #1
vld1.16 {d24[], d25[]}, [fp]
ldrh fp, [r3, #0xc]
add fp, ip, fp, lsl #1
vshl.u16 q12, q12, q9
vld1.16 {d20[], d21[]}, [fp]
ldrh fp, [r3, #0xe]
vshl.i16 q12, q12, #0xe
add fp, ip, fp, lsl #1
vshl.u16 q10, q10, q9
vld1.16 {d16[], d17[]}, [fp]
vand q10, q10, q11
vshl.u16 q8, q8, q9
ldrh fp, [r3, #0xa]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add fp, ip, fp, lsl #1
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [fp]
ldrh fp, [r3, #8]
vshl.u16 q10, q10, q9
add fp, ip, fp, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [fp]
ldrh fp, [r3, #6]
vshl.u16 q10, q10, q9
add fp, ip, fp, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #6
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [fp]
ldrh fp, [r3, #4]
ldrh r3, [r3, #2]
vshl.u16 q10, q10, q9
add fp, ip, fp, lsl #1
add r3, ip, r3, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [fp]
vshl.i16 q10, q10, #8
vshl.u16 q12, q12, q9
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r3]
vand q12, q12, q11
vshl.u16 q10, q10, q9
vshl.i16 q12, q12, #0xa
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xc
vadd.i16 q8, q8, q10
vst1.16 {d16, d17}, [r0:0x40]
bne .L0x37ae8

.L0x37bd0:
add r5, r5, sb
add r4, r4, #1
cmp r8, r4
add lr, lr, sl
bhs .L0x37ad8

.L0x37be4:
add sp, sp, #0x14
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x37bec:
mvn r8, #0
mov r4, #0
b .L0x37a74

.LDIC0x37bf8:
.quad -1407387768586240

.LDIC0x37c00:
.quad -3659221942468616

.L0x37c08:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov ip, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x2c
ldr r1, [r1, #0x2c]
ldrsh r5, [ip, #0x28]
ldr lr, [r1, #4]
ldr r8, [r1, #0xc]
mul r7, lr, lr
ldr r4, [r1, #0x10]
mov lr, #0x10
ldr r6, [ip, #0x14]
udiv lr, lr, r8
sdiv lr, r5, lr
ldr r1, [ip, #0xc]
and r5, r5, #7
mla lr, r7, lr, lr
cmp r6, r1
vldr d8, .LDIC0x37ed8
vldr d9, .LDIC0x37ee0
add r4, r4, lr, lsl #1
blt .L0x37cb0
ldr lr, [ip, #0x18]
sub r8, r3, #1
ldr r7, [ip, #0x10]
cmp r7, lr
mvngt lr, #0
movgt sl, #0
ble .L0x37e98
ldr r2, [ip, #0x18]
ldr r3, [ip, #0x10]
cmp r2, r3
blt .L0x37ec4

.L0x37c90:
sub r3, r6, r1
add r3, r3, #1
lsr r2, r3, #3
str r2, [fp, #-0x50]
lsl r3, r2, #5
lsl r2, r2, #2
str r2, [fp, #-0x4c]
b .L0x37cc4

.L0x37cb0:
mov sl, #0
mvn lr, #0
str sl, [fp, #-0x4c]
mov r3, sl
str sl, [fp, #-0x50]

.L0x37cc4:
sub sp, sp, r3
cmp r1, #0
ldr r3, [ip, #0x3c]
ldr r7, [r0]
ldr r2, [ip, #0x10]
ldr r0, [r3, #0x14]
add r3, r1, #7
movge r3, r1
add r2, r2, #3
cmp sl, lr
mov r1, #0x104
asr r3, r3, #3
mla r3, r1, r2, r3
str r0, [fp, #-0x54]
mov r2, sp
str r2, [fp, #-0x48]
bhi .L0x37e8c
str lr, [fp, #-0x58]
add r3, r3, #0x1a
mla r3, r1, sl, r3
mul r8, sl, r0
add r7, r7, r3, lsl #2
sub r3, fp, #0x34
add r5, r3, r5, lsl #1
sub r6, fp, #0x44
ldr r3, [fp, #-0x50]
str ip, [fp, #-0x5c]
add sb, r2, r3, lsl #2

.L0x37d34:
ldr r2, [fp, #-0x4c]
mov r1, r7
ldr r0, [fp, #-0x48]
bl memcpy
ldr r3, [fp, #-0x50]
cmp r3, #0
beq .L0x37e60
ldr r3, [fp, #-0x5c]
add r2, r8, #1
vmov.i16 q11, #3
lsl r2, r2, #1
ldr r1, [fp, #-0x48]
ldr lr, [r3, #0x44]

.L0x37d68:
ldrh ip, [lr, r2]
add r3, lr, r2
add r2, r2, #0x10
add ip, r4, ip, lsl #1
ldrh r0, [r1]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r3, #-2]
add ip, r4, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d16[], d17[]}, [ip]
ldrh ip, [r3, #0xa]
vand q9, q9, q11
add ip, r4, ip, lsl #1
vshl.u16 q8, q8, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #0xc]
vshl.i16 q8, q8, #0xe
add ip, r4, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d24[], d25[]}, [ip]
vand q10, q10, q11
vshl.u16 q12, q12, q4
ldrh ip, [r3, #8]
vshl.i16 q10, q10, #2
vand q12, q12, q11
add ip, r4, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #6]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #4]
ldrh r3, [r3, #2]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
add r3, r4, r3, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r3]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [r6:0x40]
ldrh r3, [r5, #-0x10]
orr r3, r0, r3
strh r3, [r1], #4
cmp r1, sb
bne .L0x37d68

.L0x37e60:
ldr r2, [fp, #-0x4c]
mov r0, r7
ldr r1, [fp, #-0x48]
add sl, sl, #1
add r7, r7, #0x410
bl memcpy
ldr r3, [fp, #-0x54]
add r8, r8, r3
ldr r3, [fp, #-0x58]
cmp sl, r3
bls .L0x37d34

.L0x37e8c:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x37e98:
sub lr, lr, r7
cmp r2, r8
add r7, lr, #1
udiv r3, r7, r3
mul sl, r3, r2
ldr r2, [ip, #0x18]
subne r3, r3, #1
addne lr, r3, sl
ldr r3, [ip, #0x10]
cmp r2, r3
bge .L0x37c90

.L0x37ec4:
mov r3, #0
str r3, [fp, #-0x4c]
str r3, [fp, #-0x50]
b .L0x37cc4

.LDIC0x37ed8:
.quad -1407387768586240

.LDIC0x37ee0:
.quad -3659221942468616

.L0x37ee8:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r8, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x3c
ldr r4, [r1, #0x2c]
ldrsh r6, [r1, #0x28]
mov lr, r0
ldr ip, [r1, #0x14]
str r1, [fp, #-0x64]
ldr r1, [r4, #4]
ldr r7, [r4, #0xc]
mul r0, r1, r1
ldr r5, [r8, #0xc]
mov r1, #0x10
ldr r4, [r4, #0x10]
udiv r1, r1, r7
sdiv r1, r6, r1
mla r1, r0, r1, r1
cmp ip, r5
add r4, r4, r1, lsl #1
and r1, r6, #7
vldr d8, .LDIC0x38258
vldr d9, .LDIC0x38260
str r1, [fp, #-0x6c]
blt .L0x37fa0
ldr r0, [r8, #0x10]
sub r6, r3, #1
ldr r1, [r8, #0x18]
cmp r0, r1
ble .L0x38220
mov r0, #0
mvn r3, #0
str r3, [fp, #-0x5c]

.L0x37f70:
ldr r3, [fp, #-0x64]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x38248
sub r3, ip, r5
add r3, r3, #1
lsr r2, r3, #3
str r2, [fp, #-0x60]
lsl r3, r2, #5
lsl ip, r2, #2
b .L0x37fb8

.L0x37fa0:
mov ip, #0
mvn r3, #0
str r3, [fp, #-0x5c]
mov r0, ip
str ip, [fp, #-0x60]
mov r3, ip

.L0x37fb8:
ldr r2, [fp, #-0x5c]
cmp r5, #0
sub sp, sp, r3
add r3, r5, #7
movge r3, r5
cmp r0, r2
ldr r2, [fp, #-0x64]
asr r3, r3, #3
ldr r5, [r2, #0x10]
ldr r2, [r2, #0x3c]
add r5, r5, #3
ldr r8, [r2, #0x14]
ldm lr, {r1, r2}
str r8, [fp, #-0x68]
mov lr, #0x104
mla r3, lr, r5, r3
mov r5, sp
add r6, r5, ip
bhi .L0x38214
str r0, [fp, #-0x54]
add r3, r3, #0x1a
mla r3, lr, r0, r3
str ip, [fp, #-0x58]
lsl r3, r3, #2
add r1, r1, r3
add r3, r2, r3
str r3, [fp, #-0x4c]
ldr r3, [fp, #-0x6c]
str r1, [fp, #-0x48]
add r7, r3, #1
sub r3, fp, #0x34
add r7, r3, r7, lsl #1
mul r3, r8, r0
str r3, [fp, #-0x50]
ldr r3, [fp, #-0x60]
lsl r8, r3, #2

.L0x38048:
ldr sb, [fp, #-0x58]
mov r0, r5
ldr r1, [fp, #-0x48]
mov r2, sb
bl memcpy
ldr r1, [fp, #-0x4c]
mov r2, sb
mov r0, r6
bl memcpy
ldr r3, [fp, #-0x60]
cmp r3, #0
beq .L0x381b0
ldr r3, [fp, #-0x50]
mov r2, #0
ldr r0, [fp, #-0x6c]
sub sl, fp, #0x44
add r1, r3, #1
ldr r3, [fp, #-0x64]
vmov.i16 q11, #3
ldr lr, [r3, #0x44]
sub r3, fp, #0x34
lsl r1, r1, #1
add sb, r3, r0, lsl #1

.L0x380a4:
ldrh ip, [lr, r1]
add r3, lr, r1
add r1, r1, #0x10
add ip, r4, ip, lsl #1
ldrh r0, [r5, r2]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r3, #-2]
add ip, r4, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r3, #0xa]
vand q9, q9, q11
add ip, r4, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #0xc]
vshl.i16 q12, q12, #0xe
add ip, r4, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r3, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, r4, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #6]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #4]
ldrh r3, [r3, #2]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
add r3, r4, r3, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r3]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [sl:0x40]
ldrh r3, [sb, #-0x10]
orr r3, r0, r3
strh r3, [r5, r2]
ldrh r0, [r7, #-0x10]
ldrh r3, [r6, r2]
orr r3, r3, r0
strh r3, [r6, r2]
add r2, r2, #4
cmp r2, r8
bne .L0x380a4

.L0x381b0:
ldr sl, [fp, #-0x58]
mov r1, r5
ldr sb, [fp, #-0x48]
mov r2, sl
mov r0, sb
bl memcpy
mov r2, sl
mov r1, r6
ldr sl, [fp, #-0x4c]
mov r0, sl
bl memcpy
ldr r3, [fp, #-0x50]
ldr r2, [fp, #-0x68]
add r3, r3, r2
str r3, [fp, #-0x50]
ldr r3, [fp, #-0x54]
ldr r2, [fp, #-0x5c]
add r3, r3, #1
cmp r3, r2
str r3, [fp, #-0x54]
add r3, sb, #0x410
str r3, [fp, #-0x48]
add r3, sl, #0x410
str r3, [fp, #-0x4c]
bls .L0x38048

.L0x38214:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x38220:
sub r1, r1, r0
cmp r2, r6
add r0, r1, #1
streq r1, [fp, #-0x5c]
udiv r3, r0, r3
mul r0, r3, r2
subne r3, r3, #1
addne r3, r3, r0
strne r3, [fp, #-0x5c]
b .L0x37f70

.L0x38248:
mov ip, #0
str ip, [fp, #-0x60]
mov r3, ip
b .L0x37fb8

.LDIC0x38258:
.quad -1407387768586240

.LDIC0x38260:
.quad -3659221942468616

.L0x38268:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r8, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x4c
ldr lr, [r1, #0x2c]
ldrsh r5, [r1, #0x28]
mov r7, r0
ldr ip, [r1, #0x14]
str r1, [fp, #-0x70]
ldr r1, [lr, #4]
ldr r6, [lr, #0xc]
mul r0, r1, r1
ldr r4, [lr, #0x10]
mov r1, #0x10
ldr lr, [r8, #0xc]
udiv r1, r1, r6
sdiv r1, r5, r1
mla r1, r0, r1, r1
cmp ip, lr
add r4, r4, r1, lsl #1
and r1, r5, #7
vldr d8, .LDIC0x38650
vldr d9, .LDIC0x38658
str r1, [fp, #-0x78]
blt .L0x38324
ldr r0, [r8, #0x10]
sub r5, r3, #1
ldr r1, [r8, #0x18]
cmp r0, r1
ble .L0x38610
mov r0, #0
mvn r3, #0
str r3, [fp, #-0x68]

.L0x382f0:
ldr r3, [fp, #-0x70]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x38638
sub r3, ip, lr
add r3, r3, #1
bic r6, r3, #7
lsr r2, r3, #3
str r2, [fp, #-0x6c]
lsl r3, r2, #5
lsl ip, r2, #2
b .L0x38340

.L0x38324:
mov ip, #0
mvn r3, #0
str r3, [fp, #-0x68]
mov r6, ip
str ip, [fp, #-0x6c]
mov r3, ip
mov r0, ip

.L0x38340:
ldr r2, [fp, #-0x68]
cmp lr, #0
ldr r1, [fp, #-0x70]
mov r8, #0x104
sub sp, sp, r3
add r3, lr, #7
movge r3, lr
mov r5, sp
cmp r0, r2
ldr r2, [r1, #0x10]
ldr r1, [r1, #0x3c]
ldr lr, [r7]
add r2, r2, #3
ldr sb, [r1, #0x14]
asr r3, r3, #3
mla r3, r8, r2, r3
add r6, r5, r6
ldmib r7, {r1, r2}
str sb, [fp, #-0x74]
add r7, r5, ip
bhi .L0x38604
str r0, [fp, #-0x60]
add r3, r3, #0x1a
mla r3, r8, r0, r3
str ip, [fp, #-0x64]
lsl r3, r3, #2
add lr, lr, r3
add r1, r1, r3
add r3, r2, r3
sub r2, fp, #0x34
str r3, [fp, #-0x50]
ldr r3, [fp, #-0x78]
str lr, [fp, #-0x54]
str r1, [fp, #-0x58]
add r3, r3, #1
add r3, r2, r3, lsl #1
str r3, [fp, #-0x4c]
mul r3, sb, r0
str r3, [fp, #-0x5c]
ldr r3, [fp, #-0x6c]
lsl r3, r3, #2
str r3, [fp, #-0x48]

.L0x383e8:
ldr r8, [fp, #-0x64]
mov r0, r5
ldr r1, [fp, #-0x54]
mov r2, r8
bl memcpy
ldr r1, [fp, #-0x58]
mov r2, r8
mov r0, r7
bl memcpy
ldr r1, [fp, #-0x50]
mov r2, r8
mov r0, r6
bl memcpy
ldr r3, [fp, #-0x6c]
cmp r3, #0
beq .L0x38584
ldr r3, [fp, #-0x78]
sub r2, fp, #0x34
ldr r0, [fp, #-0x70]
sub sl, fp, #0x44
add r8, r3, #2
vmov.i16 q11, #3
add r8, r2, r8, lsl #1
ldr r2, [fp, #-0x5c]
ldr lr, [r0, #0x44]
sub r0, fp, #0x34
add sb, r0, r3, lsl #1
add r1, r2, #1
mov r2, #0
lsl r1, r1, #1

.L0x38460:
ldrh ip, [lr, r1]
add r3, lr, r1
add r1, r1, #0x10
add ip, r4, ip, lsl #1
ldrh r0, [r5, r2]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r3, #-2]
add ip, r4, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r3, #0xa]
vand q9, q9, q11
add ip, r4, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #0xc]
vshl.i16 q12, q12, #0xe
add ip, r4, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r3, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, r4, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #6]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r3, #4]
ldrh r3, [r3, #2]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
add r3, r4, r3, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r3]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [sl:0x40]
ldrh r3, [sb, #-0x10]
orr r0, r0, r3
ldr r3, [fp, #-0x4c]
strh r0, [r5, r2]
ldrh r0, [r3, #-0x10]
ldrh r3, [r7, r2]
orr r3, r3, r0
strh r3, [r7, r2]
ldrh r0, [r8, #-0x10]
ldrh r3, [r6, r2]
orr r3, r3, r0
strh r3, [r6, r2]
add r2, r2, #4
ldr r3, [fp, #-0x48]
cmp r2, r3
bne .L0x38460

.L0x38584:
ldr r8, [fp, #-0x64]
mov r1, r5
ldr sb, [fp, #-0x54]
mov r2, r8
mov r0, sb
bl memcpy
ldr sl, [fp, #-0x58]
mov r2, r8
mov r1, r7
mov r0, sl
bl memcpy
mov r2, r8
mov r1, r6
ldr r8, [fp, #-0x50]
mov r0, r8
bl memcpy
ldr r3, [fp, #-0x5c]
ldr r2, [fp, #-0x74]
add r3, r3, r2
str r3, [fp, #-0x5c]
ldr r3, [fp, #-0x60]
ldr r2, [fp, #-0x68]
add r3, r3, #1
cmp r3, r2
str r3, [fp, #-0x60]
add r3, sb, #0x410
str r3, [fp, #-0x54]
add r3, sl, #0x410
str r3, [fp, #-0x58]
add r3, r8, #0x410
str r3, [fp, #-0x50]
bls .L0x383e8

.L0x38604:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x38610:
sub r1, r1, r0
cmp r2, r5
add r0, r1, #1
streq r1, [fp, #-0x68]
udiv r3, r0, r3
mul r0, r3, r2
subne r3, r3, #1
addne r3, r3, r0
strne r3, [fp, #-0x68]
b .L0x382f0

.L0x38638:
mov ip, #0
str ip, [fp, #-0x6c]
mov r6, ip
mov r3, ip
b .L0x38340

.LDIC0x38650:
.quad -1407387768586240

.LDIC0x38658:
.quad -3659221942468616

.L0x38660:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r8, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x54
ldr r4, [r1, #0x2c]
ldrsh lr, [r1, #0x28]
mov r6, r0
ldr ip, [r1, #0x14]
str r1, [fp, #-0x80]
ldr r1, [r4, #4]
ldr r7, [r4, #0xc]
mul r0, r1, r1
ldr r5, [r8, #0xc]
mov r1, #0x10
ldr r4, [r4, #0x10]
udiv r1, r1, r7
sdiv r1, lr, r1
mla r1, r0, r1, r1
cmp ip, r5
add r4, r4, r1, lsl #1
and r1, lr, #7
vldr d8, .LDIC0x38aa8
vldr d9, .LDIC0x38ab0
str r1, [fp, #-0x84]
blt .L0x38724
ldr r0, [r8, #0x10]
sub lr, r3, #1
ldr r1, [r8, #0x18]
cmp r0, r1
ble .L0x38a80
mov r0, #0
mvn r3, #0
str r3, [fp, #-0x78]

.L0x386e8:
ldr r3, [fp, #-0x80]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x38ab8
sub r3, ip, r5
mov r1, #0xc
add r3, r3, #1
bic lr, r3, #7
lsr r2, r3, #3
mul r1, r1, r2
lsl r3, r2, #5
lsl ip, r2, #2
str r2, [fp, #-0x7c]
b .L0x38744

.L0x38724:
mov ip, #0
mvn r3, #0
str r3, [fp, #-0x78]
mov r1, ip
str ip, [fp, #-0x7c]
mov lr, ip
mov r3, ip
mov r0, ip

.L0x38744:
sub sp, sp, r3
cmp r5, #0
ldr r2, [fp, #-0x78]
add r3, r5, #7
ldr r8, [r6]
movge r3, r5
ldr r5, [fp, #-0x80]
mov r7, #0x104
cmp r0, r2
ldr r2, [r5, #0x10]
ldr r5, [r5, #0x3c]
asr r3, r3, #3
add r2, r2, #3
ldr r5, [r5, #0x14]
mla r3, r7, r2, r3
str r5, [fp, #-0x74]
mov r5, sp
add sl, r5, lr
add sb, r5, r1
ldr lr, [r6, #4]
ldr r1, [r6, #8]
ldr r2, [r6, #0xc]
add r6, r5, ip
bhi .L0x38a74
str r0, [fp, #-0x6c]
add r3, r3, #0x1a
mla r3, r7, r0, r3
str ip, [fp, #-0x70]
lsl r3, r3, #2
add r2, r2, r3
str r2, [fp, #-0x60]
ldr r2, [fp, #-0x84]
add r7, r8, r3
add lr, lr, r3
add r1, r1, r3
add r3, r2, #1
str r7, [fp, #-0x64]
sub r2, fp, #0x34
add r2, r2, r3, lsl #1
ldr r3, [fp, #-0x74]
str lr, [fp, #-0x5c]
mul r3, r3, r0
str r1, [fp, #-0x58]
str r3, [fp, #-0x68]
ldr r3, [fp, #-0x7c]
str r2, [fp, #-0x54]
lsl r3, r3, #2
str r3, [fp, #-0x50]

.L0x38804:
ldr r7, [fp, #-0x70]
mov r0, r5
ldr r1, [fp, #-0x64]
mov r2, r7
bl memcpy
ldr r1, [fp, #-0x5c]
mov r2, r7
mov r0, r6
bl memcpy
ldr r1, [fp, #-0x58]
mov r2, r7
mov r0, sl
bl memcpy
ldr r1, [fp, #-0x60]
mov r2, r7
mov r0, sb
bl memcpy
ldr r3, [fp, #-0x7c]
cmp r3, #0
beq .L0x389d8
ldr r2, [fp, #-0x84]
sub r1, fp, #0x34
ldr r0, [fp, #-0x80]
sub r8, fp, #0x44
add r3, r2, #2
vmov.i16 q11, #3
add r3, r1, r3, lsl #1
str r3, [fp, #-0x48]
add r3, r2, #3
ldr lr, [r0, #0x44]
sub r0, fp, #0x34
add r3, r1, r3, lsl #1
str r3, [fp, #-0x4c]
ldr r3, [fp, #-0x68]
add r7, r0, r2, lsl #1
add r1, r3, #1
mov r3, #0
lsl r1, r1, #1

.L0x3889c:
ldrh ip, [lr, r1]
add r2, lr, r1
add r1, r1, #0x10
add ip, r4, ip, lsl #1
ldrh r0, [r5, r3]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r2, #-2]
add ip, r4, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r2, #0xa]
vand q9, q9, q11
add ip, r4, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #0xc]
vshl.i16 q12, q12, #0xe
add ip, r4, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r2, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, r4, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #6]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #4]
ldrh r2, [r2, #2]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
add r2, r4, r2, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r2]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [r8:0x40]
ldrh r2, [r7, #-0x10]
orr r0, r0, r2
ldr r2, [fp, #-0x54]
strh r0, [r5, r3]
ldrh r0, [r2, #-0x10]
ldrh r2, [r6, r3]
orr r2, r2, r0
strh r2, [r6, r3]
ldr r2, [fp, #-0x48]
ldrh r0, [r2, #-0x10]
ldrh r2, [sl, r3]
orr r2, r2, r0
strh r2, [sl, r3]
ldr r2, [fp, #-0x4c]
ldrh r0, [r2, #-0x10]
ldrh r2, [sb, r3]
orr r2, r2, r0
strh r2, [sb, r3]
add r3, r3, #4
ldr r2, [fp, #-0x50]
cmp r3, r2
bne .L0x3889c

.L0x389d8:
ldr r7, [fp, #-0x70]
mov r1, r5
ldr r8, [fp, #-0x64]
mov r2, r7
mov r0, r8
bl memcpy
ldr r0, [fp, #-0x5c]
mov r2, r7
mov r1, r6
bl memcpy
ldr r0, [fp, #-0x58]
mov r2, r7
mov r1, sl
bl memcpy
mov r2, r7
mov r1, sb
ldr r7, [fp, #-0x60]
mov r0, r7
bl memcpy
ldr r2, [fp, #-0x68]
ldr r1, [fp, #-0x74]
add r2, r2, r1
str r2, [fp, #-0x68]
ldr r2, [fp, #-0x6c]
add r3, r2, #1
ldr r2, [fp, #-0x78]
str r3, [fp, #-0x6c]
cmp r3, r2
add r2, r8, #0x410
ldr r3, [fp, #-0x5c]
str r2, [fp, #-0x64]
add r2, r3, #0x410
ldr r3, [fp, #-0x58]
str r2, [fp, #-0x5c]
add r3, r3, #0x410
str r3, [fp, #-0x58]
add r3, r7, #0x410
str r3, [fp, #-0x60]
bls .L0x38804

.L0x38a74:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x38a80:
sub r1, r1, r0
cmp r2, lr
add r0, r1, #1
streq r1, [fp, #-0x78]
udiv r3, r0, r3
mul r0, r3, r2
subne r3, r3, #1
addne r3, r3, r0
strne r3, [fp, #-0x78]
b .L0x386e8

.LDIC0x38aa8:
.quad -1407387768586240

.LDIC0x38ab0:
.quad -3659221942468616

.L0x38ab8:
mov ip, #0
str ip, [fp, #-0x7c]
mov r1, ip
mov lr, ip
mov r3, ip
b .L0x38744

.L0x38ad0:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r8, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x64
ldr r4, [r1, #0x2c]
ldrsh lr, [r1, #0x28]
mov r5, r0
ldr ip, [r1, #0x14]
str r1, [fp, #-0x8c]
ldr r1, [r4, #4]
ldr r6, [r4, #0xc]
mul r0, r1, r1
ldr r4, [r4, #0x10]
mov r1, #0x10
ldr r7, [r8, #0xc]
udiv r1, r1, r6
sdiv r1, lr, r1
mla r1, r0, r1, r1
cmp ip, r7
add r1, r4, r1, lsl #1
str r1, [fp, #-0x48]
and r1, lr, #7
vldr d8, .LDIC0x38ba8
vldr d9, .LDIC0x38bb0
str r1, [fp, #-0x90]
blt .L0x38bb8
ldr lr, [r8, #0x10]
sub r6, r3, #1
ldr r1, [r8, #0x18]
cmp lr, r1
ble .L0x38f88
mvn r3, #0
str r3, [fp, #-0x80]
mov r3, #0
str r3, [fp, #-0x78]

.L0x38b60:
ldr r3, [fp, #-0x8c]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x38fb4
sub r3, ip, r7
mov lr, #0xc
add r3, r3, #1
bic r6, r3, #7
lsr r2, r3, #3
mul lr, lr, r2
lsl r3, r2, #5
lsl r1, r2, #4
str r2, [fp, #-0x88]
lsl r2, r2, #2
str r2, [fp, #-0x7c]
b .L0x38be0

.LDIC0x38ba8:
.quad -1407387768586240

.LDIC0x38bb0:
.quad -3659221942468616

.L0x38bb8:
mov r2, #0
mvn r3, #0
str r3, [fp, #-0x80]
mov r1, r2
str r2, [fp, #-0x7c]
mov lr, r2
str r2, [fp, #-0x88]
mov r6, r2
str r2, [fp, #-0x78]
mov r3, r2

.L0x38be0:
ldr r2, [fp, #-0x78]
cmp r7, #0
ldr r0, [fp, #-0x80]
sub sp, sp, r3
add r3, r7, #7
ldr r4, [fp, #-0x7c]
movge r3, r7
cmp r2, r0
mov sb, sp
ldr r0, [fp, #-0x8c]
asr r3, r3, #3
ldr r2, [r0, #0x3c]
ldr r7, [r0, #0x10]
ldr r0, [r2, #0x14]
add sl, sb, r6
add r2, r7, #3
ldr ip, [r5]
mov r6, #0x104
add r7, sb, lr
add r8, sb, r1
mla r3, r6, r2, r3
ldr lr, [r5, #4]
ldr r1, [r5, #8]
ldr r2, [r5, #0xc]
str r0, [fp, #-0x84]
ldr r0, [r5, #0x10]
add r5, sb, r4
bhi .L0x38f7c
ldr r4, [fp, #-0x78]
add r3, r3, #0x1a
mla r3, r6, r4, r3
lsl r3, r3, #2
add r2, r2, r3
str r2, [fp, #-0x60]
add r2, r0, r3
str r2, [fp, #-0x70]
ldr r2, [fp, #-0x90]
add ip, ip, r3
add r1, r1, r3
ldr r0, [fp, #-0x84]
str ip, [fp, #-0x68]
add ip, lr, r3
add r3, r2, #1
str ip, [fp, #-0x6c]
sub r2, fp, #0x34
add r2, r2, r3, lsl #1
mul r3, r0, r4
ldr r4, [fp, #-0x48]
str r3, [fp, #-0x74]
ldr r3, [fp, #-0x88]
str r1, [fp, #-0x64]
lsl r3, r3, #2
str r2, [fp, #-0x5c]
str r3, [fp, #-0x58]

.L0x38cb8:
ldr r6, [fp, #-0x7c]
mov r0, sb
ldr r1, [fp, #-0x68]
mov r2, r6
bl memcpy
ldr r1, [fp, #-0x6c]
mov r2, r6
mov r0, r5
bl memcpy
ldr r1, [fp, #-0x64]
mov r2, r6
mov r0, sl
bl memcpy
ldr r1, [fp, #-0x60]
mov r2, r6
mov r0, r7
bl memcpy
ldr r1, [fp, #-0x70]
mov r2, r6
mov r0, r8
bl memcpy
ldr r3, [fp, #-0x88]
cmp r3, #0
beq .L0x38ec4
ldr r2, [fp, #-0x90]
sub r1, fp, #0x34
ldr r0, [fp, #-0x8c]
add r3, r2, #2
ldr lr, [r0, #0x44]
sub r0, fp, #0x44
add r3, r1, r3, lsl #1
str r3, [fp, #-0x48]
add r3, r2, #3
vmov.i16 q11, #3
add r3, r1, r3, lsl #1
str r3, [fp, #-0x4c]
add r3, r2, #4
str r0, [fp, #-0x54]
sub r0, fp, #0x34
add r3, r1, r3, lsl #1
str r3, [fp, #-0x50]
ldr r3, [fp, #-0x74]
add r6, r0, r2, lsl #1
add r1, r3, #1
mov r3, #0
lsl r1, r1, #1

.L0x38d70:
ldrh ip, [lr, r1]
add r2, lr, r1
add r1, r1, #0x10
add ip, r4, ip, lsl #1
ldrh r0, [sb, r3]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r2, #-2]
add ip, r4, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r2, #0xa]
vand q9, q9, q11
add ip, r4, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #0xc]
vshl.i16 q12, q12, #0xe
add ip, r4, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r2, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, r4, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #6]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #4]
ldrh r2, [r2, #2]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
add r2, r4, r2, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r2]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
ldr r2, [fp, #-0x54]
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [r2:0x40]
ldrh r2, [r6, #-0x10]
orr r0, r0, r2
ldr r2, [fp, #-0x5c]
strh r0, [sb, r3]
ldrh r0, [r2, #-0x10]
ldrh r2, [r5, r3]
orr r2, r2, r0
strh r2, [r5, r3]
ldr r2, [fp, #-0x48]
ldrh r0, [r2, #-0x10]
ldrh r2, [sl, r3]
orr r2, r2, r0
strh r2, [sl, r3]
ldr r2, [fp, #-0x4c]
ldrh r0, [r2, #-0x10]
ldrh r2, [r7, r3]
orr r2, r2, r0
strh r2, [r7, r3]
ldr r2, [fp, #-0x50]
ldrh r0, [r2, #-0x10]
ldrh r2, [r8, r3]
orr r2, r2, r0
strh r2, [r8, r3]
add r3, r3, #4
ldr r2, [fp, #-0x58]
cmp r3, r2
bne .L0x38d70

.L0x38ec4:
ldr r6, [fp, #-0x7c]
mov r1, sb
ldr r0, [fp, #-0x68]
mov r2, r6
bl memcpy
ldr r0, [fp, #-0x6c]
mov r2, r6
mov r1, r5
bl memcpy
ldr r0, [fp, #-0x64]
mov r2, r6
mov r1, sl
bl memcpy
ldr r0, [fp, #-0x60]
mov r2, r6
mov r1, r7
bl memcpy
mov r2, r6
mov r1, r8
ldr r6, [fp, #-0x70]
mov r0, r6
bl memcpy
ldr r2, [fp, #-0x74]
ldr r1, [fp, #-0x84]
add r2, r2, r1
str r2, [fp, #-0x74]
ldr r2, [fp, #-0x78]
add r3, r2, #1
ldr r2, [fp, #-0x80]
str r3, [fp, #-0x78]
cmp r3, r2
ldr r3, [fp, #-0x68]
add r2, r3, #0x410
ldr r3, [fp, #-0x6c]
str r2, [fp, #-0x68]
add r2, r3, #0x410
ldr r3, [fp, #-0x64]
str r2, [fp, #-0x6c]
add r2, r3, #0x410
ldr r3, [fp, #-0x60]
str r2, [fp, #-0x64]
add r3, r3, #0x410
str r3, [fp, #-0x60]
add r3, r6, #0x410
str r3, [fp, #-0x70]
bls .L0x38cb8

.L0x38f7c:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x38f88:
sub r1, r1, lr
cmp r2, r6
add lr, r1, #1
streq r1, [fp, #-0x80]
udiv r3, lr, r3
mul r2, r3, r2
subne r3, r3, #1
addne r3, r3, r2
str r2, [fp, #-0x78]
strne r3, [fp, #-0x80]
b .L0x38b60

.L0x38fb4:
mov r3, #0
str r3, [fp, #-0x7c]
mov r1, r3
str r3, [fp, #-0x88]
mov lr, r3
mov r6, r3
b .L0x38be0

.L0x38fd0:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov ip, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x8c
ldr r4, [r1, #0x2c]
ldrsh lr, [r1, #0x28]
ldr r8, [r1, #0x14]
str r1, [fp, #-0x98]
mov r1, r0
ldr r0, [r4, #4]
ldr r6, [r4, #0xc]
mul r5, r0, r0
ldr r7, [ip, #0xc]
mov r0, #0x10
ldr r4, [r4, #0x10]
udiv r0, r0, r6
sdiv r0, lr, r0
mla r0, r5, r0, r0
cmp r8, r7
add r4, r4, r0, lsl #1
and r0, lr, #7
vldr d8, .LDIC0x390a8
vldr d9, .LDIC0x390b0
str r0, [fp, #-0xb4]
blt .L0x390b8
ldr lr, [ip, #0x10]
ldr r0, [ip, #0x18]
sub ip, r3, #1
cmp lr, r0
ble .L0x394dc
mvn ip, #0
mov r3, #0
str r3, [fp, #-0x74]

.L0x39058:
ldr r3, [fp, #-0x98]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x39504
sub r3, r8, r7
mov r5, #0xc
add r3, r3, #1
mov r2, #0x14
bic r6, r3, #7
lsr lr, r3, #3
mul r5, r5, lr
mul r2, r2, lr
lsl r3, lr, #5
lsl r0, lr, #4
str lr, [fp, #-0x94]
lsl lr, lr, #2
str lr, [fp, #-0x48]
b .L0x390dc

.LDIC0x390a8:
.quad -1407387768586240

.LDIC0x390b0:
.quad -3659221942468616

.L0x390b8:
mov r3, #0
mvn ip, #0
str r3, [fp, #-0x48]
mov r2, r3
str r3, [fp, #-0x94]
mov r0, r3
str r3, [fp, #-0x74]
mov r5, r3
mov r6, r3

.L0x390dc:
sub sp, sp, r3
cmp r7, #0
ldr r8, [fp, #-0x98]
add lr, r7, #7
ldr r3, [fp, #-0x74]
movge lr, r7
ldr r7, [r8, #0x10]
mov sb, sp
cmp ip, r3
ldr r3, [r8, #0x3c]
asr lr, lr, #3
ldr r8, [r3, #0x14]
add r3, r7, #3
ldr r7, [r1]
str r8, [fp, #-0x90]
str r7, [fp, #-0x9c]
add r7, sb, r0
ldr r0, [r1, #8]
add r8, sb, r2
str r0, [fp, #-0xa4]
ldr r0, [r1, #0xc]
ldr r2, [r1, #4]
str r0, [fp, #-0xa8]
ldr r0, [r1, #0x10]
str r2, [fp, #-0xa0]
mov r2, #0x104
str r0, [fp, #-0xac]
ldr r0, [r1, #0x14]
add r6, sb, r6
str r0, [fp, #-0xb0]
ldr r0, [fp, #-0x48]
add r5, sb, r5
mla r3, r2, r3, lr
add sl, sb, r0
blo .L0x394d0
ldr r1, [fp, #-0x74]
add r3, r3, #0x1a
str ip, [fp, #-0xb8]
mla r3, r2, r1, r3
lsl r2, r3, #2
str r2, [fp, #-0x6c]
ldr r2, [fp, #-0xb4]
add r3, r2, #1
sub r2, fp, #0x34
add r2, r2, r3, lsl #1
ldr r3, [fp, #-0x90]
str r2, [fp, #-0x68]
mul r3, r1, r3
str r3, [fp, #-0x70]
ldr r3, [fp, #-0x94]
lsl r3, r3, #2
str r3, [fp, #-0x64]

.L0x391ac:
ldr r3, [fp, #-0x6c]
mov r0, sb
ldr r2, [fp, #-0x9c]
add r2, r2, r3
str r2, [fp, #-0x78]
mov r1, r2
ldr r2, [fp, #-0x48]
bl memcpy
ldr r3, [fp, #-0x6c]
mov r0, sl
ldr r1, [fp, #-0xa0]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x7c]
bl memcpy
ldr r3, [fp, #-0x6c]
mov r0, r6
ldr r1, [fp, #-0xa4]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x80]
bl memcpy
ldr r3, [fp, #-0x6c]
mov r0, r5
ldr r1, [fp, #-0xa8]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x84]
bl memcpy
ldr r3, [fp, #-0x6c]
mov r0, r7
ldr r1, [fp, #-0xac]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x88]
bl memcpy
ldr r1, [fp, #-0xb0]
mov r0, r8
ldr r3, [fp, #-0x6c]
ldr r2, [fp, #-0x48]
add r3, r1, r3
str r3, [fp, #-0x8c]
mov r1, r3
bl memcpy
ldr r3, [fp, #-0x94]
cmp r3, #0
beq .L0x3943c
ldr r2, [fp, #-0xb4]
sub r1, fp, #0x34
ldr r0, [fp, #-0x98]
add r3, r2, #2
vmov.i16 q11, #3
add r3, r1, r3, lsl #1
str r3, [fp, #-0x50]
add r3, r2, #3
ldr lr, [r0, #0x44]
sub r0, fp, #0x44
add r3, r1, r3, lsl #1
str r3, [fp, #-0x54]
add r3, r2, #4
str r0, [fp, #-0x60]
sub r0, fp, #0x34
add r3, r1, r3, lsl #1
str r3, [fp, #-0x58]
add r3, r2, #5
add r2, r0, r2, lsl #1
add r3, r1, r3, lsl #1
str r3, [fp, #-0x5c]
ldr r3, [fp, #-0x70]
str r2, [fp, #-0x4c]
add r1, r3, #1
mov r3, #0
lsl r1, r1, #1

.L0x392d0:
ldrh ip, [lr, r1]
add r2, lr, r1
add r1, r1, #0x10
add ip, r4, ip, lsl #1
ldrh r0, [sb, r3]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r2, #-2]
add ip, r4, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r2, #0xa]
vand q9, q9, q11
add ip, r4, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #0xc]
vshl.i16 q12, q12, #0xe
add ip, r4, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r2, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, r4, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #6]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #4]
ldrh r2, [r2, #2]
vshl.u16 q10, q10, q4
add ip, r4, ip, lsl #1
add r2, r4, r2, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r2]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
ldr r2, [fp, #-0x60]
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [r2:0x40]
ldr r2, [fp, #-0x4c]
ldrh r2, [r2, #-0x10]
orr r0, r0, r2
ldr r2, [fp, #-0x68]
strh r0, [sb, r3]
ldrh r0, [r2, #-0x10]
ldrh r2, [sl, r3]
orr r2, r2, r0
strh r2, [sl, r3]
ldr r2, [fp, #-0x50]
ldrh r0, [r2, #-0x10]
ldrh r2, [r6, r3]
orr r2, r2, r0
strh r2, [r6, r3]
ldr r2, [fp, #-0x54]
ldrh r0, [r2, #-0x10]
ldrh r2, [r5, r3]
orr r2, r2, r0
strh r2, [r5, r3]
ldr r2, [fp, #-0x58]
ldrh r0, [r2, #-0x10]
ldrh r2, [r7, r3]
orr r2, r2, r0
ldr r0, [fp, #-0x5c]
strh r2, [r7, r3]
ldrh r0, [r0, #-0x10]
ldrh r2, [r8, r3]
orr r2, r2, r0
strh r2, [r8, r3]
add r3, r3, #4
ldr r2, [fp, #-0x64]
cmp r3, r2
bne .L0x392d0

.L0x3943c:
ldr r2, [fp, #-0x48]
mov r1, sb
ldr r0, [fp, #-0x78]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, sl
ldr r0, [fp, #-0x7c]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r6
ldr r0, [fp, #-0x80]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r5
ldr r0, [fp, #-0x84]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r7
ldr r0, [fp, #-0x88]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r8
ldr r0, [fp, #-0x8c]
bl memcpy
ldr r3, [fp, #-0x70]
ldr r2, [fp, #-0x90]
add r3, r3, r2
str r3, [fp, #-0x70]
ldr r3, [fp, #-0x74]
ldr r2, [fp, #-0xb8]
add r3, r3, #1
cmp r2, r3
str r3, [fp, #-0x74]
ldr r3, [fp, #-0x6c]
add r3, r3, #0x410
str r3, [fp, #-0x6c]
bhs .L0x391ac

.L0x394d0:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x394dc:
sub lr, r0, lr
cmp r2, ip
add r0, lr, #1
moveq ip, lr
udiv r3, r0, r3
mul r2, r3, r2
subne r3, r3, #1
addne ip, r3, r2
str r2, [fp, #-0x74]
b .L0x39058

.L0x39504:
mov r3, #0
str r3, [fp, #-0x48]
mov r2, r3
str r3, [fp, #-0x94]
mov r0, r3
mov r5, r3
mov r6, r3
b .L0x390dc

.L0x39524:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r8, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0x9c
ldr r4, [r1, #0x2c]
ldrsh lr, [r1, #0x28]
ldr ip, [r1, #0x14]
str r1, [fp, #-0xa8]
ldr r1, [r4, #4]
ldr r6, [r4, #0xc]
mul r5, r1, r1
ldr r7, [r8, #0xc]
mov r1, #0x10
ldr r4, [r4, #0x10]
udiv r1, r1, r6
sdiv r1, lr, r1
mla r1, r5, r1, r1
cmp ip, r7
add sb, r4, r1, lsl #1
and r1, lr, #7
vldr d8, .LDIC0x39600
vldr d9, .LDIC0x39608
str r1, [fp, #-0xc4]
blt .L0x39610
ldr lr, [r8, #0x10]
sub r4, r3, #1
ldr r1, [r8, #0x18]
cmp lr, r1
ble .L0x39a9c
mvn r3, #0
str r3, [fp, #-0x9c]
mov r3, #0
str r3, [fp, #-0x7c]

.L0x395ac:
ldr r3, [fp, #-0xa8]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x39ac8
sub r3, ip, r7
mov r5, #0xc
add r3, r3, #1
mov lr, #0x14
bic r6, r3, #7
mov r1, #0x18
lsr r2, r3, #3
mul r5, r5, r2
mul lr, lr, r2
mul r1, r1, r2
lsl r3, r2, #5
lsl r4, r2, #4
lsl ip, r2, #2
str r2, [fp, #-0xa4]
b .L0x3963c

.LDIC0x39600:
.quad -1407387768586240

.LDIC0x39608:
.quad -3659221942468616

.L0x39610:
mov ip, #0
mvn r3, #0
str r3, [fp, #-0x9c]
mov r1, ip
str ip, [fp, #-0xa4]
mov lr, ip
str ip, [fp, #-0x7c]
mov r4, ip
mov r5, ip
mov r6, ip
mov r3, ip

.L0x3963c:
sub sp, sp, r3
cmp r7, #0
ldr r8, [fp, #-0xa8]
add r3, r7, #7
ldr r2, [fp, #-0x7c]
movge r3, r7
ldr r7, [fp, #-0x9c]
mov sl, sp
add r1, sl, r1
cmp r7, r2
ldr r2, [r8, #0x3c]
ldr r7, [r8, #0x10]
ldr r8, [r2, #0x14]
str r1, [fp, #-0x4c]
add r2, r7, #3
str r8, [fp, #-0xa0]
mov r1, #0x104
add r8, sl, lr
ldr lr, [r0, #4]
ldr r7, [r0]
str lr, [fp, #-0xb0]
ldr lr, [r0, #8]
asr r3, r3, #3
str lr, [fp, #-0xb4]
ldr lr, [r0, #0xc]
str r7, [fp, #-0xac]
str lr, [fp, #-0xb8]
ldr lr, [r0, #0x10]
add r7, sl, r4
str lr, [fp, #-0xbc]
ldr lr, [r0, #0x14]
add r6, sl, r6
add r5, sl, r5
mla r3, r1, r2, r3
ldr r0, [r0, #0x18]
add r4, sl, ip
str lr, [fp, #-0xc0]
blo .L0x39a90
ldr r2, [fp, #-0x7c]
add r3, r3, #0x1a
str r0, [fp, #-0xc8]
mla r3, r1, r2, r3
str ip, [fp, #-0x48]
lsl r1, r3, #2
str r1, [fp, #-0x50]
ldr r1, [fp, #-0xc4]
add r3, r1, #1
sub r1, fp, #0x34
add r1, r1, r3, lsl #1
ldr r3, [fp, #-0xa0]
str r1, [fp, #-0x74]
mul r3, r2, r3
str r3, [fp, #-0x78]
ldr r3, [fp, #-0xa4]
lsl r3, r3, #2
str r3, [fp, #-0x70]

.L0x3971c:
ldr r3, [fp, #-0x50]
mov r0, sl
ldr r2, [fp, #-0xac]
add r2, r2, r3
str r2, [fp, #-0x80]
mov r1, r2
ldr r2, [fp, #-0x48]
bl memcpy
ldr r3, [fp, #-0x50]
mov r0, r4
ldr r1, [fp, #-0xb0]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x84]
bl memcpy
ldr r3, [fp, #-0x50]
mov r0, r6
ldr r1, [fp, #-0xb4]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x88]
bl memcpy
ldr r3, [fp, #-0x50]
mov r0, r5
ldr r1, [fp, #-0xb8]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x8c]
bl memcpy
ldr r3, [fp, #-0x50]
mov r0, r7
ldr r1, [fp, #-0xbc]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x90]
bl memcpy
ldr r3, [fp, #-0x50]
mov r0, r8
ldr r1, [fp, #-0xc0]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x94]
bl memcpy
ldr r1, [fp, #-0xc8]
ldr r3, [fp, #-0x50]
ldr r2, [fp, #-0x48]
add r3, r1, r3
ldr r0, [fp, #-0x4c]
mov r1, r3
str r3, [fp, #-0x98]
bl memcpy
ldr r3, [fp, #-0xa4]
cmp r3, #0
beq .L0x399ec
ldr r2, [fp, #-0xc4]
sub r1, fp, #0x34
ldr r0, [fp, #-0xa8]
add r3, r2, #2
vmov.i16 q11, #3
add r3, r1, r3, lsl #1
str r3, [fp, #-0x58]
add r3, r2, #3
ldr lr, [r0, #0x44]
sub r0, fp, #0x44
add r3, r1, r3, lsl #1
str r3, [fp, #-0x5c]
add r3, r2, #4
str r0, [fp, #-0x6c]
sub r0, fp, #0x34
add r3, r1, r3, lsl #1
str r3, [fp, #-0x60]
add r3, r2, #5
add r3, r1, r3, lsl #1
str r3, [fp, #-0x64]
add r3, r2, #6
add r2, r0, r2, lsl #1
add r3, r1, r3, lsl #1
str r3, [fp, #-0x68]
ldr r3, [fp, #-0x78]
str r2, [fp, #-0x54]
add r1, r3, #1
mov r3, #0
lsl r1, r1, #1

.L0x39868:
ldrh ip, [lr, r1]
add r2, lr, r1
add r1, r1, #0x10
add ip, sb, ip, lsl #1
ldrh r0, [sl, r3]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r2, #-2]
add ip, sb, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r2, #0xa]
vand q9, q9, q11
add ip, sb, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #0xc]
vshl.i16 q12, q12, #0xe
add ip, sb, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r2, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, sb, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #6]
vshl.u16 q10, q10, q4
add ip, sb, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #4]
ldrh r2, [r2, #2]
vshl.u16 q10, q10, q4
add ip, sb, ip, lsl #1
add r2, sb, r2, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r2]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
ldr r2, [fp, #-0x6c]
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [r2:0x40]
ldr r2, [fp, #-0x54]
ldrh r2, [r2, #-0x10]
orr r0, r0, r2
ldr r2, [fp, #-0x74]
strh r0, [sl, r3]
ldrh r0, [r2, #-0x10]
ldrh r2, [r4, r3]
orr r2, r2, r0
strh r2, [r4, r3]
ldr r2, [fp, #-0x58]
ldrh r0, [r2, #-0x10]
ldrh r2, [r6, r3]
orr r2, r2, r0
strh r2, [r6, r3]
ldr r2, [fp, #-0x5c]
ldrh r0, [r2, #-0x10]
ldrh r2, [r5, r3]
ldr ip, [fp, #-0x4c]
orr r2, r2, r0
strh r2, [r5, r3]
ldr r2, [fp, #-0x60]
ldrh r0, [r2, #-0x10]
ldrh r2, [r7, r3]
orr r2, r2, r0
ldr r0, [fp, #-0x64]
strh r2, [r7, r3]
ldrh r0, [r0, #-0x10]
ldrh r2, [r8, r3]
orr r2, r2, r0
strh r2, [r8, r3]
ldr r2, [fp, #-0x68]
ldrh r0, [r2, #-0x10]
ldrh r2, [ip, r3]
orr r2, r2, r0
strh r2, [ip, r3]
add r3, r3, #4
ldr r2, [fp, #-0x70]
cmp r3, r2
bne .L0x39868

.L0x399ec:
ldr r2, [fp, #-0x48]
mov r1, sl
ldr r0, [fp, #-0x80]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r4
ldr r0, [fp, #-0x84]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r6
ldr r0, [fp, #-0x88]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r5
ldr r0, [fp, #-0x8c]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r7
ldr r0, [fp, #-0x90]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r8
ldr r0, [fp, #-0x94]
bl memcpy
ldr r2, [fp, #-0x48]
ldr r1, [fp, #-0x4c]
ldr r0, [fp, #-0x98]
bl memcpy
ldr r3, [fp, #-0x78]
ldr r2, [fp, #-0xa0]
add r3, r3, r2
str r3, [fp, #-0x78]
ldr r3, [fp, #-0x7c]
ldr r2, [fp, #-0x9c]
add r3, r3, #1
cmp r2, r3
str r3, [fp, #-0x7c]
ldr r3, [fp, #-0x50]
add r3, r3, #0x410
str r3, [fp, #-0x50]
bhs .L0x3971c

.L0x39a90:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x39a9c:
sub r1, r1, lr
cmp r2, r4
add lr, r1, #1
streq r1, [fp, #-0x9c]
udiv r3, lr, r3
mul r2, r3, r2
subne r3, r3, #1
addne r3, r3, r2
str r2, [fp, #-0x7c]
strne r3, [fp, #-0x9c]
b .L0x395ac

.L0x39ac8:
mov ip, #0
str ip, [fp, #-0xa4]
mov r1, ip
mov lr, ip
mov r4, ip
mov r5, ip
mov r6, ip
mov r3, ip
b .L0x3963c

.L0x39aec:
push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
mov r8, r1
vpush {d8, d9}
add fp, sp, #0x30
sub sp, sp, #0xac
ldr r4, [r1, #0x2c]
ldrsh lr, [r1, #0x28]
ldr ip, [r1, #0x14]
str r1, [fp, #-0xb4]
ldr r1, [r4, #4]
ldr r6, [r4, #0xc]
mul r5, r1, r1
ldr r7, [r8, #0xc]
mov r1, #0x10
ldr r4, [r4, #0x10]
udiv r1, r1, r6
sdiv r1, lr, r1
mla r1, r5, r1, r1
cmp ip, r7
add sl, r4, r1, lsl #1
and r1, lr, #7
vldr d8, .LDIC0x39bd8
vldr d9, .LDIC0x39be0
str r1, [fp, #-0xd4]
blt .L0x39be8
ldr lr, [r8, #0x10]
sub r4, r3, #1
ldr r1, [r8, #0x18]
cmp lr, r1
ble .L0x3a0ec
mvn r3, #0
str r3, [fp, #-0xa8]
mov r3, #0
str r3, [fp, #-0x84]

.L0x39b74:
ldr r3, [fp, #-0xb4]
ldr r2, [r3, #0x18]
ldr r3, [r3, #0x10]
cmp r2, r3
blt .L0x3a118
sub r3, ip, r7
mov r4, #0xc
add r3, r3, #1
mov r1, #0x14
bic r5, r3, #7
lsr r2, r3, #3
mov r3, #0x18
mul r3, r3, r2
mul r4, r4, r2
str r3, [fp, #-0x4c]
mov r3, #0x1c
mul ip, r3, r2
mul r1, r1, r2
lsl r3, r2, #5
lsl lr, r2, #4
str r2, [fp, #-0xb0]
lsl r2, r2, #2
str r2, [fp, #-0x48]
b .L0x39c1c

.LDIC0x39bd8:
.quad -1407387768586240

.LDIC0x39be0:
.quad -3659221942468616

.L0x39be8:
mov r2, #0
mvn r3, #0
str r3, [fp, #-0xa8]
mov ip, r2
str r2, [fp, #-0x48]
mov r1, r2
str r2, [fp, #-0x4c]
mov lr, r2
str r2, [fp, #-0xb0]
mov r4, r2
str r2, [fp, #-0x84]
mov r5, r2
mov r3, r2

.L0x39c1c:
ldr r6, [fp, #-0xa8]
cmp r7, #0
ldr r2, [fp, #-0x84]
sub sp, sp, r3
add r3, r7, #7
movge r3, r7
cmp r6, r2
ldr r6, [fp, #-0xb4]
asr r3, r3, #3
ldr r7, [r6, #0x10]
ldr r2, [r6, #0x3c]
ldr r6, [r2, #0x14]
add r2, r7, #3
ldr r7, [r0]
str r6, [fp, #-0xac]
str r7, [fp, #-0xb8]
mov r7, sp
add r8, r7, r1
ldr r1, [fp, #-0x4c]
add sb, r7, r4
add r1, r7, r1
str r1, [fp, #-0x4c]
add r1, r7, ip
ldr ip, [r0, #8]
str r1, [fp, #-0x50]
str ip, [fp, #-0xc0]
ldr ip, [r0, #0xc]
ldr r1, [r0, #4]
str ip, [fp, #-0xc4]
ldr ip, [r0, #0x10]
str r1, [fp, #-0xbc]
mov r1, #0x104
str ip, [fp, #-0xc8]
ldr ip, [r0, #0x14]
add r5, r7, r5
str ip, [fp, #-0xcc]
ldr ip, [r0, #0x18]
add r6, r7, lr
str ip, [fp, #-0xd0]
ldr ip, [fp, #-0x48]
mla r3, r1, r2, r3
ldr r0, [r0, #0x1c]
add r4, r7, ip
blo .L0x3a0e0
ldr r2, [fp, #-0x84]
add r3, r3, #0x1a
str r0, [fp, #-0xd8]
mla r3, r1, r2, r3
lsl r1, r3, #2
str r1, [fp, #-0x54]
ldr r1, [fp, #-0xd4]
add r3, r1, #1
sub r1, fp, #0x34
add r1, r1, r3, lsl #1
ldr r3, [fp, #-0xac]
str r1, [fp, #-0x7c]
mul r3, r2, r3
str r3, [fp, #-0x80]
ldr r3, [fp, #-0xb0]
lsl r3, r3, #2
str r3, [fp, #-0x78]
mov r3, r4
mov r4, r7
mov r7, r3

.L0x39d1c:
ldr r3, [fp, #-0x54]
mov r0, r4
ldr r2, [fp, #-0xb8]
add r2, r2, r3
str r2, [fp, #-0x88]
mov r1, r2
ldr r2, [fp, #-0x48]
bl memcpy
ldr r3, [fp, #-0x54]
mov r0, r7
ldr r1, [fp, #-0xbc]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x8c]
bl memcpy
ldr r3, [fp, #-0x54]
mov r0, r5
ldr r1, [fp, #-0xc0]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x90]
bl memcpy
ldr r3, [fp, #-0x54]
mov r0, sb
ldr r1, [fp, #-0xc4]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x94]
bl memcpy
ldr r3, [fp, #-0x54]
mov r0, r6
ldr r1, [fp, #-0xc8]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x98]
bl memcpy
ldr r3, [fp, #-0x54]
mov r0, r8
ldr r1, [fp, #-0xcc]
ldr r2, [fp, #-0x48]
add r1, r1, r3
str r1, [fp, #-0x9c]
bl memcpy
ldr r3, [fp, #-0x54]
ldr r1, [fp, #-0xd0]
ldr r2, [fp, #-0x48]
add r1, r1, r3
ldr r0, [fp, #-0x4c]
str r1, [fp, #-0xa0]
bl memcpy
ldr r1, [fp, #-0xd8]
ldr r3, [fp, #-0x54]
ldr r2, [fp, #-0x48]
add r3, r1, r3
ldr r0, [fp, #-0x50]
mov r1, r3
str r3, [fp, #-0xa4]
bl memcpy
ldr r3, [fp, #-0xb0]
cmp r3, #0
beq .L0x3a02c
ldr r2, [fp, #-0xd4]
sub r1, fp, #0x34
ldr r0, [fp, #-0xb4]
add r3, r2, #2
vmov.i16 q11, #3
add r3, r1, r3, lsl #1
str r3, [fp, #-0x5c]
add r3, r2, #3
ldr lr, [r0, #0x44]
sub r0, fp, #0x44
add r3, r1, r3, lsl #1
str r3, [fp, #-0x60]
add r3, r2, #4
str r0, [fp, #-0x74]
sub r0, fp, #0x34
add r3, r1, r3, lsl #1
str r3, [fp, #-0x64]
add r3, r2, #5
add r3, r1, r3, lsl #1
str r3, [fp, #-0x68]
add r3, r2, #6
add r3, r1, r3, lsl #1
str r3, [fp, #-0x6c]
add r3, r2, #7
add r2, r0, r2, lsl #1
add r3, r1, r3, lsl #1
str r3, [fp, #-0x70]
ldr r3, [fp, #-0x80]
str r2, [fp, #-0x58]
add r1, r3, #1
mov r3, #0
lsl r1, r1, #1

.L0x39e90:
ldrh ip, [lr, r1]
add r2, lr, r1
add r1, r1, #0x10
add ip, sl, ip, lsl #1
ldrh r0, [r4, r3]
vld1.16 {d18[], d19[]}, [ip]
ldrh ip, [r2, #-2]
add ip, sl, ip, lsl #1
vshl.u16 q9, q9, q4
vld1.16 {d24[], d25[]}, [ip]
ldrh ip, [r2, #0xa]
vand q9, q9, q11
add ip, sl, ip, lsl #1
vshl.u16 q12, q12, q4
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #0xc]
vshl.i16 q12, q12, #0xe
add ip, sl, ip, lsl #1
vshl.u16 q10, q10, q4
vld1.16 {d16[], d17[]}, [ip]
vand q10, q10, q11
vshl.u16 q8, q8, q4
ldrh ip, [r2, #8]
vshl.i16 q10, q10, #2
vand q8, q8, q11
add ip, sl, ip, lsl #1
vshl.i16 q9, q9, #0xc
vadd.i16 q8, q8, q12
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #6]
vshl.u16 q10, q10, q4
add ip, sl, ip, lsl #1
vand q10, q10, q11
vshl.i16 q10, q10, #4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [ip]
ldrh ip, [r2, #4]
ldrh r2, [r2, #2]
vshl.u16 q10, q10, q4
add ip, sl, ip, lsl #1
add r2, sl, r2, lsl #1
vand q10, q10, q11
vld1.16 {d24[], d25[]}, [ip]
vshl.i16 q10, q10, #6
vshl.u16 q12, q12, q4
vadd.i16 q8, q8, q10
vld1.16 {d20[], d21[]}, [r2]
vand q12, q12, q11
vshl.u16 q10, q10, q4
vshl.i16 q12, q12, #8
vand q10, q10, q11
vadd.i16 q8, q8, q12
vshl.i16 q10, q10, #0xa
ldr r2, [fp, #-0x74]
vadd.i16 q8, q8, q10
vadd.i16 q8, q8, q9
vst1.16 {d16, d17}, [r2:0x40]
ldr r2, [fp, #-0x58]
ldrh r2, [r2, #-0x10]
orr r0, r0, r2
ldr r2, [fp, #-0x7c]
strh r0, [r4, r3]
ldrh r0, [r2, #-0x10]
ldrh r2, [r7, r3]
orr r2, r2, r0
strh r2, [r7, r3]
ldr r2, [fp, #-0x5c]
ldrh r0, [r2, #-0x10]
ldrh r2, [r5, r3]
orr r2, r2, r0
strh r2, [r5, r3]
ldr r2, [fp, #-0x60]
ldrh r0, [r2, #-0x10]
ldrh r2, [sb, r3]
ldr ip, [fp, #-0x4c]
orr r2, r2, r0
strh r2, [sb, r3]
ldr r2, [fp, #-0x64]
ldrh r0, [r2, #-0x10]
ldrh r2, [r6, r3]
orr r2, r2, r0
ldr r0, [fp, #-0x68]
strh r2, [r6, r3]
ldrh r0, [r0, #-0x10]
ldrh r2, [r8, r3]
orr r2, r2, r0
strh r2, [r8, r3]
ldr r2, [fp, #-0x6c]
ldrh r0, [r2, #-0x10]
ldrh r2, [ip, r3]
orr r2, r2, r0
strh r2, [ip, r3]
ldr r2, [fp, #-0x70]
ldr ip, [fp, #-0x50]
ldrh r0, [r2, #-0x10]
ldrh r2, [ip, r3]
orr r2, r2, r0
strh r2, [ip, r3]
add r3, r3, #4
ldr r2, [fp, #-0x78]
cmp r3, r2
bne .L0x39e90

.L0x3a02c:
ldr r2, [fp, #-0x48]
mov r1, r4
ldr r0, [fp, #-0x88]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r7
ldr r0, [fp, #-0x8c]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r5
ldr r0, [fp, #-0x90]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, sb
ldr r0, [fp, #-0x94]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r6
ldr r0, [fp, #-0x98]
bl memcpy
ldr r2, [fp, #-0x48]
mov r1, r8
ldr r0, [fp, #-0x9c]
bl memcpy
ldr r2, [fp, #-0x48]
ldr r1, [fp, #-0x4c]
ldr r0, [fp, #-0xa0]
bl memcpy
ldr r2, [fp, #-0x48]
ldr r1, [fp, #-0x50]
ldr r0, [fp, #-0xa4]
bl memcpy
ldr r3, [fp, #-0x80]
ldr r2, [fp, #-0xac]
add r3, r3, r2
str r3, [fp, #-0x80]
ldr r3, [fp, #-0x84]
ldr r2, [fp, #-0xa8]
add r3, r3, #1
cmp r2, r3
str r3, [fp, #-0x84]
ldr r3, [fp, #-0x54]
add r3, r3, #0x410
str r3, [fp, #-0x54]
bhs .L0x39d1c

.L0x3a0e0:
sub sp, fp, #0x30
vpop {d8, d9}
pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}

.L0x3a0ec:
sub r1, r1, lr
cmp r2, r4
add lr, r1, #1
streq r1, [fp, #-0xa8]
udiv r3, lr, r3
mul r2, r3, r2
subne r3, r3, #1
addne r3, r3, r2
str r2, [fp, #-0x84]
strne r3, [fp, #-0xa8]
b .L0x39b74

.L0x3a118:
mov r3, #0
str r3, [fp, #-0x48]
mov ip, r3
str r3, [fp, #-0x4c]
mov r1, r3
str r3, [fp, #-0xb0]
mov lr, r3
mov r4, r3
mov r5, r3
b .L0x39c1c

playback_kernel_plain:
str lr, [sp, #-4]!
mov ip, r1
ldr lr, [sp, #4]
mov r1, r3
cmp r2, #8
addls pc, pc, r2, lsl #2
b .L0x3a230
b .L0x3a194
b .L0x3a1a4
b .L0x3a1b8
b .L0x3a1cc
b .L0x3a1e0
b .L0x3a1f4
b .L0x3a208
b .L0x3a21c
b .L0x3a180

.L0x3a180:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x39aec

.L0x3a194:
mov r2, lr
mov r0, ip
pop {lr}
b .L0x379f0

.L0x3a1a4:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x37c08

.L0x3a1b8:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x37ee8

.L0x3a1cc:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x38268

.L0x3a1e0:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x38660

.L0x3a1f4:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x38ad0

.L0x3a208:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x38fd0

.L0x3a21c:
mov r3, lr
mov r2, r1
pop {lr}
mov r1, ip
b .L0x39524

.L0x3a230:
pop {pc}

.section .note.GNU-stack,""
