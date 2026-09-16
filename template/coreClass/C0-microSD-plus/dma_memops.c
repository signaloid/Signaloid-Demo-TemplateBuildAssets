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
 *	`-Wl,--wrap`, so __real_* is the C library's own implementation, used for
 *	everything below the threshold. Enabled by DMA_MEMOPS=1 at build time.
 *
 *	Two limitations: a large struct assignment (`*a = *b`) is expanded inline
 *	and never reaches these at all, and memmove is not wrapped.
 */

#include "dma.h"

/*
 *	libc's own, via --wrap. Never call the unprefixed names from here.
 */
extern void *	__real_memcpy(void *  destination, const void *  source, size_t  n);
extern void *	__real_memset(void *  s, int  c, size_t  n);

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

void *
__wrap_memset(void *  s, int  c, size_t  n)
{
	uintptr_t	address = (uintptr_t) s;
	size_t		prefixBytes;
	size_t		restBytes;
	DmaResult	result;

	/*
	 *	Legal C, and a runaway in the engine (dma.h).
	 */
	if (n == 0)
	{
		return s;
	}

	/*
	 *	The one comparison the whole hot path is optimised around: even a
	 *	16-byte constant-size call reaches this prologue, so nothing more
	 *	expensive may precede it.
	 */
	if (n < (size_t) DMA_MEMOPS_THRESHOLD)
	{
		return __real_memset(s, c, n);
	}

	/*
	 *	Head bytes up to the alignment boundary, on the CPU.
	 */
	prefixBytes = (size_t) ((-address) & ((uintptr_t) kDmaAlignBytes - 1u));
	if (prefixBytes != 0)
	{
		__real_memset(s, c, prefixBytes);
	}

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
			__real_memset((void *) (address + prefixBytes), c, restBytes);
			break;

		case kDmaError:
			/*
			 *	UNDEFINED destination, not a fallback condition.
			 */
			dmaMemopsOnError((void *) (address + prefixBytes), restBytes);
			break;
	}

	/*
	 *	Same value the C library returns, on every path.
	 */
	return s;
}

void *
__wrap_memcpy(void *  destination, const void *  source, size_t  n)
{
	uintptr_t	destinationAddress = (uintptr_t) destination;
	uintptr_t	sourceAddress = (uintptr_t) source;
	size_t		prefixBytes;
	size_t		restBytes;
	size_t		dmaBytes;
	size_t		tailBytes;
	DmaResult	result;

	/*
	 *	Cheapest test first: this prologue is on every small copy's hot path.
	 */
	if (n == 0)
	{
		return destination;
	}

	if (n < (size_t) DMA_MEMOPS_THRESHOLD)
	{
		return __real_memcpy(destination, source, n);
	}

	/*
	 *	Incongruent mod 4: no head loop can align both operands.
	 */
	if (((destinationAddress ^ sourceAddress) & ((uintptr_t) kDmaAlignBytes - 1u)) != 0)
	{
		return __real_memcpy(destination, source, n);
	}

	/*
	 *	Congruent by now, so this single prefix aligns both pointers.
	 */
	prefixBytes = (size_t) ((-destinationAddress) & ((uintptr_t) kDmaAlignBytes - 1u));
	if (prefixBytes != 0)
	{
		__real_memcpy(destination, source, prefixBytes);
	}

	restBytes = n - prefixBytes;

	/*
	 *	A WHOLE NUMBER OF WORDS to the engine, odd bytes on the CPU: a Copy of
	 *	4k+r reads up to 3 bytes past the source end (dma.h), which DECERRs if
	 *	the source ends at the top of a mapped region, turning a legal memcpy
	 *	fatal.
	 */
	dmaBytes = restBytes & ~((size_t) kDmaAlignBytes - 1u);
	tailBytes = restBytes - dmaBytes;

	/*
	 *	Reachable at the smallest permitted threshold, so guard it.
	 */
	if (dmaBytes == 0)
	{
		__real_memcpy(
			(void *) (destinationAddress + prefixBytes),
			(const void *) (sourceAddress + prefixBytes),
			restBytes);

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
				__real_memcpy(
					(void *) (destinationAddress + prefixBytes + dmaBytes),
					(const void *) (sourceAddress + prefixBytes + dmaBytes),
					tailBytes);
			}
			break;

		case kDmaNotRun:
			/*
			 *	Nothing moved and nothing acked, so the whole copy is
			 *	ours.
			 */
			__real_memcpy(
				(void *) (destinationAddress + prefixBytes),
				(const void *) (sourceAddress + prefixBytes),
				restBytes);
			break;

		case kDmaError:
			/*
			 *	UNDEFINED across the range the engine owned.
			 */
			dmaMemopsOnError((void *) (destinationAddress + prefixBytes), dmaBytes);
			break;
	}

	return destination;
}
