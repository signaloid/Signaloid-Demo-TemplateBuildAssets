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
 *	memcpy and memset backed by the DMA, on top of dma.c. Reached through
 *	`-Wl,--wrap`. Below the threshold, and for the bytes either side of the
 *	engine's range, the work is done here on the CPU with word accesses
 *	(dmaMemopsCpuFill(), dmaMemopsCpuCopy()). __real_memcpy, the C library's
 *	own, is left only the copies whose operands are incongruent mod 4.
 *	Enabled by DMA_MEMOPS=1 at build time.
 *
 *	Two limitations: a large struct assignment (`*a = *b`) is expanded inline
 *	and never reaches these at all, and memmove is not wrapped.
 */

#include "dma.h"

/*
 *	libc's own, via --wrap. Never call the unprefixed names from here.
 */
extern void *	__real_memcpy(void *  destination, const void *  source, size_t  n);

/*
 *	No header declares these: dma.h stays free of the wrapper's ABI.
 */
void *	__wrap_memcpy(void *  destination, const void *  source, size_t  n);
void *	__wrap_memset(void *  s, int  c, size_t  n);

/*
 *	Below this the engine is not even consulted, because its setup cost
 *	dominates. Passed as a -D so it can be swept without editing sources.
 */
#ifndef DMA_MEMOPS_THRESHOLD
#define DMA_MEMOPS_THRESHOLD	128
#endif

/*
 *	Below kDmaAlignBytes the engine is unreachable at any alignment, so the
 *	knob would be a lie. Literal 4: an enum constant cannot appear in #if.
 */
#if DMA_MEMOPS_THRESHOLD < 4
#error "DMA_MEMOPS_THRESHOLD must be at least kDmaAlignBytes (4): below that the DMA engine cannot be reached at all."
#endif

/*
 *	The low address bits that must be clear for a word access.
 */
enum
{
	kDmaMemopsAlignMask	= kDmaAlignBytes - 1u,
};

/*
 *	The word type of the CPU fill/copy loops. may_alias: the buffers hold
 *	whatever type the caller's are, so these accesses must not be assumed
 *	(type-based alias analysis, e.g. under LTO) to leave them untouched.
 */
typedef uint32_t	DmaMemopsWord __attribute__((may_alias));

/*
 *	Weak default for dma.h's mid-transfer-failure hook.
 */
__attribute__((weak))
void
dmaMemopsOnError(void *  destination, size_t  lengthBytes)
{
	/*
	 *	Both parameters exist for an override that wants to report which range
	 *	was left in an unknown state. This default cannot.
	 */
	(void) destination;
	(void) lengthBytes;

	for (;;)
	{
		/*
		 *	"memory" clobber only, so no instruction is emitted.
		 */
		__asm__ __volatile__ ("" ::: "memory");
	}
}

/*
 *	The CPU side of memset, in place of the C library's: a size-optimised newlib
 *	(and the co-simulation's stub) fills a byte at a time, and through the
 *	write-through data cache every byte store is its own bus write. Small
 *	memsets are the common case, so this is the path that matters.
 *
 *	These loops, and dmaMemopsCpuCopy()'s, are exactly the idiom a compiler
 *	turns back into a memset / memcpy call, which --wrap would route straight
 *	back here. MEMOPS_CFLAGS (app.mk) is what stops that; never build this file
 *	without it.
 */
static inline void
dmaMemopsCpuFill(uintptr_t  address, uint8_t  value, size_t  n)
{
	uint32_t	word;
	DmaMemopsWord *	words;

	/*
	 *	Bytes up to the first word boundary: -mstrict-align.
	 */
	while ((n != 0) && ((address & kDmaMemopsAlignMask) != 0))
	{
		*(uint8_t *) address = value;
		address++;
		n--;
	}

	word	= (uint32_t) value * 0x01010101u;
	words	= (DmaMemopsWord *) address;

	/*
	 *	Unrolled by hand, so the default -O needs no help from the optimiser.
	 */
	for (; n >= 16u; n -= 16u)
	{
		words[0] = word;
		words[1] = word;
		words[2] = word;
		words[3] = word;
		words += 4;
	}

	for (; n >= 4u; n -= 4u)
	{
		*words++ = word;
	}

	address = (uintptr_t) words;
	for (; n != 0; n--)
	{
		*(uint8_t *) address = value;
		address++;
	}
}

/*
 *	The CPU side of memcpy, for operands CONGRUENT mod 4 only: one head loop
 *	then aligns both, and every access after it is a whole word. The caller
 *	sends incongruent copies to __real_memcpy instead.
 */
static inline void
dmaMemopsCpuCopy(uintptr_t  destination, uintptr_t  source, size_t  n)
{
	DmaMemopsWord *		destinationWords;
	const DmaMemopsWord *	sourceWords;

	/*
	 *	Bytes up to the first word boundary, which is both operands' boundary.
	 */
	while ((n != 0) && ((destination & kDmaMemopsAlignMask) != 0))
	{
		*(uint8_t *) destination = *(const uint8_t *) source;
		destination++;
		source++;
		n--;
	}

	destinationWords	= (DmaMemopsWord *) destination;
	sourceWords		= (const DmaMemopsWord *) source;

	/*
	 *	Unrolled by hand, as dmaMemopsCpuFill() is. All four loads before any
	 *	store: memcpy operands never overlap, so the order is free.
	 */
	for (; n >= 16u; n -= 16u)
	{
		uint32_t	word0 = sourceWords[0];
		uint32_t	word1 = sourceWords[1];
		uint32_t	word2 = sourceWords[2];
		uint32_t	word3 = sourceWords[3];

		destinationWords[0] = word0;
		destinationWords[1] = word1;
		destinationWords[2] = word2;
		destinationWords[3] = word3;
		destinationWords += 4;
		sourceWords += 4;
	}

	for (; n >= 4u; n -= 4u)
	{
		*destinationWords++ = *sourceWords++;
	}

	destination	= (uintptr_t) destinationWords;
	source		= (uintptr_t) sourceWords;
	for (; n != 0; n--)
	{
		*(uint8_t *) destination = *(const uint8_t *) source;
		destination++;
		source++;
	}
}

/*
 *	The engine paths, kept out of line on purpose. Inlined, their registers are
 *	saved in the wrappers' prologues, which every small call then pays for;
 *	out of line, the small paths need no stack frame at all.
 *
 *	That only holds if each wrapper's `return dmaMemops*Large(...)` compiles to
 *	a plain jump (a tail call), so the wrapper never needs a frame of its own.
 *	Left alone, the compiler defeats that. These functions are static, so it
 *	can see that both always return their first argument unchanged, and it
 *	uses that: rather than take the return value from the callee, the wrapper
 *	keeps its own copy of the pointer in a callee-saved register across the
 *	call and returns that. Keeping a register across a call means saving it,
 *	and the return address, on the stack, and since a function has only one
 *	prologue, every small call paid for that frame too.
 *
 *	The `__asm__ ("" : "+r" (x))` before each return stops the deduction:
 *
 *	    ""		the instruction text is empty, so nothing is emitted.
 *	    "+r" (x)	declares that the asm reads x from a register and may
 *			write a new value back to it. The compiler cannot look
 *			inside an asm, so it must assume x was changed.
 *
 *	After it, the compiler no longer knows the returned value equals the
 *	argument, so the wrappers have to use the callee's return value, and
 *	`return dmaMemops*Large(...)` becomes a jump again. The value really is
 *	unchanged; only the compiler's knowledge of it is removed. Check the
 *	disassembly after touching this: neither wrapper should store ra.
 */
__attribute__((noinline))
static void *
dmaMemopsSetLarge(void *  s, int  c, size_t  n)
{
	uintptr_t	address = (uintptr_t) s;
	size_t		prefixBytes;
	size_t		restBytes;
	DmaResult	result;

	/*
	 *	Head bytes up to the alignment boundary, on the CPU.
	 */
	prefixBytes = (size_t) ((-address) & kDmaMemopsAlignMask);
	dmaMemopsCpuFill(address, (uint8_t) c, prefixBytes);

	restBytes = n - prefixBytes;

	/*
	 *	ByteSet, never WordSet: the only set opcode with no length rule.
	 */
	result = dmaTryRun(
			kDMAOpByteSet,
			0,
			(uint32_t) (address + prefixBytes),
			(uint32_t) restBytes,
			(uint32_t) (unsigned char) c);

	switch (result)
	{
		case kDmaDone:
			/*
			 *	Engine filled the remainder, and dmaTryRun() has already
			 *	invalidated it where that is meaningful.
			 */
			break;

		case kDmaNotRun:
			/*
			 *	Not Idle, or an operand declined: nothing was touched,
			 *	so do it here.
			 */
			dmaMemopsCpuFill(address + prefixBytes, (uint8_t) c, restBytes);
			break;

		case kDmaError:
			/*
			 *	UNDEFINED destination, not a fallback condition.
			 */
			dmaMemopsOnError((void *) (address + prefixBytes), restBytes);
			break;
	}

	/*
	 *	Same value the C library returns, on every path. The empty asm emits
	 *	nothing; it hides that this is the argument, so __wrap_memset can
	 *	tail-call here (see above).
	 */
	__asm__ ("" : "+r" (s));

	return s;
}

__attribute__((noinline))
static void *
dmaMemopsCopyLarge(void *  destination, const void *  source, size_t  n)
{
	uintptr_t	destinationAddress = (uintptr_t) destination;
	uintptr_t	sourceAddress = (uintptr_t) source;
	size_t		prefixBytes;
	size_t		restBytes;
	size_t		dmaBytes;
	size_t		tailBytes;
	DmaResult	result;

	/*
	 *	The caller checked congruence, so this single prefix aligns both
	 *	pointers.
	 */
	prefixBytes = (size_t) ((-destinationAddress) & kDmaMemopsAlignMask);
	dmaMemopsCpuCopy(destinationAddress, sourceAddress, prefixBytes);

	restBytes = n - prefixBytes;

	/*
	 *	A WHOLE NUMBER OF WORDS to the engine, odd bytes on the CPU: a Copy of
	 *	4k+r reads up to 3 bytes past the source end (dma.h), which DECERRs if
	 *	the source ends at the top of a mapped region, turning a legal memcpy
	 *	fatal.
	 */
	dmaBytes = restBytes & ~((size_t) kDmaMemopsAlignMask);
	tailBytes = restBytes - dmaBytes;

	/*
	 *	Reachable at the smallest permitted threshold, so guard it.
	 */
	if (dmaBytes == 0)
	{
		dmaMemopsCpuCopy(
			destinationAddress + prefixBytes,
			sourceAddress + prefixBytes,
			restBytes);

		/*
		 *	Emits nothing: hides that this is the argument, so
		 *	__wrap_memcpy can tail-call here (see above).
		 */
		__asm__ ("" : "+r" (destination));

		return destination;
	}

	result = dmaTryRun(
			kDMAOpCopy,
			(uint32_t) (sourceAddress + prefixBytes),
			(uint32_t) (destinationAddress + prefixBytes),
			(uint32_t) dmaBytes,
			0);

	switch (result)
	{
		case kDmaDone:
			if (tailBytes != 0)
			{
				/*
				 *	The sub-word tail, on the CPU. dmaTryRun already
				 *	invalidated the range it owned, and these bytes
				 *	were never in it.
				 */
				dmaMemopsCpuCopy(
					destinationAddress + prefixBytes + dmaBytes,
					sourceAddress + prefixBytes + dmaBytes,
					tailBytes);
			}
			break;

		case kDmaNotRun:
			/*
			 *	Nothing moved and nothing acked, so the whole copy is
			 *	ours.
			 */
			dmaMemopsCpuCopy(
				destinationAddress + prefixBytes,
				sourceAddress + prefixBytes,
				restBytes);
			break;

		case kDmaError:
			/*
			 *	UNDEFINED across the range the engine owned.
			 */
			dmaMemopsOnError((void *) (destinationAddress + prefixBytes), dmaBytes);
			break;
	}

	/*
	 *	As on the early return above.
	 */
	__asm__ ("" : "+r" (destination));

	return destination;
}

void *
__wrap_memset(void *  s, int  c, size_t  n)
{
	/*
	 *	The one comparison the whole hot path is optimised around: even a
	 *	16-byte constant-size call reaches this prologue, so nothing more
	 *	expensive may precede it. n == 0 needs no test of its own: the CPU
	 *	fill does nothing with it, and the engine, for which it is a runaway
	 *	(dma.h), is only reached at the threshold or above.
	 */
	if (n < (size_t) DMA_MEMOPS_THRESHOLD)
	{
		dmaMemopsCpuFill((uintptr_t) s, (uint8_t) c, n);

		return s;
	}

	return dmaMemopsSetLarge(s, c, n);
}

void *
__wrap_memcpy(void *  destination, const void *  source, size_t  n)
{
	/*
	 *	Incongruent mod 4: no head loop can align both operands, for the CPU's
	 *	word loop or the engine, so the C library takes it at any length.
	 *	n == 0 needs no test of its own, as in __wrap_memset.
	 */
	if ((((uintptr_t) destination ^ (uintptr_t) source) & kDmaMemopsAlignMask) != 0)
	{
		return __real_memcpy(destination, source, n);
	}

	if (n < (size_t) DMA_MEMOPS_THRESHOLD)
	{
		dmaMemopsCpuCopy((uintptr_t) destination, (uintptr_t) source, n);

		return destination;
	}

	return dmaMemopsCopyLarge(destination, source, n);
}
