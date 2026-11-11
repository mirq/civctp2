/* No backtraces on AmigaOS: the crash handler (ctp/civ3_main.cpp) prints the
 * signal and leaves the stack to the debug link (addr2line). */
#ifndef CTP2_AMIGA_EXECINFO_H
#define CTP2_AMIGA_EXECINFO_H 1

static inline int backtrace(void **buffer, int size)
{
	(void) buffer; (void) size;
	return 0;
}

static inline void backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
	(void) buffer; (void) size; (void) fd;
}

#endif
