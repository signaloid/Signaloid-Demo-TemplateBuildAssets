/*
 *	Copyright (c) 2025, Signaloid.
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

.globl	_init
.globl	__errno
.align	4

.section .text.unlikely

_init:
.option push
.option norelax
	li	sp, 0x0020000	# 128K
	la	gp, __global_pointer$
.option pop
	# Clear the bss segment
	la      a0, _edata
	la      a2, _end
	sub     a2, a2, a0
	li      a1, 0
	call    memset

	# Push a0 and a1 onto the stack to save argc and argv of main
	# The RISC-V psABI requires 16-byte alignment at function call boundaries
	addi    sp,sp,-16
	sw      a0,12(sp)
	sw      a1,8(sp)

	# Call initialization function
	call	_start

	# Pop a0 and a1 off the stack to restore argc and argv of main
	lw      a0,12(sp)
	lw      a1,8(sp)
	addi    sp,sp,16

	call	main
	call	exit

errnoValue:	.word 0

__errno:
	la	a0, errnoValue
	ret
