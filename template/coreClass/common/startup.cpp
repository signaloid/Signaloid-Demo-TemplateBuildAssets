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
 *
 *	Portions of this file derive from a C++ startup routine published by
 *	five-embeddev (https://five-embeddev.com/) and dedicated to the public
 *	domain under the Unlicense (SPDX-License-Identifier: Unlicense). That
 *	dedication imposes no attribution requirement; the provenance is recorded
 *	here for completeness.
 */

#include <algorithm>
#include <cstdint>

/*
 *	Generic C function pointer.
 */
typedef void (*function_t)();

extern "C" function_t __init_array_start[];
extern "C" function_t __init_array_end[];

/*
 *	Define the symbols with "C" naming as they are used by the assembler
 */
extern "C" void _start(void);

/*
 *	Define the following to avoid compilation issues
 */
extern "C" { void* __dso_handle __attribute__ ((__weak__)); }

/*
 *	Standard entry point, no arguments.
 */
extern int main(void);

/*
 *	At this point we have a stack and global pointer, but no access to global variables.
 */
void
_start(void)
{
	/*
	 *	Call constructors
	 */
	std::for_each( __init_array_start,
			__init_array_end,
			[](const function_t pf) {pf();});
}
