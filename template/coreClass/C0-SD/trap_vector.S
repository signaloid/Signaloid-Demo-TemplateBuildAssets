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

	.equ	CONFIG_TRAP_VALUE,	0x0000001C

	.section .text.trap_handler, "ax"

	.balign	4
	.globl	trap_vector
trap_vector:

	csrr	t0, mepc
	li	t1, (0x10010004)
	sw	t0, 0(t1)

	csrr	t0, mtval
	li	t1, (0x10010008)
	sw	t0, 0(t1)

	csrr	t0, mcause
	li	t1, (0x10010000)
	sw	t0, 0(t1)

	li	t1, (0x10004000)
	li	t0, CONFIG_TRAP_VALUE
	sw	t0, 0(t1)

1:
	j	1b
