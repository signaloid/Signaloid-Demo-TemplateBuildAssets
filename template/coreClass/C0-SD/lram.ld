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
 *	Copyright (C) 2014-2024 Free Software Foundation, Inc.
 *
 *	Copying and distribution of this file, with or without modification,
 *	are permitted in any medium without royalty provided the copyright
 *	notice and this notice are preserved.
 */

ENTRY(_init)

MEMORY
{
	FLASH (rx)  : ORIGIN = 0x200000, LENGTH = ((0x3fffffc + 4) - 0x200000)
	LRAM  (rwx) : ORIGIN = 0x6000000, LENGTH = (0x606fffc - 0x6000000 + 4)
}

SECTIONS
{

	.boot :
	{
		KEEP(*(.boot))
		. = ALIGN(16);
	} > FLASH

	.text :
	{

		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.sorted.*))
		*(.text .stub .text.* .gnu.linkonce.t.*)
		*(.gnu.warning)
		. = ALIGN(4);
		PROVIDE (__etext = .);
		PROVIDE (_etext = .);
		PROVIDE (etext = .);

		*(.rodata .rodata.* .gnu.linkonce.r.*)

		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
		KEEP (*(.eh_frame)) *(.eh_frame.*)
		*(.gcc_except_table .gcc_except_table.*)
		*(.gnu_extab*)

		PROVIDE_HIDDEN (__tdata_start = .);
		*(.tdata .tdata.* .gnu.linkonce.td.*)

		. = ALIGN(4);
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
		PROVIDE_HIDDEN (__init_array_end = .);
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
		KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
		PROVIDE_HIDDEN (__fini_array_end = .);
		KEEP (*crtbegin.o(.ctors))
		KEEP (*crtbegin?.o(.ctors))
		KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.ctors))
		KEEP (*crtbegin.o(.dtors))
		KEEP (*crtbegin?.o(.dtors))
		KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		KEEP (*(.jcr))

		*(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
		*(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)

		. = ALIGN(16);
		__DATA_BEGIN__ = .;
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		SORT(CONSTRUCTORS)

		*(.got.plt) *(.igot.plt)
		*(.got) *(.igot)

		. = ALIGN(16);
		__SDATA_BEGIN__ = .;
		*(.srodata.cst16) *(.srodata.cst8)
		*(.srodata.cst4) *(.srodata.cst2)
		*(.srodata .srodata.*)
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		. = ALIGN(16);
	} > LRAM AT > FLASH

	__copy_vma_start = ADDR(.text);
	__copy_vma_end   = .;
	__copy_lma       = LOADADDR(.text);

	_edata = .;
	PROVIDE (edata = .);

	. = ALIGN(16);
	__bss_start = .;
	.tbss (NOLOAD) :
	{
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
	} > LRAM
	.sbss (NOLOAD) :
	{
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
	} > LRAM
	.bss (NOLOAD) :
	{
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(4);
	} > LRAM
	__bss_end = .;
	_end = .;
	PROVIDE (end = .);

	__global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800,
		MAX(__DATA_BEGIN__ + 0x800, __bss_end - 0x800));

	__stack_top = 0x6000000 + (0x606fffc - 0x6000000 + 4);
	PROVIDE (_heap_end    = __stack_top - 8192);
	PROVIDE (__heap_end   = _heap_end);
	PROVIDE (__heap_limit = _heap_end);

	ASSERT(__bss_end + 8192 <= __stack_top,
		"lram overflow: image + .bss plus the stack reservation exceed LRAM")
}
