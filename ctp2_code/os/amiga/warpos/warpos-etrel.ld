/* The layout of the WarpOS link: a relocatable ELF (ld -r) for the converter
 * to a WarpOS hunk executable (Makefile.warpos). Every section at 0, _start at
 * the start of .text (startupwos.o is linked first). Built without exceptions:
 * the unwind and exception tables go, as do the notes the converter cannot
 * place. */
ENTRY(_start)

SECTIONS
{
  .text 0 : { _start = .; *(.text .text.* .gnu.linkonce.t.*) }
  .init 0 : { *(.init .init.*) }
  .fini 0 : { *(.fini .fini.*) }
  .rodata 0 : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .ctors 0 : { __ctrslist = .; *(.ctors .ctors.*) }
  /* amiga_malloc.o's destructor frees the heap: after every other one, before
   * end.o's terminating 0. */
  .dtors 0 : { __dtrslist = .;
               *(EXCLUDE_FILE(*amiga_malloc.o *end.o) .dtors
                 EXCLUDE_FILE(*amiga_malloc.o *end.o) .dtors.*)
               *amiga_malloc.o(.dtors .dtors.*)
               *end.o(.dtors .dtors.*) }
  .data 0 : { *(.data .data.* .gnu.linkonce.d.*) }
  .sdata 0 : { *(.sdata .sdata.* .gnu.linkonce.s.*) }
  .sbss 0 (NOLOAD) : { *(.sbss .sbss.* .gnu.linkonce.sb.*) *(.scommon) }
  .bss 0 (NOLOAD) : { *(.bss .bss.* .gnu.linkonce.b.*) *(COMMON) }
  /DISCARD/ : {
    *(.eh_frame .eh_frame.*)
    *(.gcc_except_table .gcc_except_table.*)
    *(.comment .comment.*)
    *(.gnu.attributes)
    *(.note.GNU-stack)
  }
}
