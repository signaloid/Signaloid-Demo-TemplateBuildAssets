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

	.equ	kDmaOpInvalid,	0
	.equ	kDmaOpCopy,	1
	.equ	kDmaOpByteSet,	3

	.equ	kDmaStateIdle,	0
	.equ	kDmaStateDone,	2
	.equ	kDmaStateError,	3

	.equ	kDmaIdleWaitIterations,	4000000

	.equ	kConfigDmaWedge, 0x0000003D

	.equ	kMstatusFsDirty, 0x6000

	.section .boot, "ax"
	.balign	4
	.globl	_init
_init:

	li	t0, (0x10000000)
	sw	zero, 0(t0)
	li	t0, (0x10008000)
	sw	zero, 0(t0)

	li	t4, (0x20000000)

	li	t5, kDmaOpInvalid
	sw	t5,   ((0x20000018) - (0x20000000))(t4)
	li	t5, -1
	sw	t5,   ((0x20000004)     - (0x20000000))(t4)
	sw	zero, ((0x20000004)     - (0x20000000))(t4)

	li	t6, kDmaIdleWaitIterations
.Ldma_claim_poll:
	li	t5, -1
	sw	t5, ((0x20000004)         - (0x20000000))(t4)
	lw	t5, ((0x20000008)      - (0x20000000))(t4)
	srli	t5, t5, 24
	andi	t5, t5, 0x3
	beqz	t5, .Ldma_engine_idle
	addi	t6, t6, -1
	bnez	t6, .Ldma_claim_poll

.Ldma_wedge_park:
	li	t5, kConfigDmaWedge
	li	t0, (0x10004000)
	sw	t5, 0(t0)
.Ldma_wedge_spin:
	j	.Ldma_wedge_spin

.Ldma_engine_idle:

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

	sw	t3, ((0x2000000c)      - (0x20000000))(t4)
	sw	t0, ((0x20000014) - (0x20000000))(t4)
	sw	t2, ((0x20000010)      - (0x20000000))(t4)

	li	t5, kDmaOpCopy
	sw	t5, ((0x20000018)     - (0x20000000))(t4)
1:

	beqz	t2, .Ldma_wait_done
2:
	lw	t5, ((0x20000008)      - (0x20000000))(t4)
	srli	t5, t5, 24
	andi	t5, t5, 0x3
	li	t6, kDmaStateError
	beq	t5, t6, .Ldma_wedge_park
	li	t6, kDmaStateDone
	bne	t5, t6, 2b

	li	t5, kDmaOpInvalid
	sw	t5,   ((0x20000018) - (0x20000000))(t4)
	li	t5, -1
	sw	t5,   ((0x20000004)     - (0x20000000))(t4)
	sw	zero, ((0x20000004)     - (0x20000000))(t4)
.Ldma_wait_done:

	la	t0, __bss_start
	la	t1, __bss_end
	sub	t2, t1, t0

	beqz	t2, .Lbss_done

	li	t6, kDmaIdleWaitIterations
.Lbss_idle_poll:
	lw	t5, ((0x20000008)      - (0x20000000))(t4)
	srli	t5, t5, 24
	andi	t5, t5, 0x3
	beqz	t5, .Lbss_engine_idle
	addi	t6, t6, -1
	bnez	t6, .Lbss_idle_poll
	j	.Ldma_wedge_park

.Lbss_engine_idle:

	sw	t2,   ((0x20000010)      - (0x20000000))(t4)
	sw	zero, ((0x2000001c)         - (0x20000000))(t4)
	sw	t0,   ((0x20000014) - (0x20000000))(t4)
	li	t5, kDmaOpByteSet
	sw	t5,   ((0x20000018)     - (0x20000000))(t4)

3:
	lw	t5, ((0x20000008)      - (0x20000000))(t4)
	srli	t5, t5, 24
	andi	t5, t5, 0x3
	li	t6, kDmaStateError
	beq	t5, t6, .Ldma_wedge_park
	li	t6, kDmaStateDone
	bne	t5, t6, 3b

	li	t5, kDmaOpInvalid
	sw	t5,   ((0x20000018) - (0x20000000))(t4)
	li	t5, -1
	sw	t5,   ((0x20000004)     - (0x20000000))(t4)
	sw	zero, ((0x20000004)     - (0x20000000))(t4)
.Lbss_done:

	.word	0x0000500f

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
4:
	j	4b

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
