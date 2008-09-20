@ motion_comp_arm_s.S
@ Copyright (C) 2004 AGAWA Koji <i (AT) atty (DOT) jp>
@
@ This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
@ See http://libmpeg2.sourceforge.net/ for updates.
@
@ mpeg2dec is free software; you can redistribute it and/or modify
@ it under the terms of the GNU General Public License as published by
@ the Free Software Foundation; either version 2 of the License, or
@ (at your option) any later version.
@
@ mpeg2dec is distributed in the hope that it will be useful,
@ but WITHOUT ANY WARRANTY; without even the implied warranty of
@ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
@ GNU General Public License for more details.
@
@ You should have received a copy of the GNU General Public License
@ along with mpeg2dec; if not, write to the Free Software
@ Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


	.text

@ ----------------------------------------------------------------
	.align
	.global MC_put_o_16_arm
MC_put_o_16_arm:
	@@ void func(uint8_t * dest, const uint8_t * ref, int stride, int height)
	pld [r1]
        stmfd sp!, {r4-r11, lr} @ R14 is also called LR
	and r4, r1, #3
	adr r5, MC_put_o_16_arm_align_jt
	add r5, r5, r4, lsl #2
	ldr pc, [r5]

MC_put_o_16_arm_align0:
	ldmia r1, {r4-r7}
	add r1, r1, r2
	pld [r1]
	stmia r0, {r4-r7}
	subs r3, r3, #1
	add r0, r0, r2
	bne MC_put_o_16_arm_align0
        ldmfd sp!, {r4-r11, pc} @@ update PC with LR content.

.macro	PROC shift
	ldmia r1, {r4-r8}
	add r1, r1, r2
	mov r9, r4, lsr #(\shift)
	pld [r1]
	mov r10, r5, lsr #(\shift)
	orr r9, r9, r5, lsl #(32-\shift)
	mov r11, r6, lsr #(\shift)
	orr r10, r10, r6, lsl #(32-\shift)
	mov r12, r7, lsr #(\shift)
	orr r11, r11, r7, lsl #(32-\shift)
	orr r12, r12, r8, lsl #(32-\shift)
	stmia r0, {r9-r12}
	subs r3, r3, #1
	add r0, r0, r2
.endm

MC_put_o_16_arm_align1:
	and r1, r1, #0xFFFFFFFC
1:	PROC(8)
	bne 1b
        ldmfd sp!, {r4-r11, pc} @@ update PC with LR content.
MC_put_o_16_arm_align2:
	and r1, r1, #0xFFFFFFFC
1:	PROC(16)
	bne 1b
        ldmfd sp!, {r4-r11, pc} @@ update PC with LR content.
MC_put_o_16_arm_align3:
	and r1, r1, #0xFFFFFFFC
1:	PROC(24)
	bne 1b
        ldmfd sp!, {r4-r11, pc} @@ update PC with LR content.
MC_put_o_16_arm_align_jt:
	.word MC_put_o_16_arm_align0
	.word MC_put_o_16_arm_align1
	.word MC_put_o_16_arm_align2
	.word MC_put_o_16_arm_align3

@ ----------------------------------------------------------------
	.align
	.global MC_put_o_8_arm
MC_put_o_8_arm:
	@@ void func(uint8_t * dest, const uint8_t * ref, int stride, int height)
	pld [r1]
        stmfd sp!, {r4-r10, lr} @ R14 is also called LR
	and r4, r1, #3
	adr r5, MC_put_o_8_arm_align_jt
	add r5, r5, r4, lsl #2
	ldr pc, [r5]
MC_put_o_8_arm_align0:
	ldmia r1, {r4-r5}
	add r1, r1, r2
	pld [r1]
	stmia r0, {r4-r5}
	add r0, r0, r2
	subs r3, r3, #1
	bne MC_put_o_8_arm_align0
        ldmfd sp!, {r4-r10, pc} @@ update PC with LR content.

.macro	PROC8 shift
	ldmia r1, {r4-r6}
	add r1, r1, r2
	mov r9, r4, lsr #(\shift)
	pld [r1]
	mov r10, r5, lsr #(\shift)
	orr r9, r9, r5, lsl #(32-\shift)
	orr r10, r10, r6, lsl #(32-\shift)
	stmia r0, {r9-r10}
	subs r3, r3, #1
	add r0, r0, r2
.endm

MC_put_o_8_arm_align1:
	and r1, r1, #0xFFFFFFFC
1:	PROC8(8)
	bne 1b
        ldmfd sp!, {r4-r10, pc} @@ update PC with LR content.

MC_put_o_8_arm_align2:
	and r1, r1, #0xFFFFFFFC
1:	PROC8(16)
	bne 1b
        ldmfd sp!, {r4-r10, pc} @@ update PC with LR content.

MC_put_o_8_arm_align3:
	and r1, r1, #0xFFFFFFFC
1:	PROC8(24)
	bne 1b
        ldmfd sp!, {r4-r10, pc} @@ update PC with LR content.

MC_put_o_8_arm_align_jt:
	.word MC_put_o_8_arm_align0
	.word MC_put_o_8_arm_align1
	.word MC_put_o_8_arm_align2
	.word MC_put_o_8_arm_align3

@ ----------------------------------------------------------------
.macro	AVG_PW rW1, rW2
	mov \rW2, \rW2, lsl #24
	orr \rW2, \rW2, \rW1, lsr #8
	eor r9, \rW1, \rW2
	and \rW2, \rW1, \rW2
	and r10, r9, r12
	add \rW2, \rW2, r10, lsr #1
	and r10, r9, r11
	add \rW2, \rW2, r10
.endm

	.align
	.global MC_put_x_16_arm
MC_put_x_16_arm:
	@@ void func(uint8_t * dest, const uint8_t * ref, int stride, int height)
	pld [r1]
        stmfd sp!, {r4-r11,lr} @ R14 is also called LR
	and r4, r1, #3
	adr r5, MC_put_x_16_arm_align_jt
	ldr r11, [r5]
	mvn r12, r11
	add r5, r5, r4, lsl #2
	ldr pc, [r5, #4]

.macro	ADJ_ALIGN_QW shift, R0, R1, R2, R3, R4
	mov \R0, \R0, lsr #(\shift)
	orr \R0, \R0, \R1, lsl #(32 - \shift)
	mov \R1, \R1, lsr #(\shift)
	orr \R1, \R1, \R2, lsl #(32 - \shift)
	mov \R2, \R2, lsr #(\shift)
	orr \R2, \R2, \R3, lsl #(32 - \shift)
	mov \R3, \R3, lsr #(\shift)
	orr \R3, \R3, \R4, lsl #(32 - \shift)
	mov \R4, \R4, lsr #(\shift)
@	and \R4, \R4, #0xFF
.endm

MC_put_x_16_arm_align0:
	ldmia r1, {r4-r8}
	add r1, r1, r2
	pld [r1]
	AVG_PW r7, r8
	AVG_PW r6, r7
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r8}
	subs r3, r3, #1
	add r0, r0, r2
	bne MC_put_x_16_arm_align0
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_16_arm_align1:
	and r1, r1, #0xFFFFFFFC
1:	ldmia r1, {r4-r8}
	add r1, r1, r2
	pld [r1]
	ADJ_ALIGN_QW 8, r4, r5, r6, r7, r8
	AVG_PW r7, r8
	AVG_PW r6, r7
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r8}
	subs r3, r3, #1
	add r0, r0, r2
	bne 1b
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_16_arm_align2:
	and r1, r1, #0xFFFFFFFC
1:	ldmia r1, {r4-r8}
	add r1, r1, r2
	pld [r1]
	ADJ_ALIGN_QW 16, r4, r5, r6, r7, r8
	AVG_PW r7, r8
	AVG_PW r6, r7
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r8}
	subs r3, r3, #1
	add r0, r0, r2
	bne 1b
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_16_arm_align3:
	and r1, r1, #0xFFFFFFFC
1:	ldmia r1, {r4-r8}
	add r1, r1, r2
	pld [r1]
	ADJ_ALIGN_QW 24, r4, r5, r6, r7, r8
	AVG_PW r7, r8
	AVG_PW r6, r7
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r8}
	subs r3, r3, #1
	add r0, r0, r2
	bne 1b
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_16_arm_align_jt:
	.word 0x01010101
	.word MC_put_x_16_arm_align0
	.word MC_put_x_16_arm_align1
	.word MC_put_x_16_arm_align2
	.word MC_put_x_16_arm_align3

@ ----------------------------------------------------------------
	.align
	.global MC_put_x_8_arm
MC_put_x_8_arm:
	@@ void func(uint8_t * dest, const uint8_t * ref, int stride, int height)
	pld [r1]
        stmfd sp!, {r4-r11,lr} @ R14 is also called LR
	and r4, r1, #3
	adr r5, MC_put_x_8_arm_align_jt
	ldr r11, [r5]
	mvn r12, r11
	add r5, r5, r4, lsl #2
	ldr pc, [r5, #4]

.macro	ADJ_ALIGN_DW shift, R0, R1, R2
	mov \R0, \R0, lsr #(\shift)
	orr \R0, \R0, \R1, lsl #(32 - \shift)
	mov \R1, \R1, lsr #(\shift)
	orr \R1, \R1, \R2, lsl #(32 - \shift)
	mov \R2, \R2, lsr #(\shift)
@	and \R4, \R4, #0xFF
.endm

MC_put_x_8_arm_align0:
	ldmia r1, {r4-r6}
	add r1, r1, r2
	pld [r1]
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r6}
	subs r3, r3, #1
	add r0, r0, r2
	bne MC_put_x_8_arm_align0
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_8_arm_align1:
	and r1, r1, #0xFFFFFFFC
1:	ldmia r1, {r4-r6}
	add r1, r1, r2
	pld [r1]
	ADJ_ALIGN_DW 8, r4, r5, r6
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r6}
	subs r3, r3, #1
	add r0, r0, r2
	bne 1b
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_8_arm_align2:
	and r1, r1, #0xFFFFFFFC
1:	ldmia r1, {r4-r6}
	add r1, r1, r2
	pld [r1]
	ADJ_ALIGN_DW 16, r4, r5, r6
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r6}
	subs r3, r3, #1
	add r0, r0, r2
	bne 1b
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_8_arm_align3:
	and r1, r1, #0xFFFFFFFC
1:	ldmia r1, {r4-r6}
	add r1, r1, r2
	pld [r1]
	ADJ_ALIGN_DW 24, r4, r5, r6
	AVG_PW r5, r6
	AVG_PW r4, r5
	stmia r0, {r5-r6}
	subs r3, r3, #1
	add r0, r0, r2
	bne 1b
        ldmfd sp!, {r4-r11,pc} @@ update PC with LR content.
MC_put_x_8_arm_align_jt:
	.word 0x01010101
	.word MC_put_x_8_arm_align0
	.word MC_put_x_8_arm_align1
	.word MC_put_x_8_arm_align2
	.word MC_put_x_8_arm_align3
