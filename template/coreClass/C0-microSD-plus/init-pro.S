/*
 *	Copyright (c) 2026, Signaloid.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a copy
 *	of this software and associated documentation files (the "Software"), to deal
 *	in the Software without restriction, including without limitation the rights
 *	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	copies of the Software, and to permit persons to whom the Software is
 *	furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in all
 *	copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *	SOFTWARE.
 */

	.equ	kDmaOpCopy,	1
	.equ	kDmaOpStop,	0
	.equ	kDmaStateDone,	2

	.equ	kMstatusFsDirty, 0x6000

	.section .boot, "ax"
	.balign	4
	.globl	_init
_init:

	li	t0, (0x8200000)
	sw	zero, 0(t0)
	li	t0, (0x8208000)
	sw	zero, 0(t0)

.option push
.option norelax
	la	sp, __stack_top
	la	gp, __global_pointer$
.option pop

	la	t0, __copy_vma_start
	la	t1, __copy_vma_end
	sub	t2, t1, t0
	beqz	t2, 1f

	la	t3, __copy_lma
	li	t4, (0x10000000)

	sw	t3, ((0x1000000c)      - (0x10000000))(t4)
	sw	t0, ((0x10000014) - (0x10000000))(t4)
	sw	t2, ((0x10000010)      - (0x10000000))(t4)

	li	t5, kDmaOpCopy
	sw	t5, ((0x10000018)     - (0x10000000))(t4)
1:

	la	t0, __bss_start
	la	t1, __bss_end
2:
	bgeu	t0, t1, 3f
	sw	zero, 0(t0)
	addi	t0, t0, 4
	j	2b
3:

	beqz	t2, .Ldma_wait_done
4:
	lw	t5, ((0x10000008)      - (0x10000000))(t4)
	srli	t5, t5, 24
	andi	t5, t5, 0x3
	li	t6, kDmaStateDone
	bne	t5, t6, 4b

	li	t5, kDmaOpStop
	sw	t5,   ((0x10000018) - (0x10000000))(t4)
	li	t5, -1
	sw	t5,   ((0x10000004)     - (0x10000000))(t4)
	sw	zero, ((0x10000004)     - (0x10000000))(t4)
.Ldma_wait_done:

	la	t0, trap_vector
	csrw	mtvec, t0

	li	t0, kMstatusFsDirty
	csrs	mstatus, t0

	la	t0, _start
	jalr	t0

	la	t0, main
	jalr	t0

	la	t0, exit
	jalr	t0
5:
	j	5b

	.section .data
	.balign	4
errnoValue:
	.word	0

	.section .text
	.balign	4
	.globl	__errno
__errno:
	la	a0, errnoValue
	ret
