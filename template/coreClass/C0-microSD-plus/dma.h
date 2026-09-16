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
#ifndef DMA_H
#define DMA_H

/*
 *	DMA driver. dma_memops.c's memcpy/memset overrides are built on
 *	this.
 *
 *	  - Copy needs both operands 4-byte aligned. A Set needs only the
 *	    destination. An unaligned operand parks in Error having moved nothing.
 *	  - Bursts are 512 B on this SoC. The engine chains them itself, so there
 *	    is never a reason to chunk in software.
 *	  - Programming LENGTH 0 or LENGTH > 0xFFFFFE00 at the register is a
 *	    ~4 GiB RUNAWAY with no abort, because the engine's burst count reaches
 *	    zero at both ends, once at length 0 and once by 32-bit overflow just
 *	    below 2^32. The two are opposite REQUESTS though,
 *	    so dmaTryRun answers them differently. Zero bytes is legal with
 *	    nothing to do, and returns kDmaDone without touching the engine. The
 *	    high band is an underflowed `end - start`, so it is declined and
 *	    counted.
 *	  - CONTROL is sampled ONLY in Idle. A write in Done is dropped, and the
 *	    caller then reads the stale Done as success, which is silent corruption.
 *	  - The engine never reaches Idle by itself: Done clears on ack.irq (bit
 *	    8), Error on ack.error (bit 0), 0xFFFFFFFF covers both.
 *	  - A CORE RESET DOES NOT RESET THE DMA. It answers the SoC-level reset,
 *	    not the core reset that config.rstn triggers, so its state survives a
 *	    trap or a host core-stop.
 *	  - The core is the only agent that can start or ack one (not SD-visible),
 *	    so waiting on an operation you did not start is a deadlock, and acking
 *	    it destroys the only record of that fault anywhere.
 *	  - ByteSet takes any length and replicates set[7:0] across the lanes.
 *	    WordSet rejects length % 4 and writes set[31:0]. Same cost.
 *	  - A Copy of 4k+r bytes reads up to 3 bytes PAST the source end, which
 *	    DECERRs if the source ends at the top of a mapped region.
 *	  - The SPI flash controller IGNORES byte strobes, so every write that
 *	    reaches it programs a full 32-bit word. A sub-word write to flash
 *	    therefore also writes its neighbouring bytes, whoever issues it and
 *	    whether by CPU store or by the engine's partial final beat. Only a
 *	    word-aligned, word-multiple flash write has a footprint equal to the
 *	    range asked for.
 *	  - The cache is non-coherent but write-through: never flush before,
 *	    always invalidate a cacheable destination after.
 *	  - write_error means memory is in an unknown state, so kDmaError is not a
 *	    fallback condition.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	kDMAStateIdle = 0,
	kDMAStateBusy = 1,
	kDMAStateDone = 2,
	kDMAStateError = 3
} DMAState;

typedef enum {
	kDMAOpInvalid = 0,
	kDMAOpCopy = 1,
	kDMAOpWordSet = 2,
	kDMAOpByteSet = 3
} DMAOpCode;

/*
 *	Properties of this instantiation, not of the IP.
 */
enum
{
	kDmaAlignBytes		= 4u,
	kDmaMaxBurstBytes	= 512u,
};

/*
 *	Three outcomes, not four: "declined on policy" and "engine busy" get the
 *	same response from every caller (do it on the CPU), so they are one case.
 *	kDmaError is the one that differs in kind, and it is not a fallback.
 */
typedef enum
{
	kDmaDone	= 0,	/*	the engine performed the WHOLE operation	*/
	kDmaNotRun	= 1,	/*	nothing was done, the caller must do it		*/
	kDmaError	= 2,	/*	accepted then FAILED: destination UNDEFINED	*/
} DmaResult;

/*
 *	One operation, synchronously, only if the engine is free. Unused arguments
 *	are ignored per opcode. kDmaDone also means a cacheable destination has
 *	been invalidated, and a zero length reports it too, having moved nothing.
 *	kDmaNotRun means nothing was touched. kDmaError means the destination is
 *	UNDEFINED and must not be redone on the CPU.
 *
 *	Never blocks on, and never acks, an operation it did not start. It DOES
 *	wait unbounded for its own. Not reentrant, and not for a trap handler.
 */
DmaResult	dmaTryRun(
			DMAOpCode	op,
			uint32_t	source,
			uint32_t	destination,
			uint32_t	lengthBytes,
			uint32_t	setValue);

/*
 *	Side-effect free, so safe even on an engine somebody else parked.
 */
uint32_t	dmaStatusRaw(void);
bool		dmaIsIdle(void);

/*
 *	Plain RAM counters, so the SD host can read them. Without these a fallback
 *	is invisible and a test cannot tell one from a wrapper that never ran.
 */
uint32_t	dmaRunCount(void);
uint32_t	dmaNotRunCount(void);
uint32_t	dmaErrorCount(void);
void		dmaResetCounters(void);

/*
 *	Called by the wrappers when a DMA memcpy/memset fails mid-transfer. Weakly
 *	defined in dma_memops.c to spin: memcpy has no error channel, continuing
 *	would run on corrupt data, and the reachable causes are program bugs.
 *	Define your own to override.
 */
void		dmaMemopsOnError(void *  destination, size_t  lengthBytes);

#endif				/* DMA_H */
