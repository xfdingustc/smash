/*******************************************************************************
**      mem_function.asm
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a Jul-21-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/
.arch armv5t
.fpu softvfp

.section code 

.text
.align 2

.global Wmemset

#-----------------------------------------------------------------------------
# Name: 	Wmemset
# Purpose: word memset, 2 bytes
# Params:
#	r0 = address
#	r1 = length
#	r2 = count
# 	r3 = value to write
#-----------------------------------------------------------------------------
Wmemset:
	stmfd	sp!,{r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

# r4 = temp
# r5 = temp

	and	r1, #0xffffff80
	mov	r4, r0
	mov	r5, r1

	mov	r6, r3
	mov	r7, r3
	mov	r8, r3
	mov	r9, r3
	mov	r10, r3
	mov	r11, r3
	mov	r12, r3

.L0:
	mov	r0, r4
	mov	r1, r5

.L1:
# Does 64 transfers, 4 bytes each = 256 bytes total.
# The "stmia" instruction automatically increments r0.
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }

	sub	r1, #256
	cmp	r1, #0
	bne	.L1

	sub	r2, #1
	cmp	r2, #0
	bne	.L0

# return.
	ldmfd	sp!,{r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}
