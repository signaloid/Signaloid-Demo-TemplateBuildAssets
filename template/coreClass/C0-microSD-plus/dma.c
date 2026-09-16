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
 *	DMA driver. The hardware contract this obeys is in dma.h.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cache.h"
#include "dma.h"

/*
 *	state is bits [25:24]. The three error bits are 0, 8 and 16.
 */
enum
{
	kDmaStatusStateShift		= 24u,
	kDmaStatusStateMask		= 0x3u,
	kDmaStatusUnsupportedOp		= 1u << 0,
	kDmaStatusWriteError		= 1u << 8,
	kDmaStatusReadError		= 1u << 16,
	kDmaStatusErrorBits		= kDmaStatusUnsupportedOp |
					  kDmaStatusWriteError |
					  kDmaStatusReadError,
};

/*
 *	ACK: error at bit 0, irq at bit 8.
 */
#define kDmaAckAllStrobes		((uint32_t) 0xFFFFFFFFu)

/*
 *	Plain RAM, not volatile: only the core touches them (dma.h).
 */
static uint32_t	gDmaRunCount;
static uint32_t	gDmaNotRunCount;
static uint32_t	gDmaErrorCount;

static inline uint32_t
mmioRead32(uint32_t  address)
{
	return *(volatile uint32_t *) address;
}

static inline void
mmioWrite32(uint32_t  address, uint32_t  value)
{
	*(volatile uint32_t *) address = value;
}

/*
 *	The engine's reported state, STATUS[25:24].
 */
static inline DMAState
dmaStatusState(uint32_t  status)
{
	return (DMAState) ((status >> kDmaStatusStateShift) & kDmaStatusStateMask);
}

/*
 *	Mirrors cache.h's cacheIsCacheable() on a plain address.
 */
static inline bool
dmaAddressIsCacheable(uint32_t  address)
{
	return address < (uint32_t) (0x8200000);
}

static bool
dmaOperandsAreValid(
	DMAOpCode	op,
	uint32_t	source,
	uint32_t	destination,
	uint32_t	lengthBytes)
{
	/*
	 *	32-bit master: every opcode needs a 4-byte-aligned destination.
	 */
	if ((destination & (kDmaAlignBytes - 1u)) != 0)
	{
		return false;
	}

	switch (op)
	{
		case kDMAOpCopy:
			/*
			 *	The read side has no byte strobes, so a Copy needs its
			 *	source aligned too. The Set opcodes never read.
			 */
			return (source & (kDmaAlignBytes - 1u)) == 0;

		case kDMAOpByteSet:
			/*
			 *	No length rule whatsoever.
			 */
			return true;

		case kDMAOpWordSet:
			/*
			 *	WordSet also needs a length that is a multiple of 4.
			 */
			return (lengthBytes & (kDmaAlignBytes - 1u)) == 0;

		case kDMAOpInvalid:
		default:
			/*
			 *	kDMAOpInvalid included: the FSM parks in Error on an
			 *	unknown opcode.
			 */
			return false;
	}
}

DmaResult
dmaTryRun(
	DMAOpCode	op,
	uint32_t	source,
	uint32_t	destination,
	uint32_t	lengthBytes,
	uint32_t	setValue)
{
	uint32_t	status;
	uint32_t	errorBits;
	DMAState	state;
	bool		completed;

	/*
	 *	Zero bytes is a legal request, but programming LENGTH = 0 at the
	 *	register is a runaway (dma.h), so the engine is never told. kDmaDone
	 *	rather than a decline: zero bytes moved is a completed operation.
	 */
	if (lengthBytes == 0)
	{
		return kDmaDone;
	}

	/*
	 *	The same runaway from the other end. The engine rounds the length up to
	 *	a whole number of bursts, and in 32 bits that rounding overflows for
	 *	anything within one burst of 2^32, giving zero transfers exactly as
	 *	length 0 does. That band is an underflowed `end - start`.
	 *	A merely over-large length walks up the address space, DECERRs and
	 *	diverts to Error, but this one is unabortable. Declined rather than
	 *	programmed: a caller with a nonsense length hits its own bug on the
	 *	CPU instead of wedging a module only a power cycle recovers.
	 */
	if (lengthBytes > (0xFFFFFFFFu - (kDmaMaxBurstBytes - 1u)))
	{
		gDmaNotRunCount++;

		return kDmaNotRun;
	}

	/*
	 *	Validated by arithmetic, before the bus is touched at all.
	 */
	if (!dmaOperandsAreValid(op, source, destination, lengthBytes))
	{
		gDmaNotRunCount++;

		return kDmaNotRun;
	}

	/*
	 *	Claim only if free. On failure leave the engine EXACTLY as found,
	 *	with no ack. Its Error latch may be the only record of that fault
	 *	(dma.h).
	 */
	status = mmioRead32((0x10000008));
	if (dmaStatusState(status) != kDMAStateIdle)
	{
		gDmaNotRunCount++;

		return kDmaNotRun;
	}

	/*
	 *	The registers are volatile, the caller's buffer is not: order the
	 *	stores.
	 */
	__asm__ __volatile__ ("" ::: "memory");

	/*
	 *	LENGTH first, CONTROL last: the burst plan is recomputed off LENGTH.
	 */
	mmioWrite32((0x10000010), lengthBytes);

	if (op == kDMAOpCopy)
	{
		mmioWrite32((0x1000000c), source);
	}
	else
	{
		/*
		 *	Unsupported opcodes are already gone, so no default arm is
		 *	reachable.
		 */
		mmioWrite32((0x1000001c), setValue);
	}

	mmioWrite32((0x10000014), destination);
	mmioWrite32((0x10000018), (uint32_t) op);

	/*
	 *	Unbounded, and legitimate only here: our own operation, and no abort.
	 */
	do
	{
		status	= mmioRead32((0x10000008));
		state	= dmaStatusState(status);
	} while ((state == kDMAStateBusy) || (state == kDMAStateIdle));

	/*
	 *	Done is necessary but not sufficient: the error bits are sticky latches
	 *	that only an Error ack clears (a Done ack clears just irq and the
	 *	state), so a bit standing here means data that cannot be trusted.
	 */
	errorBits	= status & kDmaStatusErrorBits;
	completed	= (state == kDMAStateDone) && (errorBits == 0);

	/*
	 *	Release the engine. The ack, not CONTROL, is what returns it to Idle.
	 */
	mmioWrite32((0x10000018), (uint32_t) kDMAOpInvalid);
	mmioWrite32((0x10000004), kDmaAckAllStrobes);
	mmioWrite32((0x10000004), 0u);

	do
	{
		status = mmioRead32((0x10000008));
	} while (dmaStatusState(status) != kDMAStateIdle);

	/*
	 *	Second barrier: orders the caller's loads from the destination after the
	 *	transfer, even where no invalidate runs (an uncached destination).
	 */
	__asm__ __volatile__ ("" ::: "memory");

	/*
	 *	The cache is not coherent with the DMA, so a cacheable destination must
	 *	be invalidated. Safe unconditionally: write-through, no dirty state.
	 */
	if (dmaAddressIsCacheable(destination))
	{
		cacheInvalidateRange((const void *) destination, lengthBytes);
	}

	/*
	 *	Not a fallback (dma.h): the destination is in an unknown state.
	 */
	if (!completed)
	{
		gDmaErrorCount++;

		return kDmaError;
	}

	gDmaRunCount++;

	return kDmaDone;
}

uint32_t
dmaStatusRaw(void)
{
	/*
	 *	No side effects: no clear-on-read anywhere in STATUS.
	 */
	return mmioRead32((0x10000008));
}

bool
dmaIsIdle(void)
{
	return dmaStatusState(mmioRead32((0x10000008))) == kDMAStateIdle;
}

uint32_t
dmaRunCount(void)
{
	return gDmaRunCount;
}

uint32_t
dmaNotRunCount(void)
{
	return gDmaNotRunCount;
}

uint32_t
dmaErrorCount(void)
{
	return gDmaErrorCount;
}

void
dmaResetCounters(void)
{
	gDmaRunCount	= 0;
	gDmaNotRunCount	= 0;
	gDmaErrorCount	= 0;
}
