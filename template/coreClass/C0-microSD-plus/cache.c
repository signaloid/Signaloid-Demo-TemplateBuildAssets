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

/*
 *	The cache geometry, the cacheable range, the write-through data-cache policy
 *	and the set of maintenance operations are as documented for the RISC-V RX
 *	core in Lattice FPGA-IPUG-02302 sec. 2.2.1.8.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cache.h"

/*
 *	The two data-cache maintenance operations are RX-core custom instructions in
 *	the MISC-MEM major opcode (0x0F) with funct3 = 0b101. No assembler mnemonic
 *	exists for them, so they are emitted as raw 32-bit words:
 *
 *	    0x0000500f   rs1 = x0   -> invalidate the entire data cache
 *	    0x0005500f   rs1 = a0   -> invalidate the line addressed by a0
 *
 *	rs1 is baked into the encoding, so the line-invalidate operand MUST be in
 *	a0. That is why the address is bound to a0 with a local register variable
 *	rather than left to the register allocator.
 *
 *	Every one of these carries a "memory" clobber. It is not decoration: it is
 *	what stops the compiler from holding a stale copy of the invalidated data in
 *	a register across the operation, or from moving a load of that data to
 *	before it.
 */
#define kInvalidateAllInstruction	".word 0x0000500f"
#define kInvalidateLineInstruction	".word 0x0005500f"

bool
cacheIsCacheable(const void *  address)
{
	const uintptr_t	value = (uintptr_t) address;

	return (value >= kCacheableRangeLow) && (value < kCacheableRangeHigh);
}

void
cacheFlushInstruction(void)
{
	/*
	 *	fence.i lives in the Zifencei extension, which is not implied by the
	 *	rv32imfc this project builds with (it was split out of base I in the
	 *	2019 ISA revision, and GCC 16 / binutils 2.47 enforce that). The core
	 *	implements the instruction regardless, so enable the extension for this
	 *	one instruction instead of widening -march for the whole build.
	 */
	__asm__ __volatile__ (
		".option push\n"
		".option arch, +zifencei\n"
		"fence.i\n"
		".option pop"
		::: "memory");
}

void
cacheInvalidateLine(uintptr_t  address)
{
	register uintptr_t	a0 __asm__ ("a0") = address;

	__asm__ __volatile__ (kInvalidateLineInstruction : : "r" (a0) : "memory");
}

void
cacheInvalidateAll(void)
{
	__asm__ __volatile__ (kInvalidateAllInstruction ::: "memory");
}

void
cacheInvalidateRange(const void *  base, size_t  lengthBytes)
{
	uintptr_t	first;
	uintptr_t	last;
	uintptr_t	line;

	if (lengthBytes == 0)
	{
		return;
	}

	/*
	 *	Invalidating more lines than the cache holds costs more than dropping the
	 *	whole thing, and achieves exactly the same result.
	 */
	if (lengthBytes >= kCacheSizeBytes)
	{
		cacheInvalidateAll();

		return;
	}

	/*
	 *	Align the start down to a line boundary and compute the last byte
	 *	(inclusive) so the loop cannot wrap at the top of the address space.
	 */
	first = (uintptr_t) base & ~((uintptr_t) kCacheLineSizeBytes - 1u);
	last  = (uintptr_t) base + (lengthBytes - 1u);

	for (line = first; ; line += kCacheLineSizeBytes)
	{
		cacheInvalidateLine(line);

		if ((last - line) < kCacheLineSizeBytes)
		{
			break;
		}
	}
}
