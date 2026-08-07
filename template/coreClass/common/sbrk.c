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
 *	Bounded _sbrk() for the C0 compute modules SoC.
 *
 *	Overrides the libgloss/-lnosys stub, which grows the heap without an
 *	upper bound. The heap grows up from _end (end of .bss) towards _heap_end,
 *	which the linker script places just below the reserved stack at the top
 *	of LRAM. Once a request would cross _heap_end, _sbrk() fails so that
 *	malloc() returns NULL and mallinfo() reports correct totals instead of
 *	handing out memory that does not physically exist.
 */

#include <stddef.h>

extern char _end[];		/* Heap base, defined by the linker script.  */
extern char _heap_end[];	/* Heap ceiling, defined by the linker script. */

void *
_sbrk(ptrdiff_t incr)
{
	static char	*cur_brk = _end;
	char		*prev_brk;

	if ((cur_brk + incr > _heap_end) || (cur_brk + incr < _end)) {
		return NULL;
	}

	prev_brk = cur_brk;
	cur_brk += incr;

	return prev_brk;
}
