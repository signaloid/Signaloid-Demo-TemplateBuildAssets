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
#ifndef CACHE_H
#define CACHE_H

/*
 *	The cache geometry, the cacheable range, the write-through data-cache policy
 *	and the set of maintenance operations are as documented for the RISC-V RX
 *	core in Lattice FPGA-IPUG-02302 sec. 2.2.1.8.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 *	Cache geometry (FPGA-IPUG-02302 sec. 2.2.1.8). Both caches share it.
 */
enum
{
	kCacheLineSizeBytes	= 32u,
	kCacheSizeBytes		= 4096u,
	kCacheLineCount		= kCacheSizeBytes / kCacheLineSizeBytes,	/* 128 */
};

/*
 *	Cacheable range: [kCacheableRangeLow, kCacheableRangeHigh). The low limit is
 *	fixed at 0 by the core; the high limit is the CSR base, so everything at or
 *	above the MMIO block stays uncached.
 */
#define kCacheableRangeLow	((uintptr_t) 0x00000000u)
#define kCacheableRangeHigh	((uintptr_t) (0x10000000))

/*
 *	True if accesses to this address go through the data cache, i.e. if cache
 *	maintenance is meaningful for it at all.
 */
bool	cacheIsCacheable(const void *  address);

/*
 *	Synchronize writes to instruction memory with instruction fetches on the
 *	same hart, by flushing the whole instruction cache (fence.i, RISC-V
 *	Zifencei).
 */
void	cacheFlushInstruction(void);

/*
 *	Invalidate the single 32-byte data-cache line containing `address`. The core
 *	aligns down to the line boundary, so invalidating 0x10 invalidates
 *	[0x00, 0x20), not [0x10, 0x30).
 */
void	cacheInvalidateLine(uintptr_t address);

/*
 *	Invalidate the entire data cache.
 */
void	cacheInvalidateAll(void);

/*
 *	Invalidate every data-cache line overlapping [base, base + lengthBytes).
 *	Handles the unaligned head and tail. A zero length does nothing.
 *
 *	The whole data cache is only kCacheLineCount lines, so for a span at least
 *	kCacheSizeBytes wide this falls back to cacheInvalidateAll(), which is both
 *	faster and has the same effect.
 */
void	cacheInvalidateRange(const void *  base, size_t  lengthBytes);

#endif
